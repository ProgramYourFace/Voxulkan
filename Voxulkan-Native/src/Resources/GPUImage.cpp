#include "GPUImage.h"
#include "..//Plugin.h"
#include "..//Engine.h"

void GPUImageHandle::Deallocate(Engine* instance)
{
	if (m_view)
		vkDestroyImageView(instance->Device(), m_view, nullptr);
	if (m_sampler)
		vkDestroySampler(instance->Device(), m_sampler, nullptr);
	if (m_image)
		vmaDestroyImage(instance->Allocator(), m_image, m_allocation);

	m_sampler = nullptr;
	m_image = nullptr;
	m_allocation = nullptr;
}

void GPUImage::Allocate(Engine* instance)
{
	if (m_gpuHandle)
	{
		LOG("GPU image allocation failed! Already allocated!");
		return;
	}
	m_gpuHandle = new GPUImageHandle();

	VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.flags = 0;
	imageInfo.imageType = m_type;
	imageInfo.arrayLayers = m_arraySize;
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

	VK_CALL(vmaCreateImage(instance->Allocator(), &imageInfo, &allocInfo, &m_gpuHandle->m_image, &m_gpuHandle->m_allocation, nullptr));

	if (m_createView)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_gpuHandle->m_image;
		viewInfo.viewType = m_viewType;
		viewInfo.format = m_format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = m_arraySize;

		VK_CALL(vkCreateImageView(instance->Device(), &viewInfo, nullptr, &m_gpuHandle->m_view));
	}

	if (m_createSampler)
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = wrapMode;
		samplerInfo.addressModeV = wrapMode;
		samplerInfo.addressModeW = wrapMode;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.anisotropyEnable = m_maxAnistrophy < 1.0f ? VK_FALSE : VK_TRUE;
		samplerInfo.maxAnisotropy = m_maxAnistrophy;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		VK_CALL(vkCreateSampler(instance->Device(), &samplerInfo, nullptr, &m_gpuHandle->m_sampler));
	}
}

void GPUImage::Release(Engine* instance)
{
	SAFE_DESTROY(m_gpuHandle);
}
