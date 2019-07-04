#pragma once

#include "../VMA.h"
#include <atomic>

#define DEALLOC_STATE 0xFF
#define SAFE_DESTROY(handle) instance->DestroyResource(handle); handle = nullptr
class Engine;

struct GPUResourceHandle
{
	virtual void Deallocate(Engine* instance) = 0;

	inline void Pin() { m_pinnedLevel++; }
	inline void Unpin() { m_pinnedLevel--; }
	inline bool IsPinned() { return m_pinnedLevel > 0; }
private:
	std::atomic<uint8_t> m_pinnedLevel = 0;
};


class GPUResource
{
public:
	friend class Engine;
	virtual void Allocate(Engine* instance) = 0;
	virtual void Release(Engine* instance) = 0;
protected:
};