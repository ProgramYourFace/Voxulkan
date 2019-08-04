#pragma once
#include <atomic>
#include "glm/glm.hpp"
#include "Components/VoxelBody.h"
#include "Containers/MutexList.h"

class Engine;

struct CameraView
{
	glm::mat4x4 viewProjection = {};
	glm::vec3 worldPosition = {};
	float tessellationFactor = 0.0f;
};

class Camera : GPUResourceHandle
{
public:
	friend class Engine;

	Camera(Engine* instance);

	void Deallocate(Engine* instance) override;

	Engine* m_instance = nullptr;
	VkQueryPool m_occlusionQuery = VK_NULL_HANDLE;
	MutexList<BodyRenderPackage> m_renderPackage = {};
	
	volatile CameraView m_view = {};
	
	const VkDeviceSize QUERY_SIZE = 1000;
};