#pragma once
#include "GResource.h"

class Engine;

struct GImageHandle : GResourceHandle
{
	VkImage m_image = VK_NULL_HANDLE;
	VkImageView m_view = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

	void Dispose(VmaAllocator allocator) override;
};

class GImage : public GResource
{
public:

	inline VkImage GetImage() { return m_gpuHandle ? m_gpuHandle->m_image : nullptr; }

	void Allocate(VmaAllocator allocator) override;
	void Release() override;

	GImageHandle* m_gpuHandle = nullptr;

	//CreateInfo
	VmaMemoryUsage m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	VkImageUsageFlags m_usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	VkImageType m_type = VK_IMAGE_TYPE_2D;
	VkFormat m_format = VK_FORMAT_R8G8B8A8_UNORM;
	VkImageTiling m_tiling = VK_IMAGE_TILING_OPTIMAL;
	VkExtent3D m_size = {0,0,0};
};