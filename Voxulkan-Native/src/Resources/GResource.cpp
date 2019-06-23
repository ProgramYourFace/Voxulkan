#include "..//Plugin.h"
#include "GResource.h"

GResource::~GResource()
{
	Release();
}

EXPORT void DeleteNativeResouce(GResource*& resource)
{
	SAFE_DELETE(resource);
}