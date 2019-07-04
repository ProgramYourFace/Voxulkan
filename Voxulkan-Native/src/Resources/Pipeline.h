#pragma once
#include "GPUResource.h"
class Engine;

struct PipelineHandle : public GPUResourceHandle
{
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	void Deallocate(Engine* instance) override;
};

class Pipeline
{
public:
	void GetVkPipeline(VkPipeline& pipeline, VkPipelineLayout& layout);
	virtual void Construct(Engine* instance) {};
	virtual void Release(Engine* instance);
protected:
	PipelineHandle* m_gpuHandle = nullptr;
};