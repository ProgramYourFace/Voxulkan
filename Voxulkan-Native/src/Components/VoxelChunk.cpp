#include "VoxelChunk.h"
#include "..//Engine.h"
#include "..//Plugin.h"

VoxelChunk::VoxelChunk()
{
	m_densityImage.m_format = VK_FORMAT_R8_UNORM;
	m_densityImage.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_densityImage.m_type = VK_IMAGE_TYPE_3D;
	m_densityImage.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_densityImage.m_tiling = VK_IMAGE_TILING_OPTIMAL;
}

void VoxelChunk::SetMeshData(const GPUBuffer& vertexBuffer, const GPUBuffer& indexBuffer, uint32_t vertexCount, uint32_t indexCount)
{
	m_vertexBuffer = vertexBuffer;
	m_indexBuffer = indexBuffer;
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
}

void VoxelChunk::ReleaseMesh(Engine* instance)
{
	m_indexBuffer.Release(instance);
	m_vertexBuffer.Release(instance);
	m_indexCount = 0;
	m_vertexCount = 0;
}

void VoxelChunk::AllocateVolume(Engine* instance, const glm::uvec3& size)
{
	m_densityImage.m_size = { size.x, size.y , size.z };
	m_densityImage.Allocate(instance);
}

ChunkStagingResources::ChunkStagingResources(Engine* instance, uint8_t size, uint8_t padding)
{
	VkDevice device = instance->Device();

	VkEventCreateInfo eventCI = {};
	eventCI.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	vkCreateEvent(device, &eventCI, nullptr, &m_analysisCompleteEvent);

	m_attributes.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	m_attributes.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_attributes.m_byteCount = sizeof(SurfaceAttributes);
	m_attributes.Allocate(instance);
	
	m_attributesSB.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	m_attributesSB.m_memoryUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
	m_attributesSB.m_byteCount = sizeof(SurfaceAttributes);
	m_attributesSB.Allocate(instance);

	uint32_t sizeP1 = (uint32_t)size + 1;
	m_indexMap.m_size = { sizeP1 ,sizeP1 ,sizeP1 };
	m_indexMap.m_format = VK_FORMAT_R32_UINT;
	m_indexMap.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_indexMap.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_indexMap.m_type = VK_IMAGE_TYPE_3D;
	m_indexMap.Allocate(instance);

	uint32_t sizePad = sizeP1 + (uint32_t)padding * 2;
	m_colorMap.m_size = { sizePad ,sizePad ,sizePad };
	m_colorMap.m_format = VK_FORMAT_R8G8B8A8_UNORM;
	m_colorMap.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_colorMap.m_type = VK_IMAGE_TYPE_3D;
	m_colorMap.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_colorMap.Allocate(instance);

	m_cells.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	m_cells.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_cells.m_byteCount = (uint64_t)sizeP1 * sizeP1 * sizeP1 * sizeof(uint32_t) * 2;
	m_cells.Allocate(instance);

	m_verticies.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	m_verticies.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

	m_indicies.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	m_indicies.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

	m_density.m_format = VK_FORMAT_R8_UNORM;
	m_density.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_density.m_tiling = VK_IMAGE_TILING_OPTIMAL;
	m_density.m_type = VK_IMAGE_TYPE_3D;
	m_density.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
}

void ChunkStagingResources::WriteDescriptors(Engine* instance,
	VkDescriptorSet formDSet,
	VkDescriptorSet analysisDSet,
	VkDescriptorSet assemblyDSet)
{
	VkDevice device = instance->Device();
	m_formDSet = formDSet;
	m_analysisDSet = analysisDSet;
	m_assemblyDSet = assemblyDSet;

	VkDescriptorImageInfo colorMapW = { nullptr, m_colorMap.m_gpuHandle->m_view,  VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo indexMapW = { nullptr, m_indexMap.m_gpuHandle->m_view,  VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorBufferInfo cellsW = { m_cells.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo attributesW = { m_attributes.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };

	std::vector<VkWriteDescriptorSet> writes(9);
	//Form: Color map
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[0].dstArrayElement = 0;
	writes[0].dstBinding = 0;
	writes[0].dstSet = m_formDSet;
	writes[0].pImageInfo = &colorMapW;
	//Analysis: Color map;
	writes[1] = writes[0];
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;// Read only
	writes[1].dstSet = m_analysisDSet;
	writes[1].pImageInfo = &colorMapW;
	//Index map
	writes[2] = writes[1];
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writes[2].dstBinding = 1;
	writes[2].pImageInfo = &indexMapW;
	//Cells
	writes[3] = writes[1];
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[3].dstBinding = 2;
	writes[3].pImageInfo = nullptr;
	writes[3].pBufferInfo = &cellsW;
	//Attributes
	writes[4] = writes[3];
	writes[4].dstBinding = 3;
	writes[4].pBufferInfo = &attributesW;
	//Assembly: Color map
	writes[5] = writes[1];
	writes[5].dstSet = m_assemblyDSet;
	//Index map
	writes[6] = writes[5];
	writes[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;// Read only
	writes[6].dstBinding = 1;
	writes[6].pImageInfo = &indexMapW;
	//Cells
	writes[7] = writes[5];
	writes[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;// Read only
	writes[7].dstBinding = 2;
	writes[7].pImageInfo = nullptr;
	writes[7].pBufferInfo = &cellsW;
	//Vertex(3)/Index(4) buffers
	writes[8] = writes[7];
	writes[8].dstBinding = 5;
	writes[8].pBufferInfo = &attributesW;
	
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ChunkStagingResources::GetImageTransferBarriers(VkImageMemoryBarrier& colorBarrier, VkImageMemoryBarrier& indexBarrier)
{
	colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	colorBarrier.image = m_colorMap.m_gpuHandle->m_image;

	colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	colorBarrier.srcAccessMask = 0;
	colorBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	colorBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	colorBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorBarrier.subresourceRange.baseArrayLayer = 0;
	colorBarrier.subresourceRange.baseMipLevel = 0;
	colorBarrier.subresourceRange.layerCount = 1;
	colorBarrier.subresourceRange.levelCount = 1;

	indexBarrier = colorBarrier;
	indexBarrier.image = m_indexMap.m_gpuHandle->m_image;
}

void ChunkStagingResources::Deallocate(Engine* instance)
{
	VkDevice device = instance->Device();
	vkDestroyEvent(device, m_analysisCompleteEvent, nullptr);
	m_colorMap.m_gpuHandle->Deallocate(instance);
	m_indexMap.m_gpuHandle->Deallocate(instance);
	m_cells.m_gpuHandle->Deallocate(instance);
	m_attributes.m_gpuHandle->Deallocate(instance);
	m_attributesSB.m_gpuHandle->Deallocate(instance);
}
