#include "Camera.h"
#include "Plugin.h"

EXPORT Camera* CreateCameraHandle(Engine* instance)
{
	return new Camera(instance);
}

EXPORT void SetCameraView(Camera* camera, CameraView constants)
{
	const_cast<CameraView&>(camera->m_view) = constants;
}

Camera::Camera(Engine* instance) : m_instance(instance)
{

}

void Camera::Deallocate(Engine* instance)
{
}
