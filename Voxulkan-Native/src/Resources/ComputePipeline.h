#pragma once
#include "Pipeline.h"

class ComputePipeline : public Pipeline
{
public:
	void Allocate(Engine* instance) override;
	void Release(Engine* instance) override;

	//Shader info
	std::vector<char> m_shader;
};

