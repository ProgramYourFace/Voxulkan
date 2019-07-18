#pragma once
#include "glm/glm.hpp"
#include "..//Resources/GPUBuffer.h"
#include "..//Resources/GPUImage.h"
#include "..//Resources/ComputePipeline.h"

class Engine;

typedef enum VoxelChunkState {
	VC_STATE_PREINITIALIZE = 0,
	VC_STATE_VOLUME_FORMATION = 1,
	VC_STATE_VISUAL_FORMATION = 2,
	VC_STATE_IDLE = 3,
	VC_STATE_DELETE = DEALLOC_STATE
} VoxelChunkState;

struct VoxelChunk
{
	VoxelChunk();

	glm::vec3 m_min;
	glm::vec3 m_max;
	uint8_t m_subdivisionCount = 0;
	VoxelChunk* m_subChunks = nullptr;

	GPUImage m_densityImage;
	GPUBuffer m_indexBuffer;
	GPUBuffer m_vertexBuffer;
	uint32_t m_vertexCount;
	uint32_t m_indexCount;

	void SetMeshData(const GPUBuffer& vertexBuffer, const GPUBuffer& indexBuffer, uint32_t vertexCount, uint32_t indexCount);
	void ReleaseMesh(Engine* instance);
	void AllocateVolume(Engine* instance, const glm::uvec3& size);
	void ApplyForm(VkCommandBuffer commandBuffer,
		ComputePipeline* computePipeline,
		VkDescriptorSet resources,
		void* constants,
		uint32_t constantSize,
		const glm::vec3& min,
		const glm::vec3& max,
		uint8_t padding);
	void ExecuteAnalysisStage(VkCommandBuffer commandBuffer);
private:
	std::atomic<VoxelChunkState> m_chunkState = VC_STATE_PREINITIALIZE;
	float m_distance = 0.0f;
	bool operator<(const VoxelChunk& rhs)const
	{
		return m_distance < rhs.m_distance;
	}
};

struct SurfaceAttributes
{
	uint32_t cellCount;
	uint32_t vertexCount;
	uint32_t indexCount;
};

struct SurfacePipelineConstants
{
	glm::uvec3 base;
	alignas(16)glm::uvec3 range;
};

struct ChunkStagingResources : public GPUResourceHandle
{
	ChunkStagingResources(Engine* instance, uint8_t size, uint8_t padding);

	//Staging
	VkDescriptorSet m_formDSet = VK_NULL_HANDLE;
	VkDescriptorSet m_analysisDSet = VK_NULL_HANDLE;
	VkDescriptorSet m_assemblyDSet = VK_NULL_HANDLE;

	GPUImage m_colorMap = {};
	GPUImage m_indexMap = {};
	GPUBuffer m_cells = {};
	GPUBuffer m_attributes = {};
	GPUBuffer m_attributesSB = {};
	VkEvent m_analysisCompleteEvent = VK_NULL_HANDLE;

	//Output
	GPUImage m_density = {};
	GPUBuffer m_verticies = {};
	GPUBuffer m_indicies = {};

	void WriteDescriptors(Engine* instance,
		VkDescriptorSet formDSet,
		VkDescriptorSet analysisDSet,
		VkDescriptorSet assemblyDSet);

	void GetImageTransferBarriers(VkImageMemoryBarrier& colorBarrier, VkImageMemoryBarrier& indexBarrier);
	void Deallocate(Engine* instance) override;
};