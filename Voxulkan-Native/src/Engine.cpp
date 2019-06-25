#include "Engine.h"
#include "Plugin.h"
#include <sstream>
#include <vector>
#include <thread>
#include <algorithm>

typedef void(__stdcall* PollCameraInfo) (void* camera, glm::mat4x4* outInfo);

PollCameraInfo S_POLL_CAMERA;

static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
	VkDeviceCreateInfo newCInfo = *pCreateInfo;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	const float queuePriority = 0.9f;

	if (pCreateInfo->queueCreateInfoCount != 1 || pCreateInfo->pQueueCreateInfos[0].queueCount != 1)
	{
		LOG("Something strange is happening with unity queues!");
	}

	VkDeviceQueueCreateInfo uQueueInfo = pCreateInfo->pQueueCreateInfos[0];
	uint32_t cuQueueFamily = queueFamilyCount;
	uint32_t cuQueueCount = 0;
	bool hasComputeOnly = false;
	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		VkQueueFamilyProperties qfp = queueFamilies[i];
		if (qfp.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			if (qfp.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				if (!hasComputeOnly)
				{
					if (qfp.queueCount > cuQueueCount)
					{
						cuQueueFamily = i;
						cuQueueCount = qfp.queueCount;
					}
				}
			}
			else
			{
				if (!hasComputeOnly)
				{
					cuQueueCount = 0;
					hasComputeOnly = true;
				}
				if (qfp.queueCount > cuQueueCount)
				{
					cuQueueFamily = i;
					cuQueueCount = qfp.queueCount;
				}
			}
		}
	}

	uint32_t desiredCompute = Engine::GetDesiredQueueCount();
	float* priorities = nullptr;
	uint32_t queueIndex = 0;
	uint32_t queueCount = 0;
	if (cuQueueFamily == uQueueInfo.queueFamilyIndex)//If graphics and compute queue are the same
	{
		if (uQueueInfo.queueCount < cuQueueCount)
		{
			queueIndex = uQueueInfo.queueCount;
			queueCount = std::min(desiredCompute, cuQueueCount - queueIndex);
			uQueueInfo.queueCount += queueCount;
			priorities = new float[uQueueInfo.queueCount];
			for (uint32_t k = 0; k < uQueueInfo.queueCount; k++)
				if (k < queueIndex)
					priorities[k] = uQueueInfo.pQueuePriorities[k];
				else
					priorities[k] = queuePriority;
			uQueueInfo.pQueuePriorities = priorities;
		}
		else
		{
			LOG("Not enough compute queues!");
			return VK_ERROR_INITIALIZATION_FAILED;
		}
		queueCreateInfos.push_back(uQueueInfo);
	}
	else
	{
		queueCreateInfos.push_back(uQueueInfo);
		queueCount = std::min(desiredCompute, cuQueueCount);

		VkDeviceQueueCreateInfo qCreateInfo;
		qCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		qCreateInfo.pNext = nullptr;
		qCreateInfo.flags = 0;
		qCreateInfo.queueFamilyIndex = cuQueueFamily;
		qCreateInfo.queueCount = queueCount;

		priorities = new float[qCreateInfo.queueCount];
		for (uint32_t k = 0; k < qCreateInfo.queueCount; k++)
				priorities[k] = queuePriority;
		qCreateInfo.pQueuePriorities = priorities;

		queueCreateInfos.push_back(qCreateInfo);
	}

	newCInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	newCInfo.pQueueCreateInfos = queueCreateInfos.data();

	VkResult result = vkCreateDevice(physicalDevice, &newCInfo, pAllocator, pDevice);
	if (result != VK_SUCCESS)
		LOG("Device creation failed!");

	SAFE_DELETE_ARR(priorities);

	Engine::Get().SetComputeQueueFamily(cuQueueFamily);

	for (uint32_t i = 0; i < queueCount; i++)
	{
		VkQueue queue;
		vkGetDeviceQueue(*pDevice, cuQueueFamily, queueIndex + i, &queue);
		Engine::Get().EnqueueComputeQueue(queue);
	}
	return result;
}

#ifdef _DEBUG
static VKAPI_ATTR VkResult VKAPI_CALL Hook_vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
	VkInstanceCreateInfo newCInfo = *pCreateInfo;

	std::vector<const char*> extensions(pCreateInfo->enabledExtensionCount);
	bool hasDebug = false;
	for (uint32_t i = 0; i < pCreateInfo->enabledExtensionCount; i++)
	{
		extensions[i] = pCreateInfo->ppEnabledExtensionNames[i];
		if (strcmp(pCreateInfo->ppEnabledExtensionNames[i], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
			hasDebug = true;
	}
	if (!hasDebug)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);


#define VK_LAYER_VALIDATION "VK_LAYER_KHRONOS_validation"

	std::vector<const char*> layers(pCreateInfo->enabledLayerCount);
	hasDebug = false;
	for (uint32_t i = 0; i < pCreateInfo->enabledLayerCount; i++)
	{
		layers[i] = pCreateInfo->ppEnabledLayerNames[i];
		if (strcmp(pCreateInfo->ppEnabledLayerNames[i], VK_LAYER_VALIDATION) == 0)
			hasDebug = true;
	}
	if (!hasDebug)
		layers.push_back(VK_LAYER_VALIDATION);

	newCInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	newCInfo.ppEnabledExtensionNames = extensions.data();
	newCInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
	newCInfo.ppEnabledLayerNames = layers.data();

	VkResult result = vkCreateInstance(&newCInfo, pAllocator, pInstance);
	if (result != VK_SUCCESS)
		LOG("Instance creation failed!");

	return result;
}
#endif

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Hook_vkGetInstanceProcAddr(VkInstance device, const char* funcName)
{
	if (!funcName)
		return NULL;

#define INTERCEPT(fn) if (strcmp(funcName, #fn) == 0) return (PFN_vkVoidFunction)&Hook_##fn
#ifdef _DEBUG
	INTERCEPT(vkCreateInstance);
#endif
	INTERCEPT(vkCreateDevice);
#undef INTERCEPT

	return NULL;
}

static PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API InterceptVulkanInitialization(PFN_vkGetInstanceProcAddr getInstanceProcAddr, void* userData)
{
	return Hook_vkGetInstanceProcAddr;
}

void Engine::Initialize(IUnityInterfaces* unityInterfaces)
{
	if (m_isInitialized)
		return;
	m_unityVulkan = unityInterfaces->Get<IUnityGraphicsVulkan>();
	m_isInitialized = m_unityVulkan->InterceptInitialization(InterceptVulkanInitialization, this);
	if (!m_isInitialized)
		return;
}

void Engine::Deinitialize()
{
	if (!m_isInitialized)
		return;

	m_isInitialized = false;
	OnDeviceDeinitialize();
}

bool Engine::IsInitialized()
{
	return m_isInitialized;
}

void Engine::SetComputeQueueFamily(const uint32_t& computeQueueFamily)
{
	m_computeQueueFamily = computeQueueFamily;
}

void Engine::DequeueComputeQueue(VkQueue& queue)
{
	m_computeQueue.pop_front(queue);
}

void Engine::EnqueueComputeQueue(const VkQueue& queue)
{
	m_computeQueue.push_back(queue);
}

void Engine::SetChunkShaders(const std::vector<char>& vertex, const std::vector<char>& fragment)
{
	m_chunkPipeline->m_vertexShader = vertex;
	m_chunkPipeline->m_fragmentShader = fragment;
}


void Engine::OnDeviceInitialize()
{
	m_instance = m_unityVulkan->Instance();

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = m_instance.physicalDevice;
	allocatorInfo.device = m_instance.device;
	vmaCreateAllocator(&allocatorInfo, &m_allocator);

	UnityVulkanPluginEventConfig eventConfig;
	eventConfig.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
	eventConfig.renderPassPrecondition = kUnityVulkanRenderPass_EnsureInside;
	eventConfig.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission | kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
	m_unityVulkan->ConfigureEvent(1, &eventConfig);

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = m_computeQueueFamily;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	vkCreateCommandPool(m_instance.device, &cmdPoolInfo, nullptr, &m_computeCmdPool);

	m_currentFrame.store(0);

	//Construct chunk pipeline
	SAFE_DELETE(m_chunkPipeline);
	m_chunkPipeline = new RenderPipeline();
	m_chunkPipeline->m_cullMode = VK_CULL_MODE_NONE;
	m_chunkPipeline->m_wireframe = false;

	std::vector<VkPushConstantRange> pushConstants(1);
	pushConstants[0].offset = 0;
	pushConstants[0].size = 64; // single matrix
	pushConstants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	m_chunkPipeline->m_pushConstants = pushConstants;

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

	m_chunkPipeline->m_vertexBindings = vertexBindings;
	m_chunkPipeline->m_vertexAttributes = vertexAttributes;

	
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

void Engine::OnDeviceDeinitialize()
{
	SAFE_DELETE_RES(m_chunkPipeline);
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
	m_chunkPipeline->ConstructOnRenderPass(m_instance.device, recordingState.renderPass);
	m_chunkPipeline->GetVkPipeline(pipeline, layout);
	
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
}

unsigned int Engine::GetDesiredQueueCount()
{
	return std::thread::hardware_concurrency() - 1;;
}

void Engine::SafeDestroyResource(GResourceHandle* resource)
{
	SafeDestroyResource(m_currentFrame.load(), resource);
}

void Engine::SafeDestroyResource(const unsigned long long& frameNumber, GResourceHandle* resource)
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

EXPORT void SetChunkShaders(char* vs, int vsSize, char* fs, int fsSize)
{
	std::vector<char> vertex(vs, vs + vsSize);
	std::vector<char> fragment(fs, fs + fsSize);
	Engine::Get().SetChunkShaders(vertex, fragment);
}

EXPORT void TEST_RunCompute(char* shader, int size)
{
	std::vector<char> s(shader, shader + size);
	Engine::Get().RunCompute(s);
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID, void* userData)
{
	Engine::Get().Draw(static_cast<Camera*>(userData));
}

extern "C" UnityRenderingEventAndData UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderInjection()
{
	return OnRenderEvent;
}