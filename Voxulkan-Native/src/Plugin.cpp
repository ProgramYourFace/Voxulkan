#include "Plugin.h"
#include "IUnityInterface.h"
#include "IUnityGraphics.h"
#include "Engine.h"
#include <ctime>
#include <sstream>

#pragma warning(disable : 4996)

typedef void(*logMSG_t) (const char* msg);
logMSG_t logMsg;
std::string fallback;
void Log(const std::string& msg) {
	if (logMsg)
	{
		(*logMsg)((fallback + msg).c_str());
		fallback = "";
	}
	else
	{
		std::time_t result = std::time(nullptr);
		tm* lt = std::localtime(&result);
		std::stringstream ss;
		ss << "[" << lt->tm_hour << ":" << lt->tm_min << ":" << lt->tm_sec << "] ";
		fallback += ss.str() + msg + "\n";
	}
}

EXPORT void RegisterLogCallback(logMSG_t logCallback)
{
	if (logCallback)
	{
		logMsg = logCallback;
		LOG("LOG REGISTERED");
	}
	else
	{
		LOG("LOG UNREGISTERED");
		logMsg = nullptr;
	}
}

static PFN_vkGetInstanceProcAddr UNITY_INTERFACE_API InterceptVulkanInitialization(PFN_vkGetInstanceProcAddr getInstanceProcAddr, void* userData);

static IUnityGraphics* s_Graphics = NULL;
static IUnityGraphicsVulkan* s_Vulkan = NULL;
static Engine* s_Engine;

static uint32_t s_ComputeFamilyIndex;
static std::vector<VkQueue> s_ComputeQueues;

EXPORT void CreateVoxulkanInstance(Engine*& instance)
{
	instance = new Engine(s_Vulkan);
	instance->RegisterComputeQueues(s_ComputeQueues, s_ComputeFamilyIndex);
}
EXPORT void DeleteVoxulkanInstance(Engine*& instance)
{
	instance->ReleaseResources();
	delete instance;
	instance = nullptr;
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	switch (eventType)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		LOG("GFX Device Event Initialized");
		break;
	}
	case kUnityGfxDeviceEventShutdown:
	{
		LOG("GFX Device Event Shutdown");
		break;
	}
	default:
		break;
	}
}

// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_Graphics = unityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

	s_Vulkan = unityInterfaces->Get<IUnityGraphicsVulkan>();
	s_Vulkan->InterceptInitialization(InterceptVulkanInitialization, nullptr);
	UnityVulkanPluginEventConfig eventConfig;
	eventConfig.graphicsQueueAccess = kUnityVulkanGraphicsQueueAccess_DontCare;
	eventConfig.renderPassPrecondition = kUnityVulkanRenderPass_EnsureInside;
	eventConfig.flags = kUnityVulkanEventConfigFlag_EnsurePreviousFrameSubmission | kUnityVulkanEventConfigFlag_ModifiesCommandBuffersState;
	s_Vulkan->ConfigureEvent(1, &eventConfig);
}

// Unity plugin unload event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
	//Engine::Get().Deinitialize();
}

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
	s_ComputeFamilyIndex = queueFamilyCount;
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
						s_ComputeFamilyIndex = i;
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
					s_ComputeFamilyIndex = i;
					cuQueueCount = qfp.queueCount;
				}
			}
		}
	}

	uint32_t desiredCompute = Engine::GetWorkerCount();
	float* priorities = nullptr;
	uint32_t queueIndex = 0;
	uint32_t queueCount = 0;
	if (s_ComputeFamilyIndex == uQueueInfo.queueFamilyIndex)//If graphics and compute queue are the same
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
		qCreateInfo.queueFamilyIndex = s_ComputeFamilyIndex;
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
	
	s_ComputeQueues = std::vector<VkQueue>(queueCount);
	for (uint32_t i = 0; i < queueCount; i++)
	{
		vkGetDeviceQueue(*pDevice, s_ComputeFamilyIndex, queueIndex + i, &s_ComputeQueues[i]);
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
