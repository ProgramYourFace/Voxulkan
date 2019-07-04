#include "VoxelBody.h"

VoxelBody::VoxelBody(const glm::vec3& min, const glm::vec3& max)
{
	root.m_min = min;
	root.m_max = max;
}

void VoxelBody::Traverse(Engine* instance, const uint8_t& workerID)
{
}
