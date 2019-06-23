#pragma once
#include <atomic>
#include "glm/glm.hpp"

class Camera
{
public:
	std::atomic<glm::mat4x4> m_VP_Matrix;
};