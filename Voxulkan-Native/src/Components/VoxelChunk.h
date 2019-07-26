#pragma once
#include "glm/glm.hpp"
#include "..//Resources/GPUBuffer.h"
#include "..//Resources/GPUImage.h"
#include "..//Resources/ComputePipeline.h"

class Engine;

struct ChunkRenderPackage
{
	GPUBufferHandle* m_vertexBuffer = nullptr;
	GPUBufferHandle* m_indexBuffer = nullptr;
	int m_indexCount = 0;
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

	void SetMeshData(const GPUBuffer& vertexBuffer, const GPUBuffer& indexBuffer, uint32_t vertexCount, uint32_t indexCount);
	void ReleaseResources(Engine* instance, bool self = true, bool includeSub = false);
	void AllocateVolume(Engine* instance, const glm::uvec3& size);
	void ApplyForm(VkCommandBuffer commandBuffer,
		ComputePipeline* computePipeline,
		VkDescriptorSet resources,
		void* constants,
		uint32_t constantSize,
		const glm::vec3& min,
		const glm::vec3& max,
		uint8_t padding);
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
	void BeginBuilding(Engine* instance, VkCommandBuffer commandBuffer, ComputePipeline* form);
	void ProcessBuilding(Engine* instance, VkCommandBuffer commandBuffer);

	bool m_built = false;
	uint16_t m_stagingIdx;
	float m_distance = 0.0f;
};

struct SurfaceAttributes
{
	uint32_t cellCount;
	uint32_t vertexCount;
	uint32_t indexCount;
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
	GPUBuffer m_attributes = {};
	GPUBuffer m_attributesSB = {};
	VkEvent m_analysisComplete = VK_NULL_HANDLE;
	VkEvent m_assemblyComplete = VK_NULL_HANDLE;

	//Output
	GPUImage m_density = {};
	GPUBuffer m_verticies = {};
	GPUBuffer m_indicies = {};
	uint32_t m_vertexCount = 0;
	uint32_t m_indexCount = 0;

	void WriteDescriptors(Engine* instance,
		VkDescriptorSet formDSet,
		VkDescriptorSet analysisDSet,
		VkDescriptorSet assemblyDSet);
	void GetImageTransferBarriers(VkImageMemoryBarrier& colorBarrier, VkImageMemoryBarrier& indexBarrier);
	void CmdPrepare(VkCommandBuffer commandBuffer);
	void CmdClearAttributes(VkCommandBuffer commandBuffer);
	void Deallocate(Engine* instance) override;
};