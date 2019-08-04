#include "VoxelChunk.h"
#include "..//Engine.h"
#include "..//Plugin.h"
#include <algorithm>
#include <stdlib.h>
#include <glm/gtx/transform.hpp>

VoxelChunk::VoxelChunk()
{
	m_densityImage.m_format = VK_FORMAT_R8_UNORM;
	m_densityImage.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_densityImage.m_type = VK_IMAGE_TYPE_3D;
	m_densityImage.m_viewType = VK_IMAGE_VIEW_TYPE_3D;
	m_densityImage.m_usage = VK_IMAGE_USAGE_STORAGE_BIT;
	m_densityImage.m_tiling = VK_IMAGE_TILING_OPTIMAL;
	m_stagingIdx = POOL_STACK_END;
}

void VoxelChunk::SetMeshData(const GPUBuffer& vertexBuffer, const GPUBuffer& indexBuffer, uint32_t vertexCount, uint32_t indexCount)
{
	m_vertexBuffer = vertexBuffer;
	m_indexBuffer = indexBuffer;
	m_vertexCount = vertexCount;
	m_indexCount = indexCount;
}

#define SAFE_TRASH(res) if(res.m_gpuHandle) trash.push_back(res.m_gpuHandle); res.m_gpuHandle=nullptr;

void VoxelChunk::ReleaseResources(Engine* instance, std::vector<GPUResourceHandle*>& trash)
{
	/*
	m_densityImage.Release(instance);
	m_indexBuffer.Release(instance);
	m_vertexBuffer.Release(instance);*/
	size_t space = trash.capacity() - trash.size();
	if (space < 3)
		trash.reserve(3 - space);


	SAFE_TRASH(m_densityImage);
	SAFE_TRASH(m_indexBuffer);
	SAFE_TRASH(m_vertexBuffer);

	m_indexCount = 0;
	m_vertexCount = 0;
	ReleaseStaging(instance);
}

void VoxelChunk::ReleaseStaging(Engine* instance)
{
	if (m_stagingIdx != POOL_STACK_END)
	{
		ChunkStagingResources* stage = (*instance->m_stagingResources)[m_stagingIdx];
		instance->m_stagingResources->enqueue(m_stagingIdx);
		m_stagingIdx = POOL_STACK_END;
	}
}

void VoxelChunk::ReleaseSubResources(Engine* instance, std::vector<GPUResourceHandle*>& trash)
{
	for (size_t i = 0; i < m_subChunks.size(); i++)
	{
		m_subChunks[i].ReleaseResources(instance, trash);
		m_subChunks[i].ReleaseSubResources(instance, trash);
	}

	m_subChunks.clear();
}

void VoxelChunk::AllocateVolume(Engine* instance, const glm::uvec3& size)
{
	m_densityImage.m_size = { size.x, size.y , size.z };
	m_densityImage.Allocate(instance);
}

void VoxelChunk::Build(Engine* instance, VkCommandBuffer commandBuffer, float voxelSize, BodyForm* forms, uint32_t formsCount, std::vector<GPUResourceHandle*>& trash)
{
	ChunkStagingResources* staging = nullptr;
	if (m_stagingIdx == POOL_STACK_END)
	{
		m_stagingIdx = instance->m_stagingResources->dequeue();
		if (m_stagingIdx == POOL_STACK_END)
			return;

		staging = (*instance->m_stagingResources)[m_stagingIdx];
		staging->Reset();
	}
	else
	{
		staging = (*instance->m_stagingResources)[m_stagingIdx];
	}

	if (!staging->Ready(instance, commandBuffer))
		return;

	VkDevice device = instance->Device();
	if (staging->m_stage == CHUNK_STAGE_IDLE)
	{
		staging->m_stage = CHUNK_STAGE_VOLUME_ANALYSIS;

		glm::vec3 size = m_max - m_min;
		float sMax = std::max(size.x, std::max(size.y, size.z));
		int rMax = std::min((int)std::round(sMax / voxelSize), (int)Engine::CHUNK_SIZE);
#define RSIZE(axis) (uint32_t)std::round(rMax * axis / sMax)
		glm::uvec3 effectiveSize(RSIZE(size.x), RSIZE(size.y), RSIZE(size.z));
		memcpy(&staging->m_density.m_size, &effectiveSize, sizeof(glm::uvec3));

		VkImageMemoryBarrier colorMemB = {};
		colorMemB.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		colorMemB.image = staging->m_colorMap.m_gpuHandle->m_image;
		colorMemB.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		colorMemB.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		colorMemB.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		colorMemB.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		colorMemB.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		colorMemB.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		colorMemB.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorMemB.subresourceRange.baseArrayLayer = 0;
		colorMemB.subresourceRange.baseMipLevel = 0;
		colorMemB.subresourceRange.layerCount = 1;
		colorMemB.subresourceRange.levelCount = 1;

		glm::vec3 vSize = size / glm::vec3(effectiveSize);
		glm::vec3 pMin = m_min - vSize * (float)Engine::CHUNK_PADDING;
		glm::vec3 pMax = m_max + vSize * ((float)Engine::CHUNK_PADDING + 1.0f);

		for (uint32_t i = 0; i < formsCount; i++)
		{
			BodyForm form = forms[i];
			FormConstants fConsts = {};

			fConsts.scale = 1.0f / (float)vSize.length();
			if (i == 0)
			{
				fConsts.range = effectiveSize + glm::uvec3(1U + (uint32_t)Engine::CHUNK_PADDING * 2U);
				fConsts.offset = { 0,0,0 };
				fConsts.transform = glm::translate(pMin) * glm::scale(vSize);
			}
			else
			{
				glm::vec3 fMax = glm::min(pMax, form.max);
				glm::vec3 fMin = glm::max(pMin, form.min);
				glm::ivec3 vMin = glm::ceil((fMin - pMin - vSize * 0.5f) / vSize);
				glm::ivec3 vMax = glm::floor((fMax - pMin + vSize * 0.5f) / vSize);
				glm::ivec3 range = vMax - vMin;
				if (range.x <= 0 || range.y <= 0 || range.z <= 0)
					continue;

				glm::vec3 fCenter = (form.min + form.max) * 0.5f;
				glm::vec3 corner = (pMin + vSize * glm::vec3(vMin)) - fCenter;

				fConsts.range = range;
				fConsts.offset = vMin;
				fConsts.transform = glm::translate(corner) * glm::scale(vSize);
			}

			VkPipeline formPipeline;
			VkPipelineLayout formLayout;
			form.formCompute->GetVkPipeline(formPipeline, formLayout);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, formPipeline);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, formLayout, 0, 1, &staging->m_formDSet, 0, nullptr);
			vkCmdPushConstants(commandBuffer, formLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(FormConstants), &fConsts);
			vkCmdDispatch(commandBuffer,
				(uint32_t)std::ceilf(fConsts.range.x / 4.0f),
				(uint32_t)std::ceilf(fConsts.range.y / 4.0f),
				(uint32_t)std::ceilf(fConsts.range.z / 4.0f));

			if (i < formsCount - 1)
			{
				vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0,
					0, nullptr,
					0, nullptr,
					1, &colorMemB);
			}
		}

		vkCmdFillBuffer(commandBuffer, staging->m_info.m_gpuHandle->m_buffer, 0, 24, 0);
		vkCmdFillBuffer(commandBuffer, staging->m_info.m_gpuHandle->m_buffer, 24, 12, Engine::CHUNK_SIZE);

		std::vector<VkBufferMemoryBarrier> bufferMemBs(2);
		bufferMemBs[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		bufferMemBs[0].buffer = staging->m_info.m_gpuHandle->m_buffer;
		bufferMemBs[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferMemBs[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		bufferMemBs[0].offset = 0;
		bufferMemBs[0].size = VK_WHOLE_SIZE;
		bufferMemBs[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferMemBs[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

		colorMemB.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			1, bufferMemBs.data(),
			1, &colorMemB);

		VkPipeline analysisPipeline;
		VkPipelineLayout analysisPipelineLayout;
		instance->m_surfaceAnalysisPipeline.GetVkPipeline(analysisPipeline, analysisPipelineLayout);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, analysisPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, analysisPipelineLayout, 0, 1, &staging->m_analysisDSet, 0, nullptr);
		SurfaceAnalysisConstants surfConsts = {};
		surfConsts.base = glm::uvec3(Engine::CHUNK_PADDING, Engine::CHUNK_PADDING, Engine::CHUNK_PADDING);
		surfConsts.range = effectiveSize;
		vkCmdPushConstants(commandBuffer, analysisPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SurfaceAnalysisConstants), &surfConsts);
		vkCmdDispatch(commandBuffer,
			(uint32_t)std::ceilf((surfConsts.range.x + 1) / 4.0f),
			(uint32_t)std::ceilf((surfConsts.range.y + 1) / 4.0f),
			(uint32_t)std::ceilf((surfConsts.range.z + 1) / 4.0f));

		bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		bufferMemBs[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		bufferMemBs[1] = bufferMemBs[0];
		bufferMemBs[1].buffer = staging->m_infoStaging.m_gpuHandle->m_buffer;
		bufferMemBs[1].srcAccessMask = 0;
		bufferMemBs[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, nullptr,
			2, bufferMemBs.data(),
			0, nullptr);
		VkBufferCopy attribCopy = {};
		attribCopy.size = sizeof(SurfaceAnalysisInfo);
		attribCopy.dstOffset = 0;
		attribCopy.srcOffset = 0;
		vkCmdCopyBuffer(commandBuffer, staging->m_info.m_gpuHandle->m_buffer, staging->m_infoStaging.m_gpuHandle->m_buffer, 1, &attribCopy);

		bufferMemBs[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		bufferMemBs[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		bufferMemBs[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		bufferMemBs[1].dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0,
			0, nullptr,
			2, bufferMemBs.data(),
			0, nullptr);

		vkCmdSetEvent(commandBuffer, staging->m_analysisCompleteEvent, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}
	else if (staging->m_stage == CHUNK_STAGE_VOLUME_ANALYSIS)
	{
		if (vkGetEventStatus(device, staging->m_analysisCompleteEvent) == VK_EVENT_RESET)
			return;
		vkResetEvent(device, staging->m_analysisCompleteEvent);

		VmaAllocator allocator = instance->Allocator();
		void* attribData;
		vmaMapMemory(allocator, staging->m_infoStaging.m_gpuHandle->m_allocation, &attribData);
		SurfaceAnalysisInfo surfaceAttribs = {};
		std::memcpy(&surfaceAttribs, attribData, sizeof(SurfaceAnalysisInfo));
		vmaUnmapMemory(allocator, staging->m_infoStaging.m_gpuHandle->m_allocation);

		if (surfaceAttribs.cellCount > 0)
		{
			staging->m_stage = CHUNK_STAGE_VISUAL_ASSEMBLY;
			staging->m_vertexCount = surfaceAttribs.vertexCount;
			staging->m_indexCount = surfaceAttribs.indexCount;
			staging->m_boundsMax = surfaceAttribs.max;
			staging->m_boundsMin = surfaceAttribs.min;

			staging->m_verticies.Dereference();
			staging->m_verticies.m_byteCount = surfaceAttribs.vertexCount * 16LL;
			staging->m_verticies.Allocate(instance);

			staging->m_indicies.Dereference();
			staging->m_indicies.m_byteCount = surfaceAttribs.indexCount * 4LL;
			staging->m_indicies.Allocate(instance);

			VkDescriptorBufferInfo vertBI = { staging->m_verticies.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };
			VkDescriptorBufferInfo idxsBI = { staging->m_indicies.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };

			std::vector<VkWriteDescriptorSet> descWrites(2);
			descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descWrites[0].descriptorCount = 1;
			descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descWrites[0].dstArrayElement = 0;
			descWrites[0].dstBinding = 0;
			descWrites[0].pBufferInfo = &vertBI;
			descWrites[1] = descWrites[0];
			descWrites[1].dstBinding = 1;
			descWrites[1].pBufferInfo = &idxsBI;

			VkPipeline assemblyPipeline;
			VkPipelineLayout assemblyPipelineLayout;
			instance->m_surfaceAssemblyPipeline.GetVkPipeline(assemblyPipeline, assemblyPipelineLayout);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipeline);
			vkCmdPushDescriptorSet(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipelineLayout, 1, 2, descWrites.data());
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, assemblyPipelineLayout, 0, 1, &staging->m_assemblyDSet, 0, nullptr);

			SurfaceAssemblyConstants surfConsts = {};
			surfConsts.base = glm::uvec3(Engine::CHUNK_PADDING, Engine::CHUNK_PADDING, Engine::CHUNK_PADDING);
			surfConsts.offset = m_min;
			surfConsts.scale = (m_max - m_min) / 
				glm::vec3(staging->m_density.m_size.width, staging->m_density.m_size.height, staging->m_density.m_size.depth);
			vkCmdPushConstants(commandBuffer, assemblyPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SurfaceAssemblyConstants), &surfConsts);
			vkCmdDispatch(commandBuffer, (uint32_t)std::ceilf(surfaceAttribs.cellCount / 64.0f), 1, 1);

			std::vector<VkBufferMemoryBarrier> bufferMemBs(2);
			bufferMemBs[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemBs[0].buffer = staging->m_verticies.m_gpuHandle->m_buffer;
			bufferMemBs[0].srcQueueFamilyIndex = instance->m_computeQueueFamily;
			bufferMemBs[0].dstQueueFamilyIndex = instance->m_instance.queueFamilyIndex;
			bufferMemBs[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferMemBs[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
			bufferMemBs[0].offset = 0;
			bufferMemBs[0].size = VK_WHOLE_SIZE;

			bufferMemBs[1] = bufferMemBs[0];
			bufferMemBs[1].buffer = staging->m_indicies.m_gpuHandle->m_buffer;
			bufferMemBs[1].dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0, nullptr,
				2, bufferMemBs.data(),
				0, nullptr);

			vkCmdSetEvent(commandBuffer, staging->m_assemblyCompleteEvent, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
		else
		{
			//staging->m_density.Release(instance);
			//staging->m_verticies.Release(instance);
			//staging->m_indicies.Release(instance);
			SAFE_TRASH(staging->m_density);
			SAFE_TRASH(staging->m_verticies);
			SAFE_TRASH(staging->m_indicies);
			staging->m_stage = CHUNK_STAGE_IDLE;
			ReleaseResources(instance, trash);
			ReleaseSubResources(instance, trash);
			m_built = true;
		}
	}
	else if (staging->m_stage == CHUNK_STAGE_VISUAL_ASSEMBLY)
	{
		if (vkGetEventStatus(device, staging->m_assemblyCompleteEvent) == VK_EVENT_RESET)
			return;
		vkResetEvent(device, staging->m_assemblyCompleteEvent);

		glm::vec3 vSize = (m_max - m_min) /
			glm::vec3(staging->m_density.m_size.width, staging->m_density.m_size.height, staging->m_density.m_size.depth);
		m_boundMin = m_min + vSize * glm::vec3(staging->m_boundsMin);
		m_boundMax = m_min + vSize * glm::vec3(staging->m_boundsMax + 1U);

		//m_densityImage.Release(instance);
		//m_vertexBuffer.Release(instance);
		//m_indexBuffer.Release(instance);
		SAFE_TRASH(m_densityImage);
		SAFE_TRASH(m_vertexBuffer);
		SAFE_TRASH(m_indexBuffer);
		m_densityImage = staging->m_density;
		m_vertexBuffer = staging->m_verticies;
		m_indexBuffer = staging->m_indicies;
		m_vertexCount = staging->m_vertexCount;
		m_indexCount = staging->m_indexCount;
		staging->m_density.m_gpuHandle = nullptr;
		staging->m_verticies.Dereference();
		staging->m_indicies.Dereference();
		staging->m_stage = CHUNK_STAGE_IDLE;

		instance->m_stagingResources->enqueue(m_stagingIdx);
		m_stagingIdx = POOL_STACK_END;
		m_built = true;
	}
}

ChunkStagingResources::ChunkStagingResources(Engine* instance, uint8_t size, uint8_t padding)
{
	VkDevice device = instance->Device();

	VkEventCreateInfo eventCI = {};
	eventCI.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
	vkCreateEvent(device, &eventCI, nullptr, &m_analysisCompleteEvent);
	vkCreateEvent(device, &eventCI, nullptr, &m_assemblyCompleteEvent);

	m_info.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	m_info.m_memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
	m_info.m_byteCount = sizeof(SurfaceAnalysisInfo);
	m_info.Allocate(instance);
	
	m_infoStaging.m_bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	m_infoStaging.m_memoryUsage = VMA_MEMORY_USAGE_GPU_TO_CPU;
	m_infoStaging.m_byteCount = sizeof(SurfaceAnalysisInfo);
	m_infoStaging.Allocate(instance);

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
	VkDescriptorBufferInfo infoW = { m_info.m_gpuHandle->m_buffer, 0, VK_WHOLE_SIZE };
	VkDescriptorBufferInfo cellCountW = { m_info.m_gpuHandle->m_buffer, 0, sizeof(uint32_t) };

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
	//Info
	writes[4] = writes[3];
	writes[4].dstBinding = 3;
	writes[4].pBufferInfo = &infoW;
	//Assembly: Color map
	writes[5] = writes[1];
	writes[5].dstSet = m_assemblyDSet;
	//Index map
	writes[6] = writes[2];
	writes[6].dstSet = m_assemblyDSet;
	//Cells
	writes[7] = writes[3];
	writes[7].dstSet = m_assemblyDSet;
	//CellCount
	writes[8] = writes[4];
	writes[8].dstSet = m_assemblyDSet;
	writes[8].pBufferInfo = &cellCountW;
	
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void ChunkStagingResources::GetImageTransferBarriers(VkImageMemoryBarrier& colorBarrier, VkImageMemoryBarrier& indexBarrier)
{
	colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	colorBarrier.image = m_colorMap.m_gpuHandle->m_image;

	colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	colorBarrier.srcAccessMask = 0;
	colorBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
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

bool ChunkStagingResources::Ready(Engine* instance, VkCommandBuffer commandBuffer)
{
	VkDevice device = instance->Device();
	if (m_reset)
	{
		if (m_stage == CHUNK_STAGE_VOLUME_ANALYSIS)
		{
			if (vkGetEventStatus(device, m_analysisCompleteEvent) == VK_EVENT_RESET)
				return false;
			vkResetEvent(device, m_analysisCompleteEvent);
		}
		else if (m_stage == CHUNK_STAGE_VISUAL_ASSEMBLY)
		{
			if (vkGetEventStatus(device, m_assemblyCompleteEvent) == VK_EVENT_RESET)
				return false;
			vkResetEvent(device, m_assemblyCompleteEvent);
			m_verticies.Release(instance);
			m_indicies.Release(instance);
			m_density.Release(instance);
		}
		m_stage = CHUNK_STAGE_IDLE;
		m_reset = false;
	}
	return true;
}

void ChunkStagingResources::Deallocate(Engine* instance)
{
	VkDevice device = instance->Device();
	vkDestroyEvent(device, m_analysisCompleteEvent, nullptr);
	vkDestroyEvent(device, m_assemblyCompleteEvent, nullptr);
	m_colorMap.m_gpuHandle->Deallocate(instance);
	m_indexMap.m_gpuHandle->Deallocate(instance);
	m_cells.m_gpuHandle->Deallocate(instance);
	m_info.m_gpuHandle->Deallocate(instance);
	m_infoStaging.m_gpuHandle->Deallocate(instance);
#define SAFE_DEALLOC(res) if(res.m_gpuHandle) res.m_gpuHandle->Deallocate(instance)
	SAFE_DEALLOC(m_verticies);
	SAFE_DEALLOC(m_indicies);
	SAFE_DEALLOC(m_density);
#undef SAFE_DEALLOC
}

bool ChunkRenderPackage::FrustumTest(const glm::mat4x4 mvp)
{
	glm::vec4 corners[8] = {
	mvp * glm::vec4(min, 1.0),//000
	mvp * glm::vec4(max.x, min.y, min.z, 1.0),//100
	mvp * glm::vec4(min.x, max.y, min.z, 1.0),//010
	mvp * glm::vec4(max.x, max.y, min.z, 1.0),//110
	mvp * glm::vec4(min.x, min.y, max.z, 1.0),//001
	mvp * glm::vec4(max.x, min.y, max.z, 1.0),//101
	mvp * glm::vec4(min.x, max.y, max.z, 1.0),//011
	mvp * glm::vec4(max, 1.0),//111
	};

	glm::ivec3 bounds = {0,0,0};
	for (int i = 0; i < 8; i++)
	{
		glm::vec4 v = corners[i];
		if (v.x < -v.w) bounds.x++;
		if (v.x > v.w) bounds.x--;
		if (v.y < -v.w) bounds.y++;
		if (v.y > v.w) bounds.y--;
		if (v.z < 0.0f) bounds.z++;
		if (v.z > v.w) bounds.z--;
	}
	if (std::abs(bounds.x) == 8 || std::abs(bounds.y) == 8 || std::abs(bounds.z) == 8)
		return false;
	return true;
}
