#pragma once
#include <atomic>
#include "glm/glm.hpp"

class Engine;

struct CameraConstants
{
	glm::mat4x4 viewProjection = {};
	glm::vec3 worldPosition = {};
	float tessellationFactor = 0.0f;
};

class Camera
{
public:
	friend class Engine;

	Camera(Engine* instance);

	Engine* m_instance = nullptr;
	
	volatile CameraConstants m_constants = {};
};