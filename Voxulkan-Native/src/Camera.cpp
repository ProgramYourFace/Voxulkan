#include "Camera.h"
#include "Plugin.h"

EXPORT Camera* CreateNativeCamera(Engine* instance)
{
	return new Camera(instance);
}

EXPORT void DestroyNativeCamera(Camera* camera)
{
	delete camera;
}

EXPORT void SetCameraVP(Camera* camera, glm::mat4x4 VP)
{
	camera->m_VP_Matrix.store(VP, std::memory_order_relaxed);
}

Camera::Camera(Engine* instance) : m_instance(instance)
{
}