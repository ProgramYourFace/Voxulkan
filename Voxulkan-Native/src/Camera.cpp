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

EXPORT void SetCameraVP(Camera* camera, CameraConstants constants)
{
	const_cast<CameraConstants&>(camera->m_constants) = constants;
}

Camera::Camera(Engine* instance) : m_instance(instance)
{

}