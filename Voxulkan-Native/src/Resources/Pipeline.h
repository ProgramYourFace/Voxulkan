#pragma once
#include "GResource.h"
class Engine;

struct PipelineHandle : public GPUResourceHandle
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
	virtual void Release(Engine* engine);

	~Pipeline();
protected:
	PipelineHandle* m_gpuHandle = nullptr;
};