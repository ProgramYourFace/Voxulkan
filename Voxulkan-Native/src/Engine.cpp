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
	InitializeStagingResources(10); //TODO: Replace constant with variable
	
	m_testVB = new VoxelBody({ 0.0f,0.0f,0.0f }, {16.0f,16.0f,16.0f});

	GarbageCollect(GC_FORCE_COMPLETE);
}

void Engine::ReleaseResources()
{
	SAFE_DEL(m_testVB);
	ReleaseStagingResources();

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
	m_testVB->Traverse(this, 0);

	UnityVulkanRecordingState recordingState;
	if (!m_unityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
		return;

	m_loadingFrame.store(recordingState.currentFrameNumber);
	m_dumpFrame.store(recordingState.safeFrameNumber - SAFE_DUMP_MARGIN);
	
	VkPipeline pipeline;
	VkPipelineLayout layout;
	m_renderPipeline.ConstructOnRenderPass(this, recordingState.renderPass);
	m_renderPipeline.GetVkPipeline(pipeline, layout);
	
	if (pipeline && layout)
	{
		/*
		VkBuffer buffer = m_triangleBuffer.m_gpuHandle->m_buffer;
		if (buffer)
		{
			glm::mat4x4 mvp = camera->m_VP_Matrix.load(std::memory_order_relaxed);
			const VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(recordingState.commandBuffer, 0, 1, &buffer, &offset);
			vkCmdPushConstants(recordingState.commandBuffer, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &mvp);
			vkCmdBindPipeline(recordingState.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			vkCmdDraw(recordingState.commandBuffer, 3, 1, 0, 0);
		}
		else
		{
			LOG("buffer is null");
		}*/
	}
	else
	{
		LOG("pipeline or layout is null");
	}
}
/*
#include "Resources/GImage.h"

void Engine::RunCompute(const std::vector<char>& shader)
{
	LOG("Running compute...");

	const uint32_t CHUNK_SIZE = 32;
	const uint32_t CHUNK_CUBED = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

	ComputePipeline cp;
	cp.m_shader = shader;

	//Configure bindings
	VkDescriptorSetLayoutBinding dslImageBinding = {};
	dslImageBinding.binding = 0;
	dslImageBinding.pImmutableSamplers = 0;
	dslImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	dslImageBinding.descriptorCount = 1;
	dslImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	//Create layouts
	VkDescriptorSetLayoutCreateInfo dslCInfo = {};
	dslCInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslCInfo.bindingCount = 1;
	dslCInfo.pBindings = &dslImageBinding;
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1);

	VK_CALL(vkCreateDescriptorSetLayout(m_instance.device, &dslCInfo, nullptr, descriptorSetLayouts.data()));
	cp.m_descriptorSetLayouts = descriptorSetLayouts;
	cp.Construct(m_instance.device);

	//Access descriptors
	VkDescriptorPoolCreateInfo descPoolCInfo = {};
	descPoolCInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSize.descriptorCount = 1;
	descPoolCInfo.poolSizeCount = 1;
	descPoolCInfo.pPoolSizes = &poolSize;
	descPoolCInfo.maxSets = 1;

	VkDescriptorPool descPool;
	VK_CALL(vkCreateDescriptorPool(m_instance.device, &descPoolCInfo, nullptr, &descPool));

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = descriptorSetLayouts.data();

	VkDescriptorSet descSet;
	VK_CALL(vkAllocateDescriptorSets(m_instance.device, &allocInfo, &descSet));

	VkCommandBufferAllocateInfo cmdBInfo = {};
	cmdBInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBInfo.commandBufferCount = 1;
	cmdBInfo.commandPool = m_computeCmdPool;
	cmdBInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer cb;
	VK_CALL(vkAllocateCommandBuffers(m_instance.device, &cmdBInfo, &cb));

	VkQueue q;
	DequeueComputeQueue(q);

	VkCommandBufferBeginInfo cbBeginInfo = {};
	cbBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cbBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CALL(vkBeginCommandBuffer(cb, &cbBeginInfo));

	GImage image;
	image.m_type = VK_IMAGE_TYPE_3D;
	image.m_usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.m_format = VK_FORMAT_R8G8B8A8_UNORM;
	image.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	image.m_size = { CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE };

	image.Allocate(m_allocator);
	VkImage vkImg = image.GetImage();
	
	//Bind descriptor to image
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageView = image.m_gpuHandle->m_view;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(m_instance.device, 1, &descriptorWrite, 0, nullptr);

	VkImageMemoryBarrier imgBarrier = {};
	imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imgBarrier.image = vkImg;
	imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imgBarrier.srcAccessMask = 0;
	imgBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgBarrier.subresourceRange.baseMipLevel = 0;
	imgBarrier.subresourceRange.levelCount = 1;
	imgBarrier.subresourceRange.baseArrayLayer = 0;
	imgBarrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cb,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &imgBarrier);
	
	VkPipeline pipeline;
	VkPipelineLayout layout;
	cp.GetVkPipeline(pipeline, layout);

	vkCmdBindDescriptorSets(cb,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		layout,
		0,
		1,
		&descSet,
		0,
		nullptr);

	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdDispatch(cb, 8, 8, 8);

	VkImageMemoryBarrier transferBarrier = imgBarrier;
	transferBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	transferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	transferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	transferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(cb,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, &transferBarrier);

	VkBufferImageCopy copy = {};
	copy.bufferOffset = 0;
	copy.bufferImageHeight = 0;
	copy.bufferRowLength = 0;
	copy.imageExtent = image.m_size;
	copy.imageOffset = { 0, 0, 0 };
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;
	copy.imageSubresource.mipLevel = 0;

	GBuffer stagingBuffer;
	stagingBuffer.m_bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	stagingBuffer.m_byteCount = CHUNK_CUBED * sizeof(int);
	stagingBuffer.m_memoryUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
	stagingBuffer.Allocate(m_allocator);

	vkCmdCopyImageToBuffer(cb, vkImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer.m_gpuHandle->m_buffer, 1, &copy);

	VK_CALL(vkEndCommandBuffer(cb));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cb;
	vkQueueSubmit(q, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(q);

	struct color8
	{
		BYTE r, g, b, a;
	};
	color8* data = new color8[CHUNK_CUBED];
	void* mappedData;
	vmaMapMemory(m_allocator, stagingBuffer.m_gpuHandle->m_allocation, &mappedData);
	memcpy(data, mappedData, CHUNK_CUBED * sizeof(color8));
	vmaUnmapMemory(m_allocator, stagingBuffer.m_gpuHandle->m_allocation);


	color8 c0 = data[CHUNK_CUBED - 1];
	std::stringstream ss;
	ss << "(r: "  << std::to_string(c0.r);
	ss << ", g: " << std::to_string(c0.g);
	ss << ", b: " << std::to_string(c0.b);
	ss << ", a: " << std::to_string(c0.a) << ")" << std::endl;
	LOG(ss.str());
	
	delete[] data;
	vkFreeCommandBuffers(m_instance.device, m_computeCmdPool, 1, &cb);
	EnqueueComputeQueue(q);
	stagingBuffer.Release();
	image.Release();
	cp.Release();
	vkDestroyDescriptorPool(m_instance.device, descPool, nullptr);
	vkDestroyDescriptorSetLayout(m_instance.device, descriptorSetLayouts[0], nullptr);
	GarbageCollect(true);

	LOG("Compute finished!...");
}*/

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
	m_renderPipeline.m_cullMode = VK_CULL_MODE_NONE;
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

	std::vector<VkVertexInputAttributeDescription> vertexAttributes(2);
	vertexAttributes[0].binding = 0;
	vertexAttributes[0].location = 0;
	vertexAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributes[0].offset = 0;
	vertexAttributes[1].binding = 0;
	vertexAttributes[1].location = 1;
	vertexAttributes[1].format = VK_FORMAT_R8G8B8A8_UNORM;
	vertexAttributes[1].offset = 12;

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
	//Stats buffer
	//Vert buffer
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

	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
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
	//Attributes
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

	m_surfaceAnalysisPipeline.Construct(this);

	//Read only
	//Index map read only
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	//Cells read only
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	//Vertex buffer
	bindings[4].binding = 4;
	bindings[4].pImmutableSamplers = 0;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	//Index buffer
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

	m_surfaceAssemblyPipeline.Construct(this);
}

void Engine::InitializeStagingResources(uint8_t poolSize)
{
	ReleaseStagingResources();
	VkDescriptorPoolCreateInfo descPoolCI = {};
	descPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCI.maxSets = (uint32_t)poolSize * 3;
	descPoolCI.flags = 0;
	std::vector<VkDescriptorPoolSize> DPSizes(4);
	DPSizes[0].descriptorCount = poolSize * 2;
	DPSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	DPSizes[1].descriptorCount = poolSize * 3;
	DPSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	DPSizes[2].descriptorCount = poolSize * 4;
	DPSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	DPSizes[3].descriptorCount = poolSize * 2;
	DPSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descPoolCI.poolSizeCount = static_cast<uint32_t>(DPSizes.size());
	descPoolCI.pPoolSizes = DPSizes.data();
	vkCreateDescriptorPool(m_instance.device, &descPoolCI, nullptr, &m_stagingDescriptorPool);

	m_stagingResources = new LFPoolQueue<ChunkStagingResources*>(poolSize);
	for (int i = 0; i < m_stagingResources->size(); i++)
	{
		ChunkStagingResources* sRes = new ChunkStagingResources(this, 31,2);
		sRes->AllocateDescriptors(this, 
			m_stagingDescriptorPool, m_formDSetLayout,
			m_surfaceAnalysisPipeline.m_descriptorSetLayouts[0],
			m_surfaceAssemblyPipeline.m_descriptorSetLayouts[0]);
		(*m_stagingResources)[i] = sRes;
	}
}

void Engine::ReleaseStagingResources()
{
	if (m_stagingResources != nullptr) return;

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