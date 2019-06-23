#include "GBuffer.h"
#include "..\\Engine.h"
#include "..\\Plugin.h"


void GBuffer::UploadData(VmaAllocator allocator, void* data, const size_t& byteCount)
{
	void* mappedData;
	vmaMapMemory(allocator, m_gpuHandle->m_allocation, &mappedData);
	memcpy(mappedData, data, byteCount);
	vmaUnmapMemory(allocator, m_gpuHandle->m_allocation);
}

void GBuffer::Allocate(VmaAllocator allocator)
{
	Release();
	m_gpuHandle = new GBufferHandle();

	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = m_byteCount;
	bufferInfo.usage = m_bufferUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = m_memoryUsage;

	vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_gpuHandle->m_buffer, &m_gpuHandle->m_allocation, nullptr);
}

void GBuffer::Release()
{
	SAFE_RELEASE_HANDLE(m_gpuHandle);
}

void GBufferHandle::Dispose(VmaAllocator allocator)
{
	if (m_buffer)
	{
		vmaDestroyBuffer(allocator, m_buffer, m_allocation);
	}

	m_buffer = nullptr;
	m_allocation = nullptr;
}
