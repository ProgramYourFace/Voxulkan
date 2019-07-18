#pragma once
#include <vector>
#include "Pipeline.h"

class ComputePipeline : public Pipeline
{
public:
	void Allocate(Engine* instance) override;
	void Release(Engine* instance) override;

	//Layout info
	std::vector<VkPushConstantRange> m_pushConstants;
	std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

	//Shader info
	std::vector<char> m_shader;
};

