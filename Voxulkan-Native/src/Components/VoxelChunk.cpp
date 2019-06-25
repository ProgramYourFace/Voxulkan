#include "VoxelChunk.h"

VoxelChunk::VoxelChunk()
{
	m_volumeImage.m_format = VK_FORMAT_R8G8B8A8_UNORM;
	m_volumeImage.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_volumeImage.m_type = VK_IMAGE_TYPE_3D;
	m_volumeImage.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_volumeImage.m_tiling = VK_IMAGE_TILING_OPTIMAL;
}

void VoxelChunk::AllocateVolume(VmaAllocator allocator, const glm::uvec3& size, uint8_t padding)
{
	uint32_t p = ((uint32_t)padding * 2);
	m_volumeImage.m_size = { p + size.x, p + size.y , p + size.z };
	m_volumeImage.Allocate(allocator);
}
