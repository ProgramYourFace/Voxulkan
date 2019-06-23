#pragma once
#include <vector>
#include "Pipeline.h"
#include "GResource.h"

class ComputePipeline : public Pipeline
{
public:
	inline void SetPushConstants(const std::vector<VkPushConstantRange>& pushConstants)
	{ m_pushConstants = pushConstants; }

	inline void SetDescriptorSetLayouts(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts)
	{ m_descriptorSetLayouts = descriptorSetLayouts; }

	inline void SetShader(const std::vector<char>& shader)
	{ m_shader = shader; }

	void Construct(VkDevice device) override;
	void Release() override;

	friend class Engine;
private:
	//Layout info
	std::vector<VkPushConstantRange> m_pushConstants;
	std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

	//Shader info
	std::vector<char> m_shader;
};

