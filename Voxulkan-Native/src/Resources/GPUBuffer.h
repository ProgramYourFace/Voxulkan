#pragma once
#include "GPUResource.h"

struct GPUBufferHandle : GPUResourceHandle
{
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

	void Deallocate(Engine* instance) override;
};

class GPUBuffer : public GPUResource
{
public:
	void UploadData(Engine* instance, void* data, const size_t& byteCount);

	void Allocate(Engine* instance) override;
	void Release(Engine* instance) override;
	inline VkBuffer GetVk() { return m_gpuHandle ? m_gpuHandle->m_buffer : nullptr; }
	inline void Dereference() { m_gpuHandle = nullptr; }

	GPUBufferHandle* m_gpuHandle = nullptr;

	//CreateInfo
	VmaMemoryUsage m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	VkBufferUsageFlags m_bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkDeviceSize m_byteCount = 0;
};