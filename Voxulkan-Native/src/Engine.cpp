#include "Engine.h"
#include "Plugin.h"
#include <sstream>
#include <vector>
#include <thread>
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

void Engine::RegisterComputeQueues(std::vector<VkQueue> queues, const uint32_t& queueFamily)
{
	if (m_queues != nullptr || m_workers != nullptr)
		return;

	m_computeQueueFamily = queueFamily;
	m_queueCount = queues.size();
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

void Engine::SetChunkShaders(const std::vector<char>& vertex, const std::vector<char>& fragment)
{
	m_renderPipeline->m_vertexShader = vertex;
	m_renderPipeline->m_fragmentShader = fragment;
}

void Engine::InitializeResources()
{

	m_currentFrame.store(0);

	//Construct chunk pipeline
	SAFE_DELETE(m_renderPipeline);
	m_renderPipeline = new RenderPipeline();
	m_renderPipeline->m_cullMode = VK_CULL_MODE_NONE;
	m_renderPipeline->m_wireframe = false;

	std::vector<VkPushConstantRange> pushConstants(1);
	pushConstants[0].offset = 0;
	pushConstants[0].size = 64; // single matrix
	pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	m_renderPipeline->m_pushConstants = pushConstants;

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

	m_renderPipeline->m_vertexBindings = vertexBindings;
	m_renderPipeline->m_vertexAttributes = vertexAttributes;

	
	SAFE_DELETE(m_triangleBuffer);
	m_triangleBuffer = new GBuffer();
	m_triangleBuffer->m_memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
	m_triangleBuffer->m_bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

	struct MyVertex
	{
		float x, y, z;
		unsigned int color;
	};
	MyVertex verts[3] =
	{
		{ -0.5f, -0.25f,  0.0f, 0xFFff0000 },
		{ 0.5f, -0.25f,  0.0f, 0xFF00ff00 },
		{ 0,     0.5f ,  0.0f, 0xFF0000ff },
	};

	m_triangleBuffer->m_byteCount = sizeof(verts);
	m_triangleBuffer->Allocate(m_allocator);
	m_triangleBuffer->UploadData(m_allocator, verts, sizeof(verts));
	
	GarbageCollect(true);
}

void Engine::ReleaseResources()
{
	SAFE_DELETE_RES(m_renderPipeline);
	SAFE_DELETE_RES(m_triangleBuffer);

	GarbageCollect(true);
	vmaDestroyAllocator(m_allocator);
	vkDestroyCommandPool(m_instance.device, m_computeCmdPool, nullptr);
}

void Engine::Draw(Camera* camera)
{
	UpdateCurrentFrame();
	UnityVulkanRecordingState recordingState;
	if (!m_unityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
		return;
	
	VkPipeline pipeline;
	VkPipelineLayout layout;
	m_renderPipeline->ConstructOnRenderPass(m_instance.device, recordingState.renderPass);
	m_renderPipeline->GetVkPipeline(pipeline, layout);
	
	if (pipeline && layout && m_triangleBuffer->m_gpuHandle)
	{
		VkBuffer buffer = m_triangleBuffer->m_gpuHandle->m_buffer;
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
		}
	}
	else
	{
		LOG("pipeline or layout is null");
	}
	GarbageCollect();
}

void Engine::UpdateCurrentFrame()
{
	UnityVulkanRecordingState recordingState;

	if (!m_unityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
		return;

	m_currentFrame.store(recordingState.currentFrameNumber);
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

void Engine::SafeDestroyResource(GPUResourceHandle* resource)
{
	SafeDestroyResource(m_currentFrame.load(), resource);
}

void Engine::SafeDestroyResource(const unsigned long long& frameNumber, GPUResourceHandle* resource)
{
	if (resource)
	{
		m_deleteQueue[frameNumber].push_back(resource);
	}
}

void Engine::GarbageCollect(bool force)
{
	UnityVulkanRecordingState recordingState;
	if (force)
		recordingState.safeFrameNumber = ~0ull;
	else
		if (!m_unityVulkan->CommandRecordingState(&recordingState, kUnityVulkanGraphicsQueueAccess_DontCare))
			return;

	DeleteQueue::iterator it = m_deleteQueue.begin();
	while (it != m_deleteQueue.end())
	{
		if (it->first <= recordingState.safeFrameNumber)
		{
			for (size_t i = 0; i < it->second.size(); ++i)
			{
				
				it->second[i]->Dispose(m_allocator);
				SAFE_DELETE(it->second[i]);
			}
			m_deleteQueue.erase(it++);
		}
		else
			++it;
	}
}

uint8_t Engine::GetWorkerCount()
{
	return std::thread::hardware_concurrency() - 1;;
}

#pragma region FUNCTION_EXPORTS
EXPORT void SetRenderingShaders(Engine* instance, char* vs, int vsSize, char* fs, int fsSize)
{
	std::vector<char> vertex(vs, vs + vsSize);
	std::vector<char> fragment(fs, fs + fsSize);
	instance->SetChunkShaders(vertex, fragment);
	//Engine::Get().SetChunkShaders(vertex, fragment);
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID, void* userData)
{
	//Engine::Get().Draw(static_cast<Camera*>(userData));
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderInjection()
{
	return OnRenderEvent;
}
#pragma endregion