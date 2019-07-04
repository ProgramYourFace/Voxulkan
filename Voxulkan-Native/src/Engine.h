#pragma once
#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "Resources/RenderPipeline.h"
#include "Resources/ComputePipeline.h"
#include "Resources/GPUBuffer.h"
#include "Components/VoxelChunk.h"
#include "Containers/MutexList.h"
#include "Containers/LFPoolQueue.h"
#include "Camera.h"
#include <map>
#include <atomic>
#include <glm/mat4x4.hpp>

class VoxelBody;

class Engine
{
public:
	friend class VoxelBody;
	Engine(IUnityGraphicsVulkan* unityVulkan);
	void InitializeResources();
	void ReleaseResources();

	void RegisterComputeQueues(std::vector<VkQueue> queues, const uint32_t& queueFamily);
	void SetSurfaceShaders(const std::vector<char>& vertex, const std::vector<char>& fragment);
	void SetComputeShaders(const std::vector<char>& surfaceAnalysis, const std::vector<char>& surfaceAssembly);

	void Draw(Camera* camera);

	void DestroyResource(GPUResourceHandle* resource);
	void DestroyResources(const std::vector<GPUResourceHandle*>& resources);

	typedef enum GCForce {
		GC_FORCE_NONE = 0,
		GC_FORCE_UNSAFE = 1,
		GC_FORCE_PINNED = 2,
		GC_FORCE_COMPLETE = 3
	} GCForce;
	void GarbageCollect(const GCForce force = GC_FORCE_NONE);

	inline const VkDevice& Device() { return m_instance.device; }
	inline const VmaAllocator& Allocator() { return m_allocator; }

	static uint8_t GetWorkerCount();
private:

	void InitializeRenderPipeline();
	void InitializeComputePipelines();
	void InitializeStagingResources(uint8_t poolSize);
	void ReleaseStagingResources();

	IUnityGraphicsVulkan* m_unityVulkan = nullptr;
	UnityVulkanInstance m_instance = {};
	VmaAllocator m_allocator = nullptr;

	//Testing
	RenderPipeline m_renderPipeline = {};
	VkDescriptorSetLayout m_formDSetLayout;
	ComputePipeline m_surfaceAnalysisPipeline = {};
	ComputePipeline m_surfaceAssemblyPipeline = {};
	VoxelBody* m_testVB = nullptr;

	typedef struct WorkerResource
	{
		VkCommandBuffer m_CMDB = VK_NULL_HANDLE;
		std::vector<ChunkStagingResources> m_stagingQueue = {};
		uint32_t m_submittedQueueRange = 0;
		uint8_t m_queueIndex = 0xFF;
	} WorkerResource;
	WorkerResource* m_workers;
	uint8_t m_workerCount;

	typedef struct QueueResource
	{
		VkQueue m_queue = VK_NULL_HANDLE;
		uint8_t m_workerStart = 0xFF;
		uint8_t m_workerEnd = 0xFF;

		VkCommandBuffer m_CMDB = VK_NULL_HANDLE;
		GPUImage m_indexMap = {};
	} QueueResource;
	QueueResource* m_queues;
	uint8_t m_queueCount;

	VkCommandPool m_computeCmdPool;
	uint32_t m_computeQueueFamily = 0;

	LFPoolQueue<ChunkStagingResources*>* m_stagingResources = nullptr;
	VkDescriptorPool m_stagingDescriptorPool;

#define SAFE_DUMP_MARGIN 10
	typedef unsigned long long FrameNumber;
	MutexList<GPUResourceHandle*> m_loadingGarbage;
	std::vector<GPUResourceHandle*>  m_dumpingGarbage;
	std::atomic<FrameNumber> m_loadingFrame;
	std::atomic<FrameNumber> m_dumpFrame;
	FrameNumber m_dumpingFrame;
};