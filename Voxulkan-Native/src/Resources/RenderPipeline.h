#pragma once
#include <vector>
#include "Pipeline.h"

class RenderPipeline : public Pipeline
{
public:
	void AllocateOnRenderPass(Engine* instance, VkRenderPass renderPass);
	void Allocate(Engine* instance) override;

	//Layout info
	std::vector<VkPushConstantRange> m_pushConstants;

	//Shader info
	std::vector<char> m_vertexShader;
	std::vector<char> m_fragmentShader;

	//Pipeline info
	bool m_wireframe = false;
	VkCullModeFlagBits m_cullMode = VK_CULL_MODE_BACK_BIT;

	//Vertex info
	std::vector<VkVertexInputBindingDescription> m_vertexBindings;
	std::vector<VkVertexInputAttributeDescription> m_vertexAttributes;

private:
	//State data
	VkRenderPass m_registeredPass = VK_NULL_HANDLE;
};