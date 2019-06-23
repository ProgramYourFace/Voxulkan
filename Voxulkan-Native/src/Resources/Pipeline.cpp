#include "Pipeline.h"
#include "..//Engine.h"

void Pipeline::GetVkPipeline(VkPipeline& pipeline, VkPipelineLayout& layout)
{
	if (m_gpuHandle)
	{
		pipeline = m_gpuHandle->m_pipeline;
		layout = m_gpuHandle->m_layout;
	}
	else
	{
		pipeline = nullptr;
		layout = nullptr;
	}
}

void Pipeline::Release()
{
	SAFE_RELEASE_HANDLE(m_gpuHandle);
}

Pipeline::~Pipeline()
{
	Release();
}

void PipelineHandle::Dispose(VmaAllocator allocator)
{
	VkDevice dev = vmaGetAllocatorDevice(allocator);
	if (m_pipeline)
		vkDestroyPipeline(dev, m_pipeline, nullptr);
	if (m_layout)
		vkDestroyPipelineLayout(dev, m_layout, nullptr);

	m_pipeline = nullptr;
	m_layout = nullptr;
}