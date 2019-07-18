#pragma once
#include "GPUResource.h"
class Engine;

struct PipelineHandle : public GPUResourceHandle
{
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	void Deallocate(Engine* instance) override;
};

class Pipeline : GPUResource
{
public:
	void GetVkPipeline(VkPipeline& pipeline, VkPipelineLayout& layout);
	void Allocate(Engine* instance) override = 0;
	void Release(Engine* instance) override;
protected:
	PipelineHandle* m_gpuHandle = nullptr;
};