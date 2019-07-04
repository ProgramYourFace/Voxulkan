#include "RenderPipeline.h"
#include "..//Plugin.h"
#include "..//Engine.h"

void RenderPipeline::ConstructOnRenderPass(Engine* instance, VkRenderPass renderPass)
{
	if (instance && renderPass)
	{
		if (m_registeredPass != renderPass)
		{
			Release(instance);
			m_registeredPass = renderPass;
			Construct(instance);
		}
	}
}

void RenderPipeline::Construct(Engine* instance)
{

	if (!m_registeredPass || !instance)
		return;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pPushConstantRanges = m_pushConstants.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstants.size());
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.setLayoutCount = 0;

	m_gpuHandle = new PipelineHandle();
	VkDevice device = instance->Device();
	bool success = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, NULL, &m_gpuHandle->m_layout) == VK_SUCCESS;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};

	VkPipelineShaderStageCreateInfo shaderStages[2] = {};
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].pName = "main";
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].pName = "main";

	if (success)
	{
		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = static_cast<uint32_t>(m_vertexShader.size());
		moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(m_vertexShader.data());
		success = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderStages[0].module) == VK_SUCCESS;
	}

	if (success)
	{
		VkShaderModuleCreateInfo moduleCreateInfo = {};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = static_cast<uint32_t>(m_fragmentShader.size());
		moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(m_fragmentShader.data());
		success = vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderStages[1].module) == VK_SUCCESS;
	}

	if (success)
	{
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = m_gpuHandle->m_layout;
		pipelineCreateInfo.renderPass = m_registeredPass;

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineRasterizationStateCreateInfo rasterizationState = {};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.polygonMode = m_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = m_cullMode;
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.rasterizerDiscardEnable = VK_FALSE;
		rasterizationState.depthBiasEnable = VK_FALSE;
		rasterizationState.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
		blendAttachmentState[0].colorWriteMask = 0xf;
		blendAttachmentState[0].blendEnable = VK_FALSE;
		VkPipelineColorBlendStateCreateInfo colorBlendState = {};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentState;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		const VkDynamicState dynamicStateEnables[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_STENCIL_REFERENCE };
		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables;
		dynamicState.dynamicStateCount = sizeof(dynamicStateEnables) / sizeof(*dynamicStateEnables);

		VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthBoundsTestEnable = VK_FALSE;
		depthStencilState.stencilTestEnable = VK_FALSE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL; // Unity/Vulkan uses reverse Z

		VkPipelineMultisampleStateCreateInfo multisampleState = {};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.pSampleMask = NULL;

		VkPipelineVertexInputStateCreateInfo vertexInputState = {};
		vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertexBindings.size());
		vertexInputState.pVertexBindingDescriptions = m_vertexBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = m_vertexAttributes.data();

		pipelineCreateInfo.stageCount = sizeof(shaderStages) / sizeof(*shaderStages);
		pipelineCreateInfo.pStages = shaderStages;
		pipelineCreateInfo.pVertexInputState = &vertexInputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;

		success = vkCreateGraphicsPipelines(device, NULL, 1, &pipelineCreateInfo, NULL, &m_gpuHandle->m_pipeline) == VK_SUCCESS;
	}

	if (shaderStages[0].module != VK_NULL_HANDLE)
		vkDestroyShaderModule(device, shaderStages[0].module, NULL);
	if (shaderStages[1].module != VK_NULL_HANDLE)
		vkDestroyShaderModule(device, shaderStages[1].module, NULL);

	if (!success)
	{
		SAFE_DEL(m_gpuHandle);
		LOG("Pipleline creation failed!");
	}
}