#pragma once
#include "GPUResource.h"
#include <vector>
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

	void DestroyDSetLayouts(Engine* instance);

	//Layout info
	std::vector<VkPushConstantRange> m_pushConstants;
	std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
protected:
	PipelineHandle* m_gpuHandle = nullptr;
};