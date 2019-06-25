#pragma once
#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "Resources/RenderPipeline.h"
#include "Resources/ComputePipeline.h"
#include "Resources/GBuffer.h"
#include "Components/VoxelChunk.h"
#include "Camera.h"
#include <map>
#include <atomic>
#include <glm/mat4x4.hpp>

class Engine
{
public:
	Engine(IUnityGraphicsVulkan* unityVulkan);
	void InitializeResources();
	void ReleaseResources();

	void RegisterComputeQueues(std::vector<VkQueue> queues, const uint32_t& queueFamily);
	void SetChunkShaders(const std::vector<char>& vertex, const std::vector<char>& fragment);

	//Experiments
	void Draw(Camera* camera);
	void UpdateCurrentFrame();

	void SafeDestroyResource(GPUResourceHandle* resource);
	void SafeDestroyResource(const unsigned long long& frameNumber, GPUResourceHandle* resource);

	inline const VkDevice& Device() { return m_instance.device; }
	inline const VmaAllocator& Allocator() { return m_allocator; }

	static uint8_t GetWorkerCount();
private:
	void GarbageCollect(bool force = false);

	IUnityGraphicsVulkan* m_unityVulkan = nullptr;
	UnityVulkanInstance m_instance = {};
	VmaAllocator m_allocator = nullptr;

	//Testing
	RenderPipeline* m_renderPipeline = nullptr;
	GBuffer* m_triangleBuffer = nullptr;

	typedef struct WorkerResource
	{
		VkCommandBuffer m_CMDB;
		std::vector<ChunkStagingResources> m_stagingQueue;
		uint32_t m_submittedQueueRange;
		uint8_t m_queueIndex;
	} WorkerResource;
	WorkerResource* m_workers;
	uint8_t m_workerCount;

	typedef struct QueueResource
	{
		VkQueue m_queue;
		uint8_t m_workerStart;
		uint8_t m_workerEnd;

		VkCommandBuffer m_CMDB;
		GImage m_indexMap;
	} QueueResource;
	QueueResource* m_queues;
	uint8_t m_queueCount;

	VkCommandPool m_computeCmdPool;
	uint32_t m_computeQueueFamily = 0;

	typedef std::vector<GPUResourceHandle*> GPUResources;
	typedef unsigned long long FrameNumber;
	typedef std::map<FrameNumber, GPUResources> DeleteQueue;

	std::atomic<FrameNumber> m_currentFrame;
	DeleteQueue m_deleteQueue;
};