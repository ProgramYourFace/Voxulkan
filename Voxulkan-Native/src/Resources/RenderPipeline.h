#pragma once
#include "Pipeline.h"

class RenderPipeline : public Pipeline
{
public:
	void AllocateOnRenderPass(Engine* instance, VkRenderPass renderPass);
	void Allocate(Engine* instance) override;

	//Shader info
	std::vector<char> m_vertexShader;
	std::vector<char> m_fragmentShader;
	std::vector<char> m_tessCtrlShader;
	std::vector<char> m_tessEvalShader;

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