#include "ComputePipeline.h"
#include "..//Plugin.h"
#include "..//Engine.h"

void ComputePipeline::Allocate(Engine* instance)
{
	if (m_gpuHandle)
	{
		LOG("Compute Pipeline creation failed! Already allocated!");
		return;
	}
	if (!instance)
		return;

	VkShaderModule shaderModule = VK_NULL_HANDLE;

	VkDevice device = instance->Device();

	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.codeSize = static_cast<uint32_t>(m_shader.size());
	moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(m_shader.data());
	bool success = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule) == VK_SUCCESS;

	m_gpuHandle = new PipelineHandle();

	if (success)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pPushConstantRanges = m_pushConstants.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstants.size());
		pipelineLayoutCreateInfo.pSetLayouts = m_descriptorSetLayouts.data();
		pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size());

		success = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &m_gpuHandle->m_layout) == VK_SUCCESS;
	}

	if (success)
	{
		VkComputePipelineCreateInfo pipelineCreateInfo = {};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = m_gpuHandle->m_layout;

		pipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineCreateInfo.stage.module = shaderModule;
		pipelineCreateInfo.stage.pName = "main";

		success = vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &m_gpuHandle->m_pipeline) == VK_SUCCESS;
	}

	if (shaderModule != VK_NULL_HANDLE)
		vkDestroyShaderModule(device, shaderModule, NULL);

	if (!success)
	{
		SAFE_DEL(m_gpuHandle);
		LOG("Pipleline creation failed!");
	}
}

void ComputePipeline::Release(Engine* instance)
{
	SAFE_DESTROY(m_gpuHandle);
}
