#pragma once
#include "VoxelChunk.h"
#include "glm/vec3.hpp"

struct BodyRenderPackage
{
	std::vector<ChunkRenderPackage> chunks = {};
	glm::mat4x4 transform = {};
	glm::vec3 min = {};
	glm::vec3 max = {};

	float distance = 0.0f;
	bool operator<(const BodyRenderPackage& rhs)const
	{
		return distance < rhs.distance;
	}
};

class VoxelBody
{
public:
	VoxelBody(const glm::vec3& min, const glm::vec3& max);
	
	void Traverse(Engine* instance, const glm::vec3& observerPosition, float E, float voxelSize, BodyForm* forms, uint32_t formsCount, uint32_t maxDepth = 10);
	void Deallocate(Engine* instance);

	volatile glm::mat4x4 m_transform = {};
private:
	void RenderChunk(const VoxelChunk& chunk, std::vector<ChunkRenderPackage>& render, glm::vec3& min, glm::vec3& max);
	void RenderDanglingBranches(Engine* instance, VoxelChunk& chunk, std::vector<ChunkRenderPackage>& render, glm::vec3& min, glm::vec3& max);
	VoxelChunk m_root = {};
	size_t m_lastRenderSize = 0;
	std::vector<GPUResourceHandle*>* m_trash;
};