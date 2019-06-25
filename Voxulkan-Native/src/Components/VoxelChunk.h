#pragma once
#include "glm/glm.hpp"
#include "..//Resources/GBuffer.h"
#include "..//Resources/GImage.h"
#include "..//Resources/ComputePipeline.h"

typedef enum VoxelChunkState {
	VC_STATE_PREINITIALIZE = 0,
	VC_STATE_VOLUME_FORMATION = 1,
	VC_STATE_VISUAL_FORMATION = 2,
	VC_STATE_IDLE = 3,
	VC_STATE_DELETE = 0xFF
} VoxelChunkState;

struct VoxelChunk
{
	VoxelChunk();

	glm::vec3 m_min;
	glm::vec3 m_max;
	uint8_t m_subdivisionCount = 0;
	VoxelChunk* m_subChunks = nullptr;

	GImage m_volumeImage;
	GBuffer m_triangleBuffer;
	GBuffer m_vertexBuffer;
	GBuffer m_argsBuffer;

	void AllocateVolume(VmaAllocator allocator, const glm::uvec3& size, uint8_t padding);
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

struct ChunkStagingResources
{
	VoxelChunk* chunk;
	VkDescriptorPool m_pool;

	//Volume form stage
	VkDescriptorSet m_volumeFormDSet;

	//Surface analysis stage
	VkDescriptorSet m_surfaceAnalysisDSet;
	GBuffer m_surfaceCells;
	GBuffer m_attributes;
	VkEvent m_analysisCompleteEvent;

	//Vertex assembly stage
	VkDescriptorSet m_vertexAssemblyDSet;

	//Triangle assembly stage
	VkDescriptorSet m_triangleAssemblyDSet;
};