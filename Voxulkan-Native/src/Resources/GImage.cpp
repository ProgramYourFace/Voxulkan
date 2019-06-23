#include "GImage.h"
#include "..//Plugin.h"
#include "..//Engine.h"

void GImageHandle::Dispose(VmaAllocator allocator)
{
	if (m_view)
	{
		vkDestroyImageView(vmaGetAllocatorDevice(allocator), m_view, nullptr);
	}
	if (m_image)
		vmaDestroyImage(allocator, m_image, m_allocation);

	m_image = nullptr;
	m_allocation = nullptr;
}

void GImage::Allocate(VmaAllocator allocator)
{
	Release();
	m_gpuHandle = new GImageHandle();

	VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.flags = 0;
	imageInfo.imageType = m_type;
	imageInfo.arrayLayers = 1;
	imageInfo.mipLevels = 1;
	imageInfo.extent = m_size;
	imageInfo.format = m_format;
	imageInfo.tiling = m_tiling;
	imageInfo.usage = m_usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = m_memoryUsage;

	VK_CALL(vmaCreateImage(allocator, &imageInfo, &allocInfo, &m_gpuHandle->m_image, &m_gpuHandle->m_allocation, nullptr));

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_gpuHandle->m_image;
	viewInfo.viewType = static_cast<VkImageViewType>(m_type);
	viewInfo.format = m_format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	
	VkDevice device = vmaGetAllocatorDevice(allocator);
	VK_CALL(vkCreateImageView(device, &viewInfo, nullptr, &m_gpuHandle->m_view));
}

void GImage::Release()
{
	SAFE_RELEASE_HANDLE(m_gpuHandle);
}
