#pragma once
#include "glm/glm.hpp"
#include "..//Resources/GPUBuffer.h"
#include "..//Resources/GPUImage.h"
#include "..//Resources/ComputePipeline.h"

class Engine;

struct ChunkRenderPackage
{
	GPUBufferHandle* vertexBuffer = nullptr;
	GPUBufferHandle* indexBuffer = nullptr;
	int indexCount = 0;
	glm::vec3 min = {};
	glm::vec3 max = {};

	float distance = 0.0f;
	bool operator<(const ChunkRenderPackage& rhs)const
	{
		return distance < rhs.distance;
	}

	bool FrustumTest(const glm::mat4x4 mvp);
};

struct SurfaceAnalysisInfo
{
	uint32_t cellCount;
	uint32_t vertexCount;
	uint32_t indexCount;
	glm::uvec3 max;
	glm::uvec3 min;
};

struct ChunkPipelineConstants
{
	glm::mat4x4 mvp = {};
	glm::mat4x4 model = {};
	glm::vec3 worldPosition = {};
	float tessellationFactor = 0.0f;
};

struct BodyForm
{
	glm::vec3 min;
	glm::vec3 max;
	ComputePipeline* formCompute;
};

struct FormConstants
{
	alignas(16)glm::uvec3 offset;
	alignas(16)glm::uvec3 range;
	float scale;
	alignas(16)glm::mat4x4 transform;
};

struct SurfaceAnalysisConstants
{
	glm::uvec3 base;
	alignas(16)glm::uvec3 range;
};

struct SurfaceAssemblyConstants
{
	alignas(16)glm::uvec3 base;
	alignas(16)glm::vec3 offset;
	alignas(16)glm::vec3 scale;
};

struct VoxelChunk
{
	friend class VoxelBody;

	VoxelChunk();

	glm::vec3 m_min = {};
	glm::vec3 m_max = {};
	std::vector<VoxelChunk> m_subChunks = {};

	GPUImage m_densityImage = {};
	GPUBuffer m_indexBuffer = {};
	GPUBuffer m_vertexBuffer = {};
	uint32_t m_vertexCount = 0;
	uint32_t m_indexCount = 0;
	glm::vec3 m_boundMin = {};
	glm::vec3 m_boundMax = {};

	void SetMeshData(const GPUBuffer& vertexBuffer, const GPUBuffer& indexBuffer, uint32_t vertexCount, uint32_t indexCount);
	void ReleaseResources(Engine* instance, std::vector<GPUResourceHandle*>& trash);
	void ReleaseStaging(Engine* instance);
	void ReleaseSubResources(Engine* instance, std::vector<GPUResourceHandle*>& trash);
	void AllocateVolume(Engine* instance, const glm::uvec3& size);
	inline void UpdateDistance(glm::vec3 observerPosition)
	{
		glm::vec3 delta = observerPosition - glm::clamp(observerPosition, m_min, m_max);
		m_distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
	}
	bool operator<(const VoxelChunk& rhs)const
	{
		return m_distance < rhs.m_distance;
	}
private:
	void Build(Engine* instance, VkCommandBuffer commandBuffer, float voxelSize, BodyForm* forms, uint32_t formsCount, std::vector<GPUResourceHandle*>& trash);

	bool m_built = false;
	uint16_t m_stagingIdx = 0xFFFF;
	float m_distance = 0.0f;
};

typedef enum ChunkStage
{
	CHUNK_STAGE_IDLE = 0,
	CHUNK_STAGE_VOLUME_ANALYSIS = 1,
	CHUNK_STAGE_VISUAL_ASSEMBLY = 2,
} VoxelChunkState;

struct ChunkStagingResources : public GPUResourceHandle
{
	ChunkStagingResources(Engine* instance, uint8_t size, uint8_t padding);

	ChunkStage m_stage = CHUNK_STAGE_IDLE;

	//Staging
	VkDescriptorSet m_formDSet = VK_NULL_HANDLE;
	VkDescriptorSet m_analysisDSet = VK_NULL_HANDLE;
	VkDescriptorSet m_assemblyDSet = VK_NULL_HANDLE;

	GPUImage m_colorMap = {};
	GPUImage m_indexMap = {};
	GPUBuffer m_cells = {};
	GPUBuffer m_info = {};
	GPUBuffer m_infoStaging = {};
	VkEvent m_analysisCompleteEvent = VK_NULL_HANDLE;
	VkEvent m_assemblyCompleteEvent = VK_NULL_HANDLE;

	//Output
	GPUImage m_density = {};
	GPUBuffer m_verticies = {};
	GPUBuffer m_indicies = {};
	uint32_t m_vertexCount = 0;
	uint32_t m_indexCount = 0;
	glm::uvec3 m_boundsMin = {};
	glm::uvec3 m_boundsMax = {};

	void WriteDescriptors(Engine* instance,
		VkDescriptorSet formDSet,
		VkDescriptorSet analysisDSet,
		VkDescriptorSet assemblyDSet);
	void GetImageTransferBarriers(VkImageMemoryBarrier& colorBarrier, VkImageMemoryBarrier& indexBarrier);
	void Deallocate(Engine* instance) override;
	bool Ready(Engine* instance, VkCommandBuffer commandBuffer);
	inline void Reset() { m_reset = true; }
private:
	bool m_reset = false;
};