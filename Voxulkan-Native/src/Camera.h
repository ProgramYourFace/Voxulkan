#pragma once
#include <atomic>
#include "glm/glm.hpp"

class Engine;

class Camera
{
public:
	friend class Engine;

	Camera(Engine* instance);

	Engine* m_instance = nullptr;
	std::atomic<glm::mat4x4> m_VP_Matrix;
};