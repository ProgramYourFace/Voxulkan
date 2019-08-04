#pragma once
#include "IUnityGraphics.h"
#include "IUnityGraphicsVulkan.h"
#include "Resources/RenderPipeline.h"
#include "Resources/ComputePipeline.h"
#include "Resources/GPUBuffer.h"
#include "Resources/CommandBufferHandle.h"
#include "Components/VoxelBody.h"
#include "Containers/MutexList.h"
#include "Containers/LFPoolStack.h"
#include "Camera.h"
#include <map>
#include <atomic>
#include <glm/mat4x4.hpp>

class VoxelBody;

typedef struct QueueResource
{
	VkQueue m_queue = nullptr;
	VkFence* m_fences = nullptr;
	volatile uint8_t m_currentCMDB = 0;
	uint8_t m_workerStart = 0xFF;
	uint8_t m_workerEnd = 0xFF;
} QueueResource;

typedef struct WorkerResource
{
	bool m_recordingCmds = false;
	VkCommandPool m_computeCMDPool = nullptr;
	std::vector<VkCommandBuffer> m_computeCMDBs = {};
	VkCommandPool m_queryCMDPool = nullptr;
	VkCommandBuffer m_queryCMDB = nullptr;
	VkFence m_queryFence;
	VkEvent m_queryEvent;
	uint8_t m_queueIndex = 0xFF;
} WorkerResource;

class Engine
{
public:
	friend class VoxelBody;
	friend struct VoxelChunk;
	Engine(IUnityGraphicsVulkan* unityVulkan);
	void InitializeResources();
	void ReleaseResources();

	void RegisterQueues(std::vector<VkQueue> queues, const uint32_t& queueFamily, VkQueue occlusionQueue);
	void SetSurfaceShaders(std::vector<char>& vertex, std::vector<char>& tessCtrl, std::vector<char>& tessEval, std::vector<char>& fragment);
	void SetComputeShaders(const std::vector<char>& surfaceAnalysis, const std::vector<char>& surfaceAssembly);
	void SetMaterialResources(void* attributesBuffer, uint32_t attribsByteCount,
		void* colorSpecData, uint32_t csWidth, uint32_t csHeight,
		void* nrmHeightData, uint32_t nhWidth, uint32_t nhHeight,
		uint32_t materialCount);

	inline uint8_t GetQueueCount() { return m_queueCount; };

	void SubmitQueue(uint8_t queueIndex);
	void QueryOcclusion(Camera* camera, uint8_t workerIndex);
	void ClearRender();
	void Draw(Camera* camera);
	ComputePipeline* CreateFormPipeline(const std::vector<char>& shader);

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

	static const uint8_t CHUNK_SIZE = 31;
	static const uint8_t CHUNK_PADDING = 2;
	static const uint8_t WORKER_CMDB_COUNT = 3;
	static uint8_t GetWorkerCount();
	
private:
	void InitializeRenderPipeline();
	void InitializeComputePipelines();
	void InitializeStagingResources(uint8_t poolSize);

	void ReleaseRenderPipelines();
	void ReleaseComputePipelines();
	void ReleaseStagingResources();

	IUnityGraphicsVulkan* m_unityVulkan = nullptr;
	UnityVulkanInstance m_instance = {};
	VmaAllocator m_allocator = nullptr;

	//Testing
#define RENDER_CONST_STAGE_BIT VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
	RenderPipeline m_renderPipeline = {};
	VkDescriptorSet m_renderDSet = nullptr;
	VkDescriptorSetLayout m_formDSetLayout = nullptr;
	ComputePipeline m_surfaceAnalysisPipeline = {};
	ComputePipeline m_surfaceAssemblyPipeline = {};
	GPUBuffer m_surfaceAttributesBuffer = {};
	GPUImage m_surfaceColorSpecTex = {};
	GPUImage m_surfaceNrmHeightTex = {};

	MutexList<BodyRenderPackage> m_render;
	VkQueue m_occlusionQueue;
	std::mutex m_occlusionLock;

	WorkerResource* m_workers = nullptr;
	uint8_t m_workerCount = 0;

	QueueResource* m_queues = nullptr;
	uint8_t m_queueCount = 0;

	uint32_t m_computeQueueFamily = 0;

	LFPoolStack<ChunkStagingResources*>* m_stagingResources = nullptr;
	VkDescriptorPool m_stagingDescriptorPool = nullptr;

#define SAFE_DUMP_MARGIN 10
	typedef unsigned long long FrameNumber;
	MutexList<GPUResourceHandle*> m_loadingGarbage;
	std::vector<GPUResourceHandle*>  m_dumpingGarbage;
	std::atomic<FrameNumber> m_loadingFrame;
	std::atomic<FrameNumber> m_dumpFrame;
	FrameNumber m_dumpingFrame;
};

extern PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSet;