#include "Engine.h"
#include "Plugin.h"
#include <algorithm>

Engine::Engine(IUnityGraphicsVulkan* unityVulkan)
{
	m_unityVulkan = unityVulkan;
	m_instance = m_unityVulkan->Instance();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = m_instance.physicalDevice;
	allocatorInfo.device = m_instance.device;
	vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

void Engine::RegisterQueues(std::vector<VkQueue> queues, const uint32_t& queueFamily, VkQueue occlusionQueue)
{
	if (m_queues != nullptr || m_workers != nullptr || m_occlusionQueue != nullptr)
		return;

	m_occlusionQueue = occlusionQueue;
	m_computeQueueFamily = queueFamily;
	m_queueCount = static_cast<uint8_t>(queues.size());
	m_workerCount = GetWorkerCount();
	if (m_workerCount < m_queueCount)
	{
		m_queueCount = m_workerCount;
		LOG("Queue count is some how larger than worker count!");
	}

	VkCommandPoolCreateInfo computeCMDPoolInfo = {};
	computeCMDPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	computeCMDPoolInfo.queueFamilyIndex = m_computeQueueFamily;
	computeCMDPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandBufferAllocateInfo workerCMDBInfo = {};
	workerCMDBInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	workerCMDBInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	workerCMDBInfo.commandBufferCount = Engine::WORKER_CMDB_COUNT;

	VkCommandPoolCreateInfo renderCMDPoolInfo = {};
	renderCMDPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	renderCMDPoolInfo.queueFamilyIndex = m_instance.queueFamilyIndex;
	renderCMDPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandBufferAllocateInfo renderCMDBInfo = {};
	renderCMDBInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	renderCMDBInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	renderCMDBInfo.commandBufferCount = 1;

	VkFenceCreateInfo fenceCI = {};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	VkEventCreateInfo eventCI = {};
	eventCI.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

	uint32_t fenceCount = m_queueCount * Engine::WORKER_CMDB_COUNT;
	VkFence* fences = new VkFence[fenceCount];
	for (uint8_t i = 0; i < fenceCount; i++)
	{
		VK_CALL(vkCreateFence(m_instance.device, &fenceCI, nullptr, fences + i));
	}

	m_queues = new QueueResource[m_queueCount];
	m_workers = new WorkerResource[m_workerCount];
	uint8_t workerStart = 0;
	for (uint8_t i = 0; i < m_queueCount; i++)
	{
		QueueResource& qr = m_queues[i];
		qr.m_queue = queues[i];
		qr.m_currentCMDB = 0;
		qr.m_fences = fences + i * (uint64_t)Engine::WORKER_CMDB_COUNT;
		qr.m_workerStart = workerStart;
		qr.m_workerEnd = ((i + 1) * m_workerCount) / m_queueCount;
		workerStart = qr.m_workerEnd;
		for (uint32_t j = qr.m_workerStart; j < qr.m_workerEnd; j++)
		{
			WorkerResource& wr = m_workers[j];
			wr.m_queueIndex = i;

			VK_CALL(vkCreateCommandPool(m_instance.device, &computeCMDPoolInfo, nullptr, &wr.m_computeCMDPool));
			workerCMDBInfo.commandPool = wr.m_computeCMDPool;
			wr.m_computeCMDBs = std::vector<VkCommandBuffer>(workerCMDBInfo.commandBufferCount);
			VK_CALL(vkAllocateCommandBuffers(m_instance.device, &workerCMDBInfo, wr.m_computeCMDBs.data()));

			VK_CALL(vkCreateCommandPool(m_instance.device, &renderCMDPoolInfo, nullptr, &wr.m_queryCMDPool));
			renderCMDBInfo.commandPool = wr.m_queryCMDPool;
			VK_CALL(vkAllocateCommandBuffers(m_instance.device, &renderCMDBInfo, &wr.m_queryCMDB));
			VK_CALL(vkCreateFence(m_instance.device, &fenceCI, nullptr, &wr.m_queryFence));
			VK_CALL(vkCreateEvent(m_instance.device, &eventCI, nullptr, &wr.m_queryEvent));
		}
	}
}

void Engine::SetSurfaceShaders(std::vector<char>& vertex, std::vector<char>& tessCtrl, std::vector<char>& tessEval, std::vector<char>& fragment)
{
	m_renderPipeline.m_vertexShader.swap(vertex);
	m_renderPipeline.m_tessCtrlShader.swap(tessCtrl);
	m_renderPipeline.m_tessEvalShader.swap(tessEval);
	m_renderPipeline.m_fragmentShader.swap(fragment);
}

void Engine::SetComputeShaders(const std::vector<char>& surfaceAnalysis, const std::vector<char>& surfaceAssembly)
{
	m_surfaceAnalysisPipeline.m_shader = surfaceAnalysis;
	m_surfaceAssemblyPipeline.m_shader = surfaceAssembly;
}

void Engine::SetMaterialResources(void* attributesBuffer, uint32_t attribsByteCount,
	void* colorSpecData, uint32_t csWidth, uint32_t csHeight,
	void* nrmHeightData, uint32_t nhWidth, uint32_t nhHeight,
	uint32_t materialCount)
{
	m_surfaceAttributesBuffer.m_bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	m_surfaceAttributesBuffer.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_surfaceAttributesBuffer.m_byteCount = attribsByteCount;
	m_surfaceAttributesBuffer.Allocate(this);

	m_surfaceColorSpecTex.m_size = { csWidth, csHeight, 1};
	m_surfaceColorSpecTex.m_arraySize = materialCount;
	m_surfaceColorSpecTex.m_createSampler = true;
	m_surfaceColorSpecTex.m_createView = true;
	m_surfaceColorSpecTex.m_format = VK_FORMAT_R8G8B8A8_UNORM;
	m_surfaceColorSpecTex.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_surfaceColorSpecTex.m_tiling = VK_IMAGE_TILING_OPTIMAL;
	m_surfaceColorSpecTex.m_type = VK_IMAGE_TYPE_2D;
	m_surfaceColorSpecTex.m_viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	m_surfaceColorSpecTex.m_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	m_surfaceNrmHeightTex = m_surfaceColorSpecTex;
	m_surfaceColorSpecTex.Allocate(this);

	m_surfaceNrmHeightTex.m_size = { nhWidth, csHeight, 1 };
	m_surfaceNrmHeightTex.Allocate(this);

	VkDeviceSize csByteCount = (VkDeviceSize)csWidth * csHeight * materialCount * 4;
	VkDeviceSize nhByteCount = (VkDeviceSize)nhWidth * nhHeight * materialCount * 4;

	GPUBuffer sb = {};
	sb.m_bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	sb.m_memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;

	sb.m_byteCount = attribsByteCount + csByteCount + nhByteCount;
	sb.Allocate(this);
	void* mappedData;
	vmaMapMemory(m_allocator, sb.m_gpuHandle->m_allocation, &mappedData);

	memcpy(mappedData, attributesBuffer, attribsByteCount);
	char* cdata = static_cast<char*>(mappedData);
	memcpy(cdata + attribsByteCount, colorSpecData, csByteCount);
	memcpy(cdata + (attribsByteCount + csByteCount), nrmHeightData, nhByteCount);
	vmaUnmapMemory(m_allocator, sb.m_gpuHandle->m_allocation);
	
	VmaAllocationInfo allocI;
	vmaGetAllocationInfo(m_allocator, sb.m_gpuHandle->m_allocation, &allocI);
	if ((allocI.memoryType | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
	{
		vmaFlushAllocation(m_allocator, sb.m_gpuHandle->m_allocation, 0, sb.m_byteCount);
	}

	VkCommandBuffer cmdb = m_workers[0].m_computeCMDBs[0];
	VkQueue q = m_queues[0].m_queue;
	VkCommandBufferBeginInfo beginI = {};
	beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CALL(vkBeginCommandBuffer(cmdb, &beginI));

	VkBufferCopy cpyB = {};
	cpyB.size = attribsByteCount;
	cpyB.srcOffset = 0;
	cpyB.dstOffset = 0;
	vkCmdCopyBuffer(cmdb, sb.m_gpuHandle->m_buffer, m_surfaceAttributesBuffer.m_gpuHandle->m_buffer, 1, &cpyB);
	
	std::vector<VkImageMemoryBarrier> imgBs(2);
	imgBs[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBs[0].image = m_surfaceColorSpecTex.m_gpuHandle->m_image;
	imgBs[0].srcAccessMask = 0;
	imgBs[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imgBs[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgBs[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imgBs[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBs[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBs[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBs[0].subresourceRange.baseArrayLayer = 0;
	imgBs[0].subresourceRange.baseMipLevel = 0;
	imgBs[0].subresourceRange.levelCount = 1;
	imgBs[0].subresourceRange.layerCount = materialCount;
	imgBs[1] = imgBs[0];
	imgBs[1].image = m_surfaceNrmHeightTex.m_gpuHandle->m_image;

	VkMemoryBarrier memB = {};
	memB.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memB.srcAccessMask = 0;
	memB.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(cmdb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		2, imgBs.data());

	VkBufferImageCopy csCpy = {};
	csCpy.imageExtent = m_surfaceColorSpecTex.m_size;
	csCpy.bufferOffset = attribsByteCount;
	csCpy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	csCpy.imageSubresource.baseArrayLayer = 0;
	csCpy.imageSubresource.layerCount = materialCount;
	csCpy.imageSubresource.mipLevel = 0;
	vkCmdCopyBufferToImage(cmdb, sb.m_gpuHandle->m_buffer, m_surfaceColorSpecTex.m_gpuHandle->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &csCpy);

	VkBufferImageCopy nhCpy = csCpy;
	nhCpy.bufferOffset += csByteCount;
	vkCmdCopyBufferToImage(cmdb, sb.m_gpuHandle->m_buffer, m_surfaceNrmHeightTex.m_gpuHandle->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &nhCpy);

	imgBs[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imgBs[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgBs[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imgBs[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imgBs[1] = imgBs[0];
	imgBs[1].image = m_surfaceNrmHeightTex.m_gpuHandle->m_image;
	memB.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(cmdb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		2, imgBs.data());
		
	VK_CALL(vkEndCommandBuffer(cmdb));

	VkSubmitInfo submitI = {};
	submitI.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitI.commandBufferCount = 1;
	submitI.pCommandBuffers = &cmdb;
	VK_CALL(vkQueueSubmit(q, 1, &submitI, nullptr));
	VK_CALL(vkQueueWaitIdle(q));

	sb.m_gpuHandle->Deallocate(this);
	sb.Dereference();
}

void Engine::InitializeResources()
{
	m_loadingFrame.store(0);
	InitializeRenderPipeline();
	InitializeComputePipelines();
	InitializeStagingResources(50); //TODO: Replace constant with variable
	
	GarbageCollect(GC_FORCE_COMPLETE);
}

void Engine::ReleaseResources()
{
	vkDeviceWaitIdle(m_instance.device);
	ReleaseStagingResources();
	ReleaseRenderPipelines();
	ReleaseComputePipelines();
	m_surfaceAttributesBuffer.Release(this);
	m_surfaceColorSpecTex.Release(this);
	m_surfaceNrmHeightTex.Release(this);

	GarbageCollect(GC_FORCE_COMPLETE);
	vmaDestroyAllocator(m_allocator);
}

void Engine::SubmitQueue(uint8_t queueIndex)
{
	QueueResource& qr = m_queues[queueIndex];

	std::vector<VkCommandBuffer> cmds(qr.m_workerEnd - qr.m_workerStart);
	uint32_t cmdbCount = 0;
	for (int i = qr.m_workerStart; i < qr.m_workerEnd; i++)
	{
		WorkerResource& wr = m_workers[i];

		if (wr.m_recordingCmds)
		{
			VkCommandBuffer cmdb = wr.m_computeCMDBs[qr.m_currentCMDB];
			VK_CALL(vkEndCommandBuffer(cmdb));
			wr.m_recordingCmds = false;
			cmds[cmdbCount] = cmdb;
			cmdbCount++;
		}
	}

	if (cmdbCount > 0)
	{
		VkSubmitInfo subI = {};
		subI.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		subI.commandBufferCount = cmdbCount;
		subI.pCommandBuffers = cmds.data();
		vkResetFences(m_instance.device, 1, &qr.m_fences[qr.m_currentCMDB]);
		VK_CALL(vkQueueSubmit(qr.m_queue, 1, &subI, qr.m_fences[qr.m_currentCMDB]));
		qr.m_currentCMDB = (qr.m_currentCMDB + 1) % Engine::WORKER_CMDB_COUNT;
	}
}

void Engine::QueryOcclusion(Camera* camera, uint8_t workerIndex)
{
	std::vector<BodyRenderPackage> render = m_render.vector();
	CameraView cameraConsts = const_cast<CameraView&>(camera->m_view);
	for (size_t i = 0; i < render.size(); i++)
	{
		BodyRenderPackage& r = render[i];
		glm::vec3 cPos = glm::inverse(const_cast<glm::mat4x4&>(r.transform)) * glm::vec4(cameraConsts.worldPosition, 1.0);
		glm::vec3 delta = cPos - glm::clamp(cPos, r.min, r.max);
		r.distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		
		for (size_t j = 0; j < r.chunks.size(); j++)
		{
			ChunkRenderPackage& chunk = r.chunks[j];
			delta = cPos - glm::clamp(cPos, chunk.min, chunk.max);
			chunk.distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		}
		std::sort(r.chunks.begin(), r.chunks.end());
	}
	std::sort(render.begin(), render.end());

	camera->m_renderPackage.swap(render);
}

void Engine::ClearRender()
{
	m_render.clear();
}

//#include <chrono>
//typedef std::chrono::high_resolution_clock Clock;

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
	
	CameraView cameraConsts = const_cast<CameraView&>(camera->m_view);
	const VkDeviceSize offset = 0;
	vkCmdBindPipeline(recordingState.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(recordingState.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &m_renderDSet, 0, nullptr);

	//auto t1 = Clock::now();
	camera->m_renderPackage.lock();
	for (size_t i = 0; i < camera->m_renderPackage.size(); i++)
	{
		BodyRenderPackage& brp = camera->m_renderPackage[i];

		ChunkPipelineConstants cpc = {};
		cpc.model = const_cast<glm::mat4x4&>(brp.transform);
		cpc.mvp = cameraConsts.viewProjection * cpc.model;
		cpc.tessellationFactor = cameraConsts.tessellationFactor;
		cpc.worldPosition = cameraConsts.worldPosition;

		vkCmdPushConstants(recordingState.commandBuffer, layout, RENDER_CONST_STAGE_BIT,
			0, sizeof(ChunkPipelineConstants), &cpc);

		for (size_t j = 0; j < brp.chunks.size(); j++)
		{
			ChunkRenderPackage crp = brp.chunks[j];
			vkCmdBindVertexBuffers(recordingState.commandBuffer, 0, 1, &crp.vertexBuffer->m_buffer, &offset);
			vkCmdBindIndexBuffer(recordingState.commandBuffer, crp.indexBuffer->m_buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(recordingState.commandBuffer, crp.indexCount, 1, 0, 0, 0);
		}
	}
	camera->m_renderPackage.unlock();
	/*
	auto t2 = Clock::now();
	std::stringstream ss;
	ss << "Delta t2-t1: "
		<< std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count()
		<< " nanoseconds" << std::endl;
	LOG(ss.str());*/
}

ComputePipeline* Engine::CreateFormPipeline(const std::vector<char>& shader)
{
	ComputePipeline* form = new ComputePipeline();
	form->m_shader = shader;
	form->m_descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(1);
	form->m_descriptorSetLayouts[0] = m_formDSetLayout;
	form->m_pushConstants = std::vector<VkPushConstantRange>(1);
	form->m_pushConstants[0].offset = 0;
	form->m_pushConstants[0].size = sizeof(FormConstants);
	form->m_pushConstants[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	form->Allocate(this);
	return form;
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
	return std::thread::hardware_concurrency();
}

void Engine::InitializeRenderPipeline()
{
	//Construct chunk pipeline
	ReleaseRenderPipelines();
	m_renderPipeline.m_cullMode = VK_CULL_MODE_BACK_BIT;
	m_renderPipeline.m_wireframe = false;

	std::vector<VkPushConstantRange> pushConstants(1);
	pushConstants[0].offset = 0;
	pushConstants[0].size = sizeof(ChunkPipelineConstants);
	pushConstants[0].stageFlags = RENDER_CONST_STAGE_BIT;
	m_renderPipeline.m_pushConstants = pushConstants;

	std::vector<VkDescriptorSetLayout> dSetLayouts(1);
	std::vector<VkDescriptorSetLayoutBinding> bindings(3);
	bindings[0].binding = 0;
	bindings[0].descriptorCount = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[0].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[0].pImmutableSamplers = 0;
	bindings[1] = bindings[0];
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[2] = bindings[1];
	bindings[2].binding = 2;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutCI = {};
	layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutCI.pBindings = bindings.data();

	VK_CALL(vkCreateDescriptorSetLayout(m_instance.device,
		&layoutCI,
		nullptr,
		&dSetLayouts[0]));
	m_renderPipeline.m_descriptorSetLayouts = dSetLayouts;


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
	ReleaseComputePipelines();

	//Volume image
	//Index map image
	//Surface Cells buffer
	//Stats/Vert buffer
	//Tri buffer
	std::vector<VkDescriptorSetLayoutBinding> bindings(4);

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
	surfacePushConsts[0].size = sizeof(SurfaceAnalysisConstants);
	surfacePushConsts[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	m_surfaceAnalysisPipeline.m_pushConstants = surfacePushConsts;
	m_surfaceAnalysisPipeline.Allocate(this);

	//Index buffer
	bindings[0].binding = 0;
	bindings[0].pImmutableSamplers = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	//Attribs buffer
	bindings[1].binding = 1;
	bindings[1].pImmutableSamplers = 0;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	layoutCI.bindingCount = 2;
	layoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	m_surfaceAssemblyPipeline.m_descriptorSetLayouts = std::vector<VkDescriptorSetLayout>(2);
	m_surfaceAssemblyPipeline.m_descriptorSetLayouts[0] = m_surfaceAnalysisPipeline.m_descriptorSetLayouts[0];
	VK_CALL(vkCreateDescriptorSetLayout(m_instance.device,
		&layoutCI,
		nullptr,
		&m_surfaceAssemblyPipeline.m_descriptorSetLayouts[1]));

	surfacePushConsts[0].size = sizeof(SurfaceAssemblyConstants);
	m_surfaceAssemblyPipeline.m_pushConstants = surfacePushConsts;
	m_surfaceAssemblyPipeline.Allocate(this);
}

void Engine::InitializeStagingResources(uint8_t poolSize)
{
	ReleaseStagingResources();
	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.maxSets = (uint32_t)poolSize * 3 + 1;
	descPoolCI.flags = 0;

	std::vector<VkDescriptorPoolSize> DPSizes(4);
	DPSizes[0].descriptorCount = poolSize * 5;
	DPSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	DPSizes[1].descriptorCount = poolSize * 4;
	DPSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	//Rendering descriptor
	DPSizes[2].descriptorCount = 1;
	DPSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	DPSizes[3].descriptorCount = 2;
	DPSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	descPoolCI.poolSizeCount = static_cast<uint32_t>(DPSizes.size());
	descPoolCI.pPoolSizes = DPSizes.data();
	vkCreateDescriptorPool(m_instance.device, &descPoolCI, nullptr, &m_stagingDescriptorPool);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_stagingDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_renderPipeline.m_descriptorSetLayouts[0];
	VK_CALL(vkAllocateDescriptorSets(m_instance.device, &allocInfo, &m_renderDSet));

	VkDescriptorImageInfo texCSI = {};
	texCSI.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	texCSI.imageView = m_surfaceColorSpecTex.m_gpuHandle->m_view;
	texCSI.sampler = m_surfaceColorSpecTex.m_gpuHandle->m_sampler;
	VkDescriptorImageInfo texNHI = texCSI;
	texNHI.imageView = m_surfaceNrmHeightTex.m_gpuHandle->m_view;
	texNHI.sampler = m_surfaceNrmHeightTex.m_gpuHandle->m_sampler;
	VkDescriptorBufferInfo attribInfo = { m_surfaceAttributesBuffer.m_gpuHandle->m_buffer , 0, VK_WHOLE_SIZE };
	std::vector<VkWriteDescriptorSet> writes(3);
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[0].dstArrayElement = 0;
	writes[0].dstBinding = 0;
	writes[0].dstSet = m_renderDSet;
	writes[0].pBufferInfo = &attribInfo;
	writes[1] = writes[0];
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].dstBinding = 1;
	writes[1].pBufferInfo = nullptr;
	writes[1].pImageInfo = &texNHI;
	writes[2] = writes[1];
	writes[2].dstBinding = 2;
	writes[2].pImageInfo = &texCSI;

	vkUpdateDescriptorSets(m_instance.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

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

	std::vector<VkDescriptorSetLayout> layouts(3);
	layouts[0] = m_formDSetLayout;
	layouts[1] = m_surfaceAnalysisPipeline.m_descriptorSetLayouts[0];
	layouts[2] = m_surfaceAssemblyPipeline.m_descriptorSetLayouts[0];
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> sets(3);
	m_stagingResources = new LFPoolStack<ChunkStagingResources*>(poolSize);
	for (uint32_t i = 0; i < m_stagingResources->size(); i++)
	{
		ChunkStagingResources* sRes = new ChunkStagingResources(this, CHUNK_SIZE, CHUNK_PADDING);

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

void Engine::ReleaseRenderPipelines()
{
	m_renderPipeline.Release(this);
	m_renderPipeline.DestroyDSetLayouts(this);
}

void Engine::ReleaseComputePipelines()
{
	m_surfaceAnalysisPipeline.Release(this);
	m_surfaceAssemblyPipeline.Release(this);
	//m_surfaceAnalysisPipeline.DestroyDSetLayouts(this);//Assembly shares analysis Layouts and more, therefor only assembly should be destroyed
	m_surfaceAssemblyPipeline.DestroyDSetLayouts(this);
	if (m_surfaceAnalysisPipeline.m_descriptorSetLayouts.size() != 0)
	if (m_formDSetLayout)
		vkDestroyDescriptorSetLayout(m_instance.device, m_formDSetLayout, nullptr);
}

void Engine::ReleaseStagingResources()
{
	if (m_stagingResources == nullptr) return;

	std::vector<GPUResourceHandle*> resources(m_stagingResources->data(),
		m_stagingResources->data() + m_stagingResources->size());
	DestroyResources(resources);

	vkDestroyDescriptorPool(m_instance.device, m_stagingDescriptorPool, nullptr);
	SAFE_DEL(m_stagingResources);

	VkFence* fences = m_queues[0].m_fences;

	for (int i = 0; i < m_queueCount * Engine::WORKER_CMDB_COUNT; i++)
		vkDestroyFence(m_instance.device, fences[i], nullptr);
	for (int i = 0; i < m_workerCount; i++)
	{
		WorkerResource& wr = m_workers[i];
		vkDestroyCommandPool(m_instance.device, wr.m_computeCMDPool, nullptr);
		vkDestroyCommandPool(m_instance.device, wr.m_queryCMDPool, nullptr);
		vkDestroyEvent(m_instance.device, wr.m_queryEvent, nullptr);
		vkDestroyFence(m_instance.device, wr.m_queryFence, nullptr);
	}

	delete[] fences;

	delete[] m_workers;
	delete[] m_queues;
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
EXPORT void SetSurfaceShaders(Engine* instance, char* vs, int vsSize, char* tc, int tcSize, char* te, int teSize, char* fs, int fsSize)
{
	std::vector<char> vertex(vs, vs + vsSize);
	std::vector<char> tessCtrl(tc, tc + tcSize);
	std::vector<char> tessEval(te, te + teSize);
	std::vector<char> fragment(fs, fs + fsSize);
	instance->SetSurfaceShaders(vertex, tessCtrl, tessEval, fragment);
}

EXPORT void SetComputeShaders(Engine* instance, char* analysis, int analysisSize, char* assembly, int assemblySize)
{
	std::vector<char> surfaceAnalysis(analysis, analysis + analysisSize);
	std::vector<char> surfaceAssembly(assembly, assembly + assemblySize);
	instance->SetComputeShaders(surfaceAnalysis, surfaceAssembly);
}

EXPORT void SetMaterialResources(Engine* instance, void* attributesBuffer, uint32_t attribsByteCount,
	void* colorSpecData, uint32_t csWidth, uint32_t csHeight,
	void* nrmHeightData, uint32_t nhWidth, uint32_t nhHeight,
	uint32_t materialCount)
{
	instance->SetMaterialResources(attributesBuffer, attribsByteCount,
		colorSpecData, csWidth, csHeight,
		nrmHeightData, nhWidth, nhHeight,
		materialCount);
}

EXPORT uint8_t GetQueueCount(Engine* instance)
{
	return instance->GetQueueCount();
}

EXPORT void QueryOcclusion(Engine* instance, Camera* camera, uint8_t workerIndex)
{
	instance->QueryOcclusion(camera, workerIndex);
}

EXPORT void ClearRender(Engine* instance)
{
	instance->ClearRender();
}

EXPORT void SubmitQueue(Engine* instance, uint8_t queueIndex)
{
	instance->SubmitQueue(queueIndex);
}

EXPORT void FillMapData(char* csData, uint32_t csSize, char* nhData, uint32_t nhSize, char* byteData)
{
	memcpy(byteData, csData, (size_t)csSize * 4);
	memcpy(byteData + ((size_t)csSize * 4), nhData, (size_t)nhSize * 4);
}

EXPORT ComputePipeline* CreateFormPipeline(Engine* instance, char* formShader, int shaderSize)
{
	std::vector<char> shader(formShader, formShader + shaderSize);
	return instance->CreateFormPipeline(shader);
}

EXPORT void ReleaseHandle(Engine* instance, GPUResourceHandle*& resourceHandle)
{
	instance->DestroyResource(resourceHandle);
}

EXPORT void Release(Engine* instance, GPUResource*& resource)
{
	SAFE_DEL_DEALLOC(resource, instance);
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