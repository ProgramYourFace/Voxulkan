#pragma once
#include "VoxelChunk.h"
#include "glm/vec3.hpp"

class VoxelBody
{
public:
	VoxelBody(const glm::vec3& min, const glm::vec3& max);

	void Traverse(Engine* instance, const uint8_t& workerID);
private:
	VoxelChunk root = {};
};