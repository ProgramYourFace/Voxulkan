#pragma once
#include "GResource.h"

class Engine;

struct GBufferHandle : GResourceHandle
{
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VmaAllocation m_allocation = VK_NULL_HANDLE;

	void Dispose(VmaAllocator allocator) override;
};

class GBuffer : public GResource
{
public:
	inline void SetMemoryUsage(const VmaMemoryUsage& memoryUsage) { m_memoryUsage = memoryUsage; }
	inline void SetBufferUsage(const VkBufferUsageFlags& bufferUsage) { m_bufferUsage = bufferUsage; }
	inline void SetByteCount(const VkDeviceSize& byteCount) { m_byteCount = byteCount; }

	inline VkBuffer GetBuffer() { return m_gpuHandle ? m_gpuHandle->m_buffer : nullptr; }

	void UploadData(VmaAllocator allocator, void* data, const size_t& byteCount);

	void Allocate(VmaAllocator allocator) override;
	void Release() override;

	friend class Engine;
private:
	GBufferHandle* m_gpuHandle = nullptr;

	//CreateInfo
	VmaMemoryUsage m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	VkBufferUsageFlags m_bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	VkDeviceSize m_byteCount = 0;
};