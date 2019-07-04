#include "..//Plugin.h"
#include "GPUResource.h"

EXPORT void DeleteNativeResouce(Engine* instance, GPUResource*& resource)
{
	SAFE_DEL_DEALLOC(resource, instance);
}