#include "VoxelChunk.h"
#include "..//Engine.h"
#include "..//Plugin.h"

VoxelChunk::VoxelChunk()
{
	m_densityImage.m_format = VK_FORMAT_R8_UNORM;
	m_densityImage.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_densityImage.m_type = VK_IMAGE_TYPE_3D;
	m_densityImage.m_viewType = VK_IMAGE_VIEW_TYPE_3D;
	m_densityImage.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_densityImage.m_tiling = VK_IMAGE_TILING_OPTIMAL;
	m_stagingIdx = POOL_QUEUE_END;
}

void VoxelChunk::SetMeshData(const GPUBuffer& vertexBuffer, const GPUBuffer& indexBuffer, uint32_t vertexCount, uint32_t indexCount)
{
	m_vertexBuffer = vertexBuffer;
	m_indexBuffer = indexBuffer;
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
}

void VoxelChunk::ReleaseResources(Engine* instance, bool self, bool includeSub)
{
	if (self)
	{
		m_densityImage.Release(instance);
		m_indexBuffer.Release(instance);
		m_vertexBuffer.Release(instance);
		m_indexCount = 0;
		m_vertexCount = 0;
		if (m_stagingIdx != POOL_QUEUE_END)
		{
			instance->m_stagingResources->enqueue(m_stagingIdx);
			m_stagingIdx = POOL_QUEUE_END;
		}
	}

	if (includeSub)
	{
		for (size_t i = 0; i < m_subChunks.size(); i++)
			m_subChunks[i].ReleaseResources(instance, true, true);

		m_subChunks.clear();
	}
}

void VoxelChunk::AllocateVolume(Engine* instance, const glm::uvec3& size)
{
	m_densityImage.m_size = { size.x, size.y , size.z };
	m_densityImage.Allocate(instance);
}

void VoxelChunk::BeginBuilding(Engine* instance, VkCommandBuffer commandBuffer, ComputePipeline* form)
{
	m_stagingIdx = instance->m_stagingResources->dequeue();
	if (m_stagingIdx == POOL_QUEUE_END)
		return;

	ChunkStagingResources* stage = (*instance->m_stagingResources)[m_stagingIdx];
	stage->CmdPrepare(commandBuffer);
	stage->m_stage = CHUNK_STAGE_VOLUME_ANALYSIS;
	
	std::vector<VkImageMemoryBarrier> imageMemBs(2);
	imageMemBs[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemBs[0].image = stage->m_colorMap.m_gpuHandle->m_image;
	imageMemBs[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemBs[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemBs[0].srcAccessMask = 0;
	imageMemBs[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemBs[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemBs[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemBs[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemBs[0].subresourceRange.baseArrayLayer = 0;
	imageMemBs[0].subresourceRange.baseMipLevel = 0;
	imageMemBs[0].subresourceRange.layerCount = 1;
	imageMemBs[0].subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, imageMemBs.data());

	VkExtent3D res = stage->m_colorMap.m_size;//TODO: Correct for actual size of mesh
	VkPipeline formPipeline;
	VkPipelineLayout formLayout;
	form->GetVkPipeline(formPipeline, formLayout);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, formPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, formLayout, 0, 1, &stage->m_formDSet, 0, nullptr);
	vkCmdDispatch(commandBuffer,
		(uint32_t)std::ceilf(res.width / 4.0f),
		(uint32_t)std::ceilf(res.height / 4.0f),
		(uint32_t)std::ceilf(res.depth / 4.0f));

	imageMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemBs[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBs[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemBs[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		0, nullptr,
		1, imageMemBs.data());

	VkPipeline analysisPipeline;
	VkPipelineLayout analysisPipelineLayout;
	instance->m_surfaceAnalysisPipeline.GetVkPipeline(analysisPipeline, analysisPipelineLayout);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, analysisPipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, analysisPipelineLayout, 0, 1, &stage->m_analysisDSet, 0, nullptr);
	SurfaceAnalysisConstants surfConsts = {};
	surfConsts.base = glm::uvec3(Engine::CHUNK_PADDING, Engine::CHUNK_PADDING, Engine::CHUNK_PADDING);
	surfConsts.range = glm::uvec3(Engine::CHUNK_SIZE, Engine::CHUNK_SIZE, Engine::CHUNK_SIZE);//TODO: Update to actual size of chunk
	vkCmdPushConstants(commandBuffer, analysisPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SurfaceAnalysisConstants), &surfConsts);
	vkCmdDispatch(commandBuffer, 
		(uint32_t)std::ceilf((surfConsts.range.x + 1) / 4.0f),
		(uint32_t)std::ceilf((surfConsts.range.y + 1) / 4.0f),
		(uint32_t)std::ceilf((surfConsts.range.z + 1) / 4.0f));

	std::vector<VkBufferMemoryBarrier> bufferMemBs(2);
	bufferMemBs[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemBs[0].buffer = stage->m_attributes.m_gpuHandle->m_buffer;
	bufferMemBs[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferMemBs[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferMemBs[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	bufferMemBs[0].offset = 0;
	bufferMemBs[0].size = VK_WHOLE_SIZE;
	bufferMemBs[1] = bufferMemBs[0];
	bufferMemBs[1].buffer = stage->m_cells.m_gpuHandle->m_buffer;
	bufferMemBs[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	imageMemBs[1] = imageMemBs[0];
	imageMemBs[1].image = stage->m_indexMap.m_gpuHandle->m_image;
	imageMemBs[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	imageMemBs[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		1, &bufferMemBs[1],
		1, &imageMemBs[1]);

	bufferMemBs[1].buffer = stage->m_attributesSB.m_gpuHandle->m_buffer;
	bufferMemBs[1].srcAccessMask = 0;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	/*imageMemBs[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemBs[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	imageMemBs[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageMemBs[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;*/
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		2, bufferMemBs.data(),
		0, nullptr);
	VkBufferCopy attribCopy = {};
	attribCopy.size = sizeof(SurfaceAttributes);
	attribCopy.dstOffset = 0;
	attribCopy.srcOffset = 0;
	vkCmdCopyBuffer(commandBuffer, stage->m_attributes.m_gpuHandle->m_buffer, stage->m_attributesSB.m_gpuHandle->m_buffer, 1, &attribCopy);

	bufferMemBs[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	bufferMemBs[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	bufferMemBs[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufferMemBs[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		2, bufferMemBs.data(),
		0, nullptr);
	
	vkCmdSetEvent(commandBuffer, stage->m_analysisComplete, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void VoxelChunk::ProcessBuilding(Engine* instance, VkCommandBuffer commandBuffer)
{
	VkDevice device = instance->Device();
	ChunkStagingResources* stage = (*instance->m_stagingResources)[m_stagingIdx];

	if (stage->m_stage == CHUNK_STAGE_VOLUME_ANALYSIS)
	{
		if (vkGetEventStatus(device, stage->m_analysisComplete) == VK_EVENT_RESET)
			return;
		vkResetEvent(device, stage->m_analysisComplete);

		VmaAllocator allocator = instance->Allocator();
		void* attribData;
		vmaMapMemory(allocator, stage->m_attributesSB.m_gpuHandle->m_allocation, &attribData);
		SurfaceAttributes surfaceAttribs = {};
		std::memcpy(&surfaceAttribs, attribData, sizeof(SurfaceAttributes));
		vmaUnmapMemory(allocator, stage->m_attributesSB.m_gpuHandle->m_allocation);

		if (surfaceAttribs.cellCount > 0)
		{
			stage->m_stage = CHUNK_STAGE_VISUAL_ASSEMBLY;
			stage->m_vertexCount = surfaceAttribs.vertexCount;
			stage->m_indexCount = surfaceAttribs.indexCount;

			stage->m_verticies.Dereference();
			stage->m_verticies.m_byteCount = surfaceAttribs.vertexCount * 16LL;
			stage->m_verticies.Allocate(instance);

			stage->m_indicies.Dereference();
			stage->m_indicies.m_byteCount = surfaceAttribs.indexCount * 4LL;
			stage->m_indicies.Allocate(instance);

			VkDescriptorBufferInfo vertBI = { stage->m_verticies.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };
			VkDescriptorBufferInfo idxsBI = { stage->m_indicies.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };

			std::vector<VkWriteDescriptorSet> descWrites(2);
			descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descWrites[0].descriptorCount = 1;
			descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descWrites[0].dstArrayElement = 0;
			descWrites[0].dstBinding = 3;
			descWrites[0].dstSet = stage->m_assemblyDSet;
			descWrites[0].pBufferInfo = &vertBI;
			descWrites[1] = descWrites[0];
			descWrites[1].dstBinding = 4;
			descWrites[1].pBufferInfo = &idxsBI;
			vkUpdateDescriptorSets(device, 2, descWrites.data(), 0, nullptr);

			VkPipeline assemblyPipeline;
			VkPipelineLayout assemblyPipelineLayout;
			instance->m_surfaceAssemblyPipeline.GetVkPipeline(assemblyPipeline, assemblyPipelineLayout);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipelineLayout, 0, 1, &stage->m_assemblyDSet, 0, nullptr);
			SurfaceAssemblyConstants surfConsts = {};
			surfConsts.base = glm::uvec3(Engine::CHUNK_PADDING, Engine::CHUNK_PADDING, Engine::CHUNK_PADDING);
			surfConsts.offset = m_min;
			surfConsts.scale = (m_max - m_min) / (float)Engine::CHUNK_SIZE;
			vkCmdPushConstants(commandBuffer, assemblyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SurfaceAssemblyConstants), &surfConsts);
			vkCmdDispatch(commandBuffer, (uint32_t)std::ceilf(surfaceAttribs.cellCount / 64.0f), 1, 1);

			std::vector<VkBufferMemoryBarrier> bufferMemBs(2);
			bufferMemBs[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemBs[0].buffer = stage->m_verticies.m_gpuHandle->m_buffer;
			bufferMemBs[0].srcQueueFamilyIndex = instance->m_computeQueueFamily;
			bufferMemBs[0].dstQueueFamilyIndex = instance->m_instance.queueFamilyIndex;
			bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferMemBs[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			bufferMemBs[0].offset = 0;
			bufferMemBs[0].size = VK_WHOLE_SIZE;

			bufferMemBs[1] = bufferMemBs[0];
			bufferMemBs[1].buffer = stage->m_indicies.m_gpuHandle->m_buffer;
			bufferMemBs[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferMemBs[1].dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				2, bufferMemBs.data(),
				0, nullptr);

			stage->CmdClearAttributes(commandBuffer);
			vkCmdSetEvent(commandBuffer, stage->m_assemblyComplete, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
		else
		{
			stage->m_stage = CHUNK_STAGE_IDLE;
			stage->CmdClearAttributes(commandBuffer);
			ReleaseResources(instance, true, true);
			m_built = true;
		}
	}
	else if (stage->m_stage == CHUNK_STAGE_VISUAL_ASSEMBLY)
	{
		if (vkGetEventStatus(device, stage->m_assemblyComplete) == VK_EVENT_RESET)
			return;
		vkResetEvent(device, stage->m_assemblyComplete);

		m_vertexBuffer.Release(instance);
		m_indexBuffer.Release(instance);
		m_vertexBuffer = stage->m_verticies;
		m_indexBuffer = stage->m_indicies;
		m_vertexCount = stage->m_vertexCount;
		m_indexCount = stage->m_indexCount;
		stage->m_verticies.Dereference();
		stage->m_indicies.Dereference();
		stage->m_stage = CHUNK_STAGE_IDLE;

		instance->m_stagingResources->enqueue(m_stagingIdx);
		m_stagingIdx = POOL_QUEUE_END;
		m_built = true;
		//LOG(instance->m_stagingResources->getPtrsString());
	}
}

ChunkStagingResources::ChunkStagingResources(Engine* instance, uint8_t size, uint8_t padding)
{
	VkDevice device = instance->Device();

	VkEventCreateInfo eventCI = {};
	eventCI.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	vkCreateEvent(device, &eventCI, nullptr, &m_analysisComplete);
	vkCreateEvent(device, &eventCI, nullptr, &m_assemblyComplete);

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
	m_indexMap.m_viewType = VK_IMAGE_VIEW_TYPE_3D;
	m_indexMap.Allocate(instance);

	uint32_t sizePad = sizeP1 + (uint32_t)padding * 2;
	m_colorMap.m_size = { sizePad ,sizePad ,sizePad };
	m_colorMap.m_format = VK_FORMAT_R8G8_UINT;
	m_colorMap.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_colorMap.m_type = VK_IMAGE_TYPE_3D;
	m_colorMap.m_viewType = VK_IMAGE_VIEW_TYPE_3D;
	m_colorMap.m_usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
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
	m_density.m_viewType = VK_IMAGE_VIEW_TYPE_3D;
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

void ChunkStagingResources::CmdPrepare(VkCommandBuffer commandBuffer)
{
	if (m_stage == CHUNK_STAGE_VOLUME_ANALYSIS)
	{
		vkCmdWaitEvents(commandBuffer, 1, &m_analysisComplete,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, nullptr,
			0, nullptr,
			0, nullptr);
		vkCmdResetEvent(commandBuffer, m_analysisComplete, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		CmdClearAttributes(commandBuffer);
	}
	else if (m_stage == CHUNK_STAGE_VISUAL_ASSEMBLY)
	{
		vkCmdWaitEvents(commandBuffer, 1, &m_assemblyComplete,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, nullptr,
			0, nullptr,
			0, nullptr);
		vkCmdResetEvent(commandBuffer, m_assemblyComplete, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		CmdClearAttributes(commandBuffer);
	}
	m_stage = CHUNK_STAGE_IDLE;
}

void ChunkStagingResources::CmdClearAttributes(VkCommandBuffer commandBuffer)
{
	VkBufferMemoryBarrier memB = {};
	memB.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	memB.buffer = m_attributes.m_gpuHandle->m_buffer;
	memB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memB.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	memB.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memB.offset = 0;
	memB.size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, nullptr,
		1, &memB,
		0, nullptr);
	vkCmdFillBuffer(commandBuffer, memB.buffer, 0, VK_WHOLE_SIZE, 0);
	memB.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memB.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		0,
		0, nullptr,
		1, &memB,
		0, nullptr);
}

void ChunkStagingResources::Deallocate(Engine* instance)
{
	VkDevice device = instance->Device();
	vkDestroyEvent(device, m_analysisComplete, nullptr);
	vkDestroyEvent(device, m_assemblyComplete, nullptr);
	m_colorMap.m_gpuHandle->Deallocate(instance);
	m_indexMap.m_gpuHandle->Deallocate(instance);
	m_cells.m_gpuHandle->Deallocate(instance);
	m_attributes.m_gpuHandle->Deallocate(instance);
	m_attributesSB.m_gpuHandle->Deallocate(instance);
}