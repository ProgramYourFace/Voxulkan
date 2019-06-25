#pragma once
#include <vector>
#include "Pipeline.h"
#include "GResource.h"

class ComputePipeline : public Pipeline
{
public:
	void Construct(VkDevice device) override;
	void Release() override;

	//Layout info
	std::vector<VkPushConstantRange> m_pushConstants;
	std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

	//Shader info
	std::vector<char> m_shader;
};

