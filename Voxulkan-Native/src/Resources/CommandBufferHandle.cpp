#include "CommandBufferHandle.h"
#include "..//Engine.h"

void CommandBufferHandle::Deallocate(Engine* instance)
{
	if (m_CMDB && m_pool)
	{
		vkFreeCommandBuffers(instance->Device(), m_pool, 1, &m_CMDB);
	}
}