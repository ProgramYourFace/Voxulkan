#pragma once
#include "GPUResource.h"

class CommandBufferHandle : GPUResourceHandle
{
	void Deallocate(Engine* instance) override;

	VkCommandBuffer m_CMDB;
	VkCommandPool m_pool;
};