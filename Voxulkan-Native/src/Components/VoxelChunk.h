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

	GPUImage m_volumeImage;
	GPUBuffer m_triangleBuffer;
	GPUBuffer m_vertexBuffer;
	GPUBuffer m_argsBuffer;

	void AllocateVolume(Engine* instance, const glm::uvec3& size, uint8_t padding);
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

	void AllocateDescriptors(Engine* instance,
		VkDescriptorPool descriptorPool,
		VkDescriptorSetLayout formDSetLayout,
		VkDescriptorSetLayout analysisDSetLayout,
		VkDescriptorSetLayout assemblyDSetLayout);
	void Deallocate(Engine* instance) override;
};