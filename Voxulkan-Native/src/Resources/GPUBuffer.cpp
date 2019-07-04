#include "GPUBuffer.h"
#include "..\\Engine.h"
#include "..\\Plugin.h"


void GPUBuffer::UploadData(Engine* instance, void* data, const size_t& byteCount)
{
	VmaAllocator allocator = instance->Allocator();
	void* mappedData;
	vmaMapMemory(allocator, m_gpuHandle->m_allocation, &mappedData);
	memcpy(mappedData, data, byteCount);
	vmaUnmapMemory(allocator, m_gpuHandle->m_allocation);
}

void GPUBuffer::Allocate(Engine* instance)
{
	if (m_gpuHandle)
	{
		LOG("GPU buffer allocation failed! Already allocated!");
		return;
	}
	m_gpuHandle = new GPUBufferHandle();

	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = m_byteCount;
	bufferInfo.usage = m_bufferUsage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = m_memoryUsage;

	vmaCreateBuffer(instance->Allocator(), &bufferInfo, &allocInfo, &m_gpuHandle->m_buffer, &m_gpuHandle->m_allocation, nullptr);
}

void GPUBuffer::Release(Engine* instance)
{
	SAFE_DESTROY(m_gpuHandle);
}

void GPUBufferHandle::Deallocate(Engine* instance)
{
	if (m_buffer)
		vmaDestroyBuffer(instance->Allocator(), m_buffer, m_allocation);

	m_buffer = nullptr;
	m_allocation = nullptr;
}
