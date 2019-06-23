#pragma once
#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "Containers/MutexQueue.h"
#include "Resources/RenderPipeline.h"
#include "Resources/ComputePipeline.h"
#include "Resources/GBuffer.h"
#include "Camera.h"
#include <map>
#include <atomic>
#include <glm/mat4x4.hpp>

class Engine
{
public:
	Engine() {}
	void Initialize(IUnityInterfaces* unityInterfaces);
	void Deinitialize();
	bool IsInitialized();

	void SetComputeQueueFamily(const uint32_t& computeQueueFamily);
	void DequeueComputeQueue(VkQueue& queue);
	void EnqueueComputeQueue(const VkQueue& queue);

	void SetChunkShaders(const std::vector<char>& vertex, const std::vector<char>& fragment);

	void OnDeviceInitialize();
	void OnDeviceDeinitialize();

	//Experiments
	void Draw(Camera* camera);
	void UpdateCurrentFrame();
	void RunCompute(const std::vector<char>& shader);

	void SafeDestroyResource(GResourceHandle* resource);
	void SafeDestroyResource(const unsigned long long& frameNumber, GResourceHandle* resource);

	static unsigned int GetDesiredQueueCount();
	inline const VkDevice& Device() { return m_instance.device; }
	inline const VmaAllocator& Allocator() { return m_allocator; }
	static Engine& Get()
	{
		static Engine instance;
		return instance;
	}

	Engine(Engine const&) = delete;
	void operator=(Engine const&) = delete;
private:
	void GarbageCollect(bool force = false);

	bool m_isInitialized = false;
	IUnityGraphicsVulkan* m_unityVulkan = nullptr;
	UnityVulkanInstance m_instance = {};
	VmaAllocator m_allocator;

	//Testing
	RenderPipeline* m_chunkPipeline = nullptr;
	GBuffer* m_triangleBuffer = nullptr;

	MutexQueue<VkQueue> m_computeQueue;
	VkCommandPool m_computeCmdPool;
	uint32_t m_computeQueueFamily = 0;

	typedef std::vector<GResourceHandle*> GPUResources;
	typedef std::map<unsigned long long, GPUResources> DeleteQueue;

	std::atomic<unsigned long long> m_currentFrame;
	DeleteQueue m_deleteQueue;
};