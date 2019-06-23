#pragma once

#include "../VMA.h"

#define SAFE_RELEASE_HANDLE(handle) Engine::Get().SafeDestroyResource(handle); handle = nullptr

struct GResourceHandle
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