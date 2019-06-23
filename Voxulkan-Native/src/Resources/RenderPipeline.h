#pragma once
#include <vector>
#include "Pipeline.h"
#include "GResource.h"

class RenderPipeline : public Pipeline
{
public:
	void ConstructWithRP(VkDevice device, VkRenderPass renderPass);

	inline void SetPushConstants(const std::vector<VkPushConstantRange>& pushConstants)
	{
		m_pushConstants = pushConstants;
	}

	inline void SetShaders(const std::vector<char>& vertexShader, const std::vector<char>& fragmentShader)
	{
		m_vertexShader = vertexShader;
		m_fragmentShader = fragmentShader;
	}

	inline void SetWireframe(const bool& wireframe)
	{
		m_wireframe = wireframe;
	}

	inline void SetCullMode(const VkCullModeFlagBits& cullMode)
	{
		m_cullMode = cullMode;
	}

	inline void SetVertexLayout(const std::vector<VkVertexInputBindingDescription>& vertexBindings, const std::vector<VkVertexInputAttributeDescription>& vertexAttributes)
	{
		m_vertexBindings = vertexBindings;
		m_vertexAttributes = vertexAttributes;
	}

	void Construct(VkDevice device) override;

private:
	//State data
	VkRenderPass m_registeredPass = VK_NULL_HANDLE;

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
};