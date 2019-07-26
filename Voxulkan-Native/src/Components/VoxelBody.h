#pragma once
#include "VoxelChunk.h"
#include "glm/vec3.hpp"

struct GPUBodyConstants : GPUResourceHandle
{
	volatile glm::mat4x4 modelMatrix;

	void Deallocate(Engine* instance) override {};
};

struct BodyRenderPackage
{
	std::vector<ChunkRenderPackage> m_chunks;
	GPUBodyConstants* m_constants;
};

class VoxelBody
{
public:
	VoxelBody(const glm::vec3& min, const glm::vec3& max);
	
	void AddChunkToRender(const VoxelChunk& chunk, std::vector<ChunkRenderPackage>& render, bool includeSub = false);
	void Traverse(Engine* instance, const uint8_t& workerID, const glm::vec3& observerPosition, float E, float leafSize, ComputePipeline* form, int maxDepth = 10);
	void Deallocate(Engine* instance);
private:
	VoxelChunk m_root = {};
	GPUBodyConstants* m_GPUConstants;
	size_t m_lastRenderSize = 0;
};