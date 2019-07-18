#include "Engine.h"
#include "Plugin.h"
#include "Components/VoxelBody.h"
#include <sstream>
#include <vector>
#include <thread>

Engine::Engine(IUnityGraphicsVulkan* unityVulkan)
{
	m_unityVulkan = unityVulkan;
	m_instance = m_unityVulkan->Instance();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = m_instance.physicalDevice;
	allocatorInfo.device = m_instance.device;
	vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

void Engine::RegisterComputeQueues(std::vector<VkQueue> queues, const uint32_t& queueFamily)
{
	if (m_queues != nullptr || m_workers != nullptr)
		return;

	m_computeQueueFamily = queueFamily;
	m_queueCount = static_cast<uint8_t>(queues.size());
	m_workerCount = GetWorkerCount();
	if (m_workerCount < m_queueCount)
	{
		m_queueCount = m_workerCount;
		LOG("Queue count is some how larger than worker count!");
	}

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = m_computeQueueFamily;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	vkCreateCommandPool(m_instance.device, &cmdPoolInfo, nullptr, &m_computeCmdPool);

	std::vector<VkCommandBuffer> queueCMDBs(m_queueCount);
	std::vector<VkCommandBuffer> workerCMDBs(m_workerCount);

	VkCommandBufferAllocateInfo queueCMDBInfo = {};
	queueCMDBInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	queueCMDBInfo.commandBufferCount = m_queueCount;
	queueCMDBInfo.commandPool = m_computeCmdPool;
	queueCMDBInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VK_CALL(vkAllocateCommandBuffers(m_instance.device, &queueCMDBInfo, queueCMDBs.data()));

	VkCommandBufferAllocateInfo workerCMDBInfo = {};
	workerCMDBInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	workerCMDBInfo.commandBufferCount = m_workerCount;
	workerCMDBInfo.commandPool = m_computeCmdPool;
	workerCMDBInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

	VK_CALL(vkAllocateCommandBuffers(m_instance.device, &workerCMDBInfo, workerCMDBs.data()));

	m_queues = new QueueResource[m_queueCount];
	m_workers = new WorkerResource[m_workerCount];
	uint8_t workerStart = 0;
	for (int i = 0; i < m_queueCount; i++)
	{
		QueueResource& qr = m_queues[i];
		qr.m_queue = queues[i];
		qr.m_CMDB = queueCMDBs[i];
		qr.m_workerStart = workerStart;
		qr.m_workerEnd = ((i + 1) * m_workerCount) / m_queueCount;
		workerStart = qr.m_workerEnd;
		for (int j = qr.m_workerStart; j < qr.m_workerEnd; j++)
		{
			WorkerResource& wr = m_workers[j];
			wr.m_queueIndex = i;
			wr.m_CMDB = workerCMDBs[j];
		}
	}
}

void Engine::SetSurfaceShaders(const std::vector<char>& vertex, const std::vector<char>& fragment)
{
	m_renderPipeline.m_vertexShader = vertex;
	m_renderPipeline.m_fragmentShader = fragment;
}

void Engine::SetComputeShaders(const std::vector<char>& surfaceAnalysis, const std::vector<char>& surfaceAssembly)
{
	m_surfaceAnalysisPipeline.m_shader = surfaceAnalysis;
	m_surfaceAssemblyPipeline.m_shader = surfaceAssembly;
}

void Engine::InitializeResources()
{
	m_loadingFrame.store(0);
	InitializeRenderPipeline();
	InitializeComputePipelines();
	InitializeStagingResources(3); //TODO: Replace constant with variable
	
	GarbageCollect(GC_FORCE_COMPLETE);
}

void Engine::ReleaseResources()
{
	ReleaseStagingResources();
	m_testMutex.lock();
	m_vertexBuffer.Release(this);
	m_indexBuffer.Release(this);
	m_testMutex.unlock();

	m_renderPipeline.Release(this);
	m_surfaceAnalysisPipeline.Release(this);
	m_surfaceAssemblyPipeline.Release(this);
	if (m_formDSetLayout)
		vkDestroyDescriptorSetLayout(m_instance.device, m_formDSetLayout, nullptr);

	GarbageCollect(GC_FORCE_COMPLETE);
	vmaDestroyAllocator(m_allocator);
	vkDestroyCommandPool(m_instance.device, m_computeCmdPool, nullptr);
}

void Engine::Draw(Camera* camera)
{
	UnityVulkanRecordingState recordingState;
	if (!m_unityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
		return;

	m_loadingFrame.store(recordingState.currentFrameNumber);
	m_dumpFrame.store(recordingState.safeFrameNumber - SAFE_DUMP_MARGIN);
	
	VkPipeline pipeline;
	VkPipelineLayout layout;
	m_renderPipeline.AllocateOnRenderPass(this, recordingState.renderPass);
	m_renderPipeline.GetVkPipeline(pipeline, layout);
	
	if (pipeline && layout)
	{
		m_testMutex.lock();
		VkBuffer vb = m_vertexBuffer.GetVk();
		VkBuffer ib = m_indexBuffer.GetVk();
		if (vb && ib)
		{
			glm::mat4x4 mvp = camera->m_VP_Matrix.load(std::memory_order_relaxed);
			const VkDeviceSize offset = 0;
			vkCmdPushConstants(recordingState.commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &mvp);
			vkCmdBindPipeline(recordingState.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdBindVertexBuffers(recordingState.commandBuffer, 0, 1, &vb, &offset);
			vkCmdBindIndexBuffer(recordingState.commandBuffer, ib, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(recordingState.commandBuffer, m_indexCount, 1, 0, 0, 0);
		}
		m_testMutex.unlock();
	}
	else
	{
		LOG("pipeline or layout is null");
	}
}

ComputePipeline* Engine::CreateFormPipeline(const std::vector<char>& shader)
{
	ComputePipeline* form = new ComputePipeline();
	form->m_shader = shader;
	form->m_descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(1);
	form->m_descriptorSetLayouts[0] = m_formDSetLayout;
	form->Allocate(this);
	return form;
}

void Engine::ComputeTest(ComputePipeline* form)
{
	uint16_t stageIndex = m_stagingResources->dequeue();
	if (stageIndex == END)
		return;
	ChunkStagingResources* stage = (*m_stagingResources)[stageIndex];
	const uint8_t workerIndex = 0;
	WorkerResource* wr = m_workers + workerIndex;
	QueueResource* qr = m_queues + wr->m_queueIndex;

	VkCommandBufferBeginInfo beginI = {};
	beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkCommandBufferInheritanceInfo inheretInfo = {};
	inheretInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	beginI.pInheritanceInfo = &inheretInfo;
	VK_CALL(vkBeginCommandBuffer(wr->m_CMDB, &beginI));

	VkImageMemoryBarrier colorB = {};
	std::vector<VkImageMemoryBarrier> imageMemBs(2);
	imageMemBs[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemBs[0].image = stage->m_colorMap.m_gpuHandle->m_image;
	imageMemBs[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemBs[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemBs[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBs[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemBs[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemBs[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemBs[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemBs[0].subresourceRange.baseArrayLayer = 0;
	imageMemBs[0].subresourceRange.baseMipLevel = 0;
	imageMemBs[0].subresourceRange.layerCount = 1;
	imageMemBs[0].subresourceRange.levelCount = 1;
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imageMemBs[0]);
	
	VkPipeline formPipeline;
	VkPipelineLayout formLayout;
	form->GetVkPipeline(formPipeline, formLayout);
	vkCmdBindDescriptorSets(wr->m_CMDB, VK_PIPELINE_BIND_POINT_COMPUTE, formLayout, 0, 1, &stage->m_formDSet, 0, nullptr);
	vkCmdBindPipeline(wr->m_CMDB, VK_PIPELINE_BIND_POINT_COMPUTE, formPipeline);
	uint32_t sd4 = 36 / 4;
	vkCmdDispatch(wr->m_CMDB, sd4, sd4, sd4);

	std::vector<VkBufferMemoryBarrier> bufferMemBs(2);
	bufferMemBs[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemBs[0].buffer = stage->m_attributes.m_gpuHandle->m_buffer;
	bufferMemBs[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferMemBs[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferMemBs[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufferMemBs[0].offset = 0;
	bufferMemBs[0].size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[0],
		0, nullptr);
	vkCmdFillBuffer(wr->m_CMDB, bufferMemBs[0].buffer, 0, VK_WHOLE_SIZE, 0);
	bufferMemBs[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufferMemBs[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[0],
		0, nullptr);

	bufferMemBs[1] = bufferMemBs[0];
	bufferMemBs[1].buffer = stage->m_cells.m_gpuHandle->m_buffer;
	bufferMemBs[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

	imageMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemBs[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBs[1] = imageMemBs[0];
	imageMemBs[1].image = stage->m_indexMap.m_gpuHandle->m_image;
	imageMemBs[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBs[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[1],
		2, imageMemBs.data());
	
	VkPipeline analysisPipeline;
	VkPipelineLayout analysisPipelineLayout;
	m_surfaceAnalysisPipeline.GetVkPipeline(analysisPipeline, analysisPipelineLayout);
	vkCmdBindDescriptorSets(wr->m_CMDB, VK_PIPELINE_BIND_POINT_COMPUTE, analysisPipelineLayout, 0, 1, &stage->m_analysisDSet, 0, nullptr);
	vkCmdBindPipeline(wr->m_CMDB, VK_PIPELINE_BIND_POINT_COMPUTE, analysisPipeline);
	SurfacePipelineConstants surfConsts = {};
	surfConsts.base = glm::uvec3(2, 2, 2);
	surfConsts.range = glm::uvec3(31, 31, 31);
	vkCmdPushConstants(wr->m_CMDB, analysisPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SurfacePipelineConstants), &surfConsts);
	glm::uvec3 dispRange((uint32_t)std::ceilf((surfConsts.range.x + 1) / 4.0f), (uint32_t)std::ceilf((surfConsts.range.y + 1) / 4.0f), (uint32_t)std::ceilf((surfConsts.range.z + 1) / 4.0f));
	vkCmdDispatch(wr->m_CMDB, dispRange.x, dispRange.y, dispRange.z);

	bufferMemBs[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBs[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemBs[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[1],
		1, &imageMemBs[1]);

	bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferMemBs[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	bufferMemBs[1].buffer = stage->m_attributesSB.m_gpuHandle->m_buffer;
	bufferMemBs[1].srcAccessMask = 0;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		2, bufferMemBs.data(),
		0, nullptr);
	VkBufferCopy attribCopy = {};
	attribCopy.size = sizeof(SurfaceAttributes);
	attribCopy.dstOffset = 0;
	attribCopy.srcOffset = 0;
	
	vkCmdCopyBuffer(wr->m_CMDB, stage->m_attributes.m_gpuHandle->m_buffer, stage->m_attributesSB.m_gpuHandle->m_buffer, 1, &attribCopy);
	
	bufferMemBs[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	bufferMemBs[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferMemBs[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[0],
		0, nullptr);
	vkCmdPipelineBarrier(wr->m_CMDB, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[1],
		0, nullptr);
		
	VK_CALL(vkEndCommandBuffer(wr->m_CMDB));

	VK_CALL(vkBeginCommandBuffer(qr->m_CMDB, &beginI));
	vkCmdExecuteCommands(qr->m_CMDB, 1, &wr->m_CMDB);
	VK_CALL(vkEndCommandBuffer(qr->m_CMDB));

	VkSubmitInfo subI = {};
	subI.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	subI.commandBufferCount = 1;
	subI.pCommandBuffers = &qr->m_CMDB;
	vkQueueSubmit(qr->m_queue, 1, &subI, nullptr);
	vkQueueWaitIdle(qr->m_queue);
	
	void* attribData;
	vmaMapMemory(m_allocator, stage->m_attributesSB.m_gpuHandle->m_allocation, &attribData);
	SurfaceAttributes surfaceAttribs = {};
	std::memcpy(&surfaceAttribs, attribData, sizeof(SurfaceAttributes));
	vmaUnmapMemory(m_allocator, stage->m_attributesSB.m_gpuHandle->m_allocation);

	LOG("Surface Attributes");
	LOG("Cell Count: " + std::to_string(surfaceAttribs.cellCount));
	LOG("Vert Count: " + std::to_string(surfaceAttribs.vertexCount));
	LOG("Tris Count: " + std::to_string(surfaceAttribs.indexCount));
	if (surfaceAttribs.cellCount > 0)
	{
		stage->m_verticies.Dereference();
		stage->m_verticies.m_byteCount = surfaceAttribs.vertexCount * 16LL;
		stage->m_verticies.Allocate(this);

		stage->m_indicies.Dereference();
		stage->m_indicies.m_byteCount = surfaceAttribs.indexCount * 4LL;
		stage->m_indicies.Allocate(this);

		VkDescriptorBufferInfo vertBI = { stage->m_verticies.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };
		VkDescriptorBufferInfo idxsBI = { stage->m_indicies.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };


		std::vector<VkWriteDescriptorSet> descWrites(2);
		descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descWrites[0].descriptorCount = 1;
		descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descWrites[0].dstArrayElement = 0;
		descWrites[0].dstBinding = 3;
		descWrites[0].dstSet = stage->m_assemblyDSet;
		descWrites[0].pBufferInfo = &vertBI;
		descWrites[1] = descWrites[0];
		descWrites[1].dstBinding = 4;
		descWrites[1].pBufferInfo = &idxsBI;
		vkUpdateDescriptorSets(m_instance.device, 2, descWrites.data(), 0, nullptr);
		
		VK_CALL(vkBeginCommandBuffer(qr->m_CMDB, &beginI));
		bufferMemBs[0].buffer = stage->m_verticies.m_gpuHandle->m_buffer;
		bufferMemBs[0].srcAccessMask = 0;
		bufferMemBs[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferMemBs[1].buffer = stage->m_indicies.m_gpuHandle->m_buffer;
		bufferMemBs[1].srcAccessMask = 0;
		bufferMemBs[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(qr->m_CMDB, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			2, bufferMemBs.data(),
			0, nullptr);

		VkPipeline assemblyPipeline;
		VkPipelineLayout assemblyPipelineLayout;
		m_surfaceAssemblyPipeline.GetVkPipeline(assemblyPipeline, assemblyPipelineLayout);
		vkCmdBindDescriptorSets(qr->m_CMDB, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipelineLayout, 0, 1, &stage->m_assemblyDSet, 0, nullptr);
		vkCmdBindPipeline(qr->m_CMDB, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipeline);
		vkCmdPushConstants(qr->m_CMDB, assemblyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SurfacePipelineConstants), &surfConsts);
		vkCmdDispatch(qr->m_CMDB, (uint32_t)std::ceilf(surfaceAttribs.cellCount / 64.0f), 1, 1);

		bufferMemBs[0].srcQueueFamilyIndex = m_computeQueueFamily;
		bufferMemBs[0].dstQueueFamilyIndex = m_instance.queueFamilyIndex;
		bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferMemBs[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		bufferMemBs[1].srcQueueFamilyIndex = m_computeQueueFamily;
		bufferMemBs[1].dstQueueFamilyIndex = m_instance.queueFamilyIndex;
		bufferMemBs[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferMemBs[1].dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
		vkCmdPipelineBarrier(qr->m_CMDB, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0,
			0, nullptr,
			2, bufferMemBs.data(),
			0, nullptr);
		VK_CALL(vkEndCommandBuffer(qr->m_CMDB));

		vkQueueSubmit(qr->m_queue, 1, &subI, nullptr);
		vkQueueWaitIdle(qr->m_queue);

		LOG("MESH GENERATED!");
		m_testMutex.lock();
		m_vertexBuffer.Release(this);
		m_indexBuffer.Release(this);
		m_vertexBuffer = stage->m_verticies;
		m_indexBuffer = stage->m_indicies;
		m_vertexCount = surfaceAttribs.vertexCount;
		m_indexCount = surfaceAttribs.indexCount;
		m_testMutex.unlock();
	}
	m_stagingResources->enqueue(stageIndex);
}

void Engine::DestroyResource(GPUResourceHandle* resource)
{
	if (resource)
		m_loadingGarbage.add(resource);
}

void Engine::DestroyResources(const std::vector<GPUResourceHandle*>& resources)
{
	m_loadingGarbage.add(resources);
}

uint8_t Engine::GetWorkerCount()
{
	return std::thread::hardware_concurrency() - 1;;
}

void Engine::InitializeRenderPipeline()
{
	//Construct chunk pipeline
	m_renderPipeline.Release(this);
	m_renderPipeline.m_cullMode = VK_CULL_MODE_BACK_BIT;
	m_renderPipeline.m_wireframe = false;

	std::vector<VkPushConstantRange> pushConstants(1);
	pushConstants[0].offset = 0;
	pushConstants[0].size = 64; // single matrix
	pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	m_renderPipeline.m_pushConstants = pushConstants;

	std::vector<VkVertexInputBindingDescription> vertexBindings(1);
	vertexBindings[0].binding = 0;
	vertexBindings[0].stride = 16;
	vertexBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	std::vector<VkVertexInputAttributeDescription> vertexAttributes(3);
	vertexAttributes[0].binding = 0;
	vertexAttributes[0].location = 0;
	vertexAttributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexAttributes[0].offset = 0;
	vertexAttributes[1].binding = 0;
	vertexAttributes[1].location = 1;
	vertexAttributes[1].format = VK_FORMAT_R8G8B8_UNORM;
	vertexAttributes[1].offset = 12;
	vertexAttributes[2].binding = 0;
	vertexAttributes[2].location = 2;
	vertexAttributes[2].format = VK_FORMAT_R8_UINT;
	vertexAttributes[2].offset = 15;
	m_renderPipeline.m_vertexBindings = vertexBindings;
	m_renderPipeline.m_vertexAttributes = vertexAttributes;
}

void Engine::InitializeComputePipelines()
{
	m_surfaceAnalysisPipeline.Release(this);
	m_surfaceAssemblyPipeline.Release(this);
	if (m_formDSetLayout)
		vkDestroyDescriptorSetLayout(m_instance.device, m_formDSetLayout, nullptr);

	//Volume image
	//Index map image
	//Surface Cells buffer
	//Stats/Vert buffer
	//Tri buffer
	std::vector<VkDescriptorSetLayoutBinding> bindings(6);

	//Color map
	bindings[0].binding = 0;
	bindings[0].pImmutableSamplers = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.bindingCount = 1;
	layoutCI.pBindings = bindings.data();

	VK_CALL(vkCreateDescriptorSetLayout(m_instance.device,
		&layoutCI,
		nullptr,
		&m_formDSetLayout));


	//bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	//Index map
	bindings[1].binding = 1;
	bindings[1].pImmutableSamplers = 0;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	//Cells
	bindings[2].binding = 2;
	bindings[2].pImmutableSamplers = 0;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	//Attributes/Vertex buffer
	bindings[3].binding = 3;
	bindings[3].pImmutableSamplers = 0;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	//Create layouts
	layoutCI.bindingCount = 4;

	m_surfaceAnalysisPipeline.m_descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(1);
	VK_CALL(vkCreateDescriptorSetLayout(m_instance.device,
		&layoutCI,
		nullptr,
		m_surfaceAnalysisPipeline.m_descriptorSetLayouts.data()));

	std::vector<VkPushConstantRange> surfacePushConsts(1);
	surfacePushConsts[0].offset = 0;
	surfacePushConsts[0].size = sizeof(SurfacePipelineConstants);
	surfacePushConsts[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	m_surfaceAnalysisPipeline.m_pushConstants = surfacePushConsts;
	m_surfaceAnalysisPipeline.Allocate(this);


	//Index buffer
	bindings[4].binding = 4;
	bindings[4].pImmutableSamplers = 0;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	//Attribs buffer
	bindings[5].binding = 5;
	bindings[5].pImmutableSamplers = 0;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	layoutCI.bindingCount = 6;
	m_surfaceAssemblyPipeline.m_descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(1);
	VK_CALL(vkCreateDescriptorSetLayout(m_instance.device,
		&layoutCI,
		nullptr,
		m_surfaceAssemblyPipeline.m_descriptorSetLayouts.data()));

	m_surfaceAssemblyPipeline.m_pushConstants = surfacePushConsts;
	m_surfaceAssemblyPipeline.Allocate(this);
}

void Engine::InitializeStagingResources(uint8_t poolSize)
{
	ReleaseStagingResources();
	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.maxSets = (uint32_t)poolSize * 3;
	descPoolCI.flags = 0;
	std::vector<VkDescriptorPoolSize> DPSizes(2);
	DPSizes[0].descriptorCount = poolSize * 5;
	DPSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	DPSizes[1].descriptorCount = poolSize * 6;
	DPSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descPoolCI.poolSizeCount = static_cast<uint32_t>(DPSizes.size());
	descPoolCI.pPoolSizes = DPSizes.data();
	vkCreateDescriptorPool(m_instance.device, &descPoolCI, nullptr, &m_stagingDescriptorPool);


	std::vector<VkDescriptorSetLayout> layouts(3);
	layouts[0] = m_formDSetLayout;
	layouts[1] = m_surfaceAnalysisPipeline.m_descriptorSetLayouts[0];
	layouts[2] = m_surfaceAssemblyPipeline.m_descriptorSetLayouts[0];

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_stagingDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> sets(3);

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = m_computeQueueFamily;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VkCommandPool cmdP = VK_NULL_HANDLE;
	VK_CALL(vkCreateCommandPool(m_instance.device, &cmdPoolInfo, nullptr, &cmdP));

	VkCommandBufferAllocateInfo cmdBAllocInfo = {};
	cmdBAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBAllocInfo.commandPool = cmdP;
	cmdBAllocInfo.commandBufferCount = 1;
	cmdBAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer cmdB;
	VK_CALL(vkAllocateCommandBuffers(m_instance.device, &cmdBAllocInfo, &cmdB));

	VkCommandBufferBeginInfo beginI = {};
	beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CALL(vkBeginCommandBuffer(cmdB, &beginI));

	std::vector<VkImageMemoryBarrier> imageBarriers((uint64_t)poolSize * 2);

	m_stagingResources = new LFPoolQueue<ChunkStagingResources*>(poolSize);
	for (uint32_t i = 0; i < m_stagingResources->size(); i++)
	{
		ChunkStagingResources* sRes = new ChunkStagingResources(this,31,2); //TODO: Adjustible size/padding

		VK_CALL(vkAllocateDescriptorSets(m_instance.device, &allocInfo, sets.data()));
		sRes->WriteDescriptors(this, sets[0], sets[1], sets[2]);
		sRes->GetImageTransferBarriers(imageBarriers[i], imageBarriers[i + (uint64_t)poolSize]);

		(*m_stagingResources)[i] = sRes;
	}

	vkCmdPipelineBarrier(cmdB,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		static_cast<uint32_t>(imageBarriers.size()), imageBarriers.data());

	VK_CALL(vkEndCommandBuffer(cmdB));

	VkQueue queue = m_queues[0].m_queue;
	VkSubmitInfo subInfo = {};
	subInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	subInfo.commandBufferCount = 1;
	subInfo.pCommandBuffers = &cmdB;
	vkQueueSubmit(queue, 1, &subInfo, nullptr);
	vkQueueWaitIdle(queue);

	vkDestroyCommandPool(m_instance.device, cmdP, nullptr);
}

void Engine::ReleaseStagingResources()
{
	if (m_stagingResources == nullptr) return;

	std::vector<GPUResourceHandle*> resources(m_stagingResources->data(),
		m_stagingResources->data() + m_stagingResources->size());
	DestroyResources(resources);

	vkDestroyDescriptorPool(m_instance.device, m_stagingDescriptorPool, nullptr);
	SAFE_DEL(m_stagingResources);
}

void Engine::GarbageCollect(const GCForce force)
{
	if (force >= GC_FORCE_UNSAFE || m_dumpingFrame <= m_dumpFrame.load())
	{
		uint32_t retIndex = 0;
		for (uint32_t i = 0;i < m_dumpingGarbage.size();i++)
		{
			GPUResourceHandle* res = m_dumpingGarbage[i];
			if (force >= GC_FORCE_PINNED || !res->IsPinned())
			{
				res->Deallocate(this);
				delete res;
			}
			else
			{
				m_dumpingGarbage[retIndex] = res;
				retIndex++;
			}
		}
		m_dumpingGarbage.resize(retIndex);
		m_loadingGarbage.swap(m_dumpingGarbage);
		m_dumpingFrame = m_loadingFrame.load();
		if (force >= GC_FORCE_COMPLETE)
		{
			for (uint32_t i = 0; i < m_dumpingGarbage.size(); i++)
			{
				GPUResourceHandle* res = m_dumpingGarbage[i];
				res->Deallocate(this);
				delete res;
			}
			m_dumpingGarbage.clear();
		}
	}
}

#pragma region FUNCTION_EXPORTS
EXPORT void SetSurfaceShaders(Engine* instance, char* vs, int vsSize, char* fs, int fsSize)
{
	std::vector<char> vertex(vs, vs + vsSize);
	std::vector<char> fragment(fs, fs + fsSize);
	instance->SetSurfaceShaders(vertex, fragment);
}

EXPORT void SetComputeShaders(Engine* instance, char* analysis, int analysisSize, char* assembly, int assemblySize)
{
	std::vector<char> surfaceAnalysis(analysis, analysis + analysisSize);
	std::vector<char> surfaceAssembly(assembly, assembly + assemblySize);
	instance->SetComputeShaders(surfaceAnalysis, surfaceAssembly);
}

EXPORT ComputePipeline* CreateFormPipeline(Engine* instance, char* formShader, int shaderSize)
{
	std::vector<char> shader(formShader, formShader + shaderSize);
	return instance->CreateFormPipeline(shader);
}

EXPORT void Release(Engine* instance, GPUResource*& resource)
{
	SAFE_DEL_DEALLOC(resource, instance);
}

EXPORT void ComputeTest(Engine* instance, ComputePipeline* compute)
{
	instance->ComputeTest(compute);
}

EXPORT void InitializeVoxulkanInstance(Engine* instance)
{
	instance->InitializeResources();
}

EXPORT void InvokeGC(Engine* instance)
{
	instance->GarbageCollect();
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID, void* userData)
{
	Camera* camera = static_cast<Camera*>(userData);
	camera->m_instance->Draw(camera);
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderInjection()
{
	return OnRenderEvent;
}
#pragma endregion