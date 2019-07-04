#include "VoxelChunk.h"
#include "..//Engine.h"
#include "..//Plugin.h"

VoxelChunk::VoxelChunk()
{
	m_volumeImage.m_format = VK_FORMAT_R8G8B8A8_UNORM;
	m_volumeImage.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_volumeImage.m_type = VK_IMAGE_TYPE_3D;
	m_volumeImage.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_volumeImage.m_tiling = VK_IMAGE_TILING_OPTIMAL;
}

void VoxelChunk::AllocateVolume(Engine* instance, const glm::uvec3& size, uint8_t padding)
{
	uint32_t p = ((uint32_t)padding * 2);
	m_volumeImage.m_size = { p + size.x, p + size.y , p + size.z };
	m_volumeImage.Allocate(instance);
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
	
	m_attributesSB.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	m_attributesSB.m_memoryUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
	m_attributesSB.m_byteCount = sizeof(SurfaceAttributes);
	m_attributesSB.Allocate(instance);

	uint8_t sizeP1 = size + 1;
	m_indexMap.m_size = { sizeP1 ,sizeP1 ,sizeP1 };
	m_indexMap.m_format = VK_FORMAT_R32_UINT;
	m_indexMap.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_indexMap.m_type = VK_IMAGE_TYPE_3D;
	m_indexMap.Allocate(instance);

	uint8_t sizePad = sizeP1 + padding * 2;
	m_colorMap.m_size = { sizePad ,sizePad ,sizePad };
	m_colorMap.m_format = VK_FORMAT_R8G8B8A8_UNORM;
	m_colorMap.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_colorMap.m_type = VK_IMAGE_TYPE_3D;
	m_colorMap.Allocate(instance);

	VkDeviceSize s = (VkDeviceSize)size;
	m_cells.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	m_cells.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_cells.m_byteCount = s*s*s*8;
	m_cells.Allocate(instance);
}

void ChunkStagingResources::AllocateDescriptors(Engine* instance,
	VkDescriptorPool descriptorPool,
	VkDescriptorSetLayout formDSetLayout,
	VkDescriptorSetLayout analysisDSetLayout,
	VkDescriptorSetLayout assemblyDSetLayout)
{
	VkDevice device = instance->Device();

	std::vector<VkDescriptorSetLayout> layouts(3);
	layouts[0] = formDSetLayout;
	layouts[1] = analysisDSetLayout;
	layouts[2] = assemblyDSetLayout;

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();

	std::vector<VkDescriptorSet> sets(3);
	VK_CALL(vkAllocateDescriptorSets(device, &allocInfo, sets.data()));

	m_formDSet = sets[0];
	m_analysisDSet = sets[1];
	m_assemblyDSet = sets[2];

	VkDescriptorImageInfo colorMapW = { nullptr, m_colorMap.m_gpuHandle->m_view,  VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo colorMapR = { nullptr, m_colorMap.m_gpuHandle->m_view,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	VkDescriptorImageInfo indexMapW = { nullptr, m_indexMap.m_gpuHandle->m_view,  VK_IMAGE_LAYOUT_GENERAL };
	VkDescriptorImageInfo indexMapR = { nullptr, m_indexMap.m_gpuHandle->m_view,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
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
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	writes[1].dstSet = m_analysisDSet;
	writes[1].pImageInfo = &colorMapR;
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
	writes[6].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	writes[6].dstBinding = 1;
	writes[6].pImageInfo = &indexMapR;
	//Cells
	writes[7] = writes[5];
	writes[7].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[7].dstBinding = 2;
	writes[7].pImageInfo = nullptr;
	writes[7].pBufferInfo = &cellsW;
	//Attributes
	writes[8] = writes[7];
	writes[8].dstBinding = 3;
	writes[8].pBufferInfo = &attributesW;
	
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ChunkStagingResources::Deallocate(Engine* instance)
{
	VkDevice device = instance->Device();
	vkDestroyEvent(device, m_analysisCompleteEvent, nullptr);
	m_indexMap.m_gpuHandle->Deallocate(instance);
	m_cells.m_gpuHandle->Deallocate(instance);
	m_attributes.m_gpuHandle->Deallocate(instance);
	m_attributesSB.m_gpuHandle->Deallocate(instance);
}
