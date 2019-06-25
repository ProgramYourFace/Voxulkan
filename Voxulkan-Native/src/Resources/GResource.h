#pragma once

#include "../VMA.h"

#define SAFE_RELEASE_HANDLE(handle) engine->SafeDestroyResource(handle); handle = nullptr

struct GPUResourceHandle
{
	virtual void Dispose(VmaAllocator allocator) {};
};

class GResource
{
public:
	virtual void Release() {};
	virtual void Allocate(VmaAllocator allocator) {};

	~GResource();
	friend class Engine;
protected:
};