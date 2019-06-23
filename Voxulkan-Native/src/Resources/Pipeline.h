#pragma once
#include "GResource.h"

struct PipelineHandle : public GResourceHandle
{
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	void Dispose(VmaAllocator allocator) override;
};

class Pipeline
{
public:
	void GetVkPipeline(VkPipeline& pipeline, VkPipelineLayout& layout);
	virtual void Construct(VkDevice device) {};
	virtual void Release();

	~Pipeline();
protected:
	PipelineHandle* m_gpuHandle = nullptr;
};