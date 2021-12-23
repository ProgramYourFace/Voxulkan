#include "VoxelBody.h"
#include "..//Engine.h"
#include "..//Plugin.h"
#include <glm/glm.hpp>
#include <algorithm>

VoxelBody::VoxelBody(const glm::vec3& min, const glm::vec3& max)
{
	m_root.m_min = min;
	m_root.m_max = max;
	const_cast<glm::mat4x4&>(m_transform) = glm::mat4x4(1.0f);
	m_trash = new std::vector<GPUResourceHandle*>[Engine::WORKER_CMDB_COUNT];
}

inline float ComputeError(float distance, float size)
{
	return size / std::max(distance, 0.00001f);
}

void VoxelBody::RenderChunk(const VoxelChunk& chunk, std::vector<ChunkRenderPackage>& render, glm::vec3& min, glm::vec3& max)
{
	if (chunk.m_indexCount != 0 &&
		chunk.m_vertexBuffer.m_gpuHandle != nullptr &&
		chunk.m_indexBuffer.m_gpuHandle != nullptr)
	{
		ChunkRenderPackage p;
		p.vertexBuffer = chunk.m_vertexBuffer.m_gpuHandle;
		p.indexBuffer = chunk.m_indexBuffer.m_gpuHandle;
		p.indexCount = chunk.m_indexCount;
		p.max = chunk.m_max;
		p.min = chunk.m_min;
		max = glm::max(max, p.max);
		min = glm::min(min, p.min);
		render.push_back(p);
	}
}

void VoxelBody::RenderDanglingBranches(Engine* instance, VoxelChunk& chunk, std::vector<ChunkRenderPackage>& render, glm::vec3& min, glm::vec3& max)
{
	RenderChunk(chunk, render, min, max);

	for (size_t i = 0; i < chunk.m_subChunks.size(); i++)
	{
		RenderDanglingBranches(instance,chunk.m_subChunks[i], render, min, max);
		chunk.m_subChunks[i].ReleaseStaging(instance);
	}
}


void VoxelBody::Traverse(Engine* instance, const glm::vec3& observerPosition, float E, float voxelSize, BodyForm* forms, uint32_t formsCount, uint32_t maxDepth)
{
	WorkerResource* worker;
	instance->m_workers->pop(worker);
	QueueResource& queue = instance->m_queues[worker->m_queueIndex];

	std::vector<GPUResourceHandle*>& trash = m_trash[queue.m_currentCMDB];
	instance->DestroyResources(trash);
	trash.clear();

	std::vector<ChunkRenderPackage> render(0);
	render.reserve(m_lastRenderSize);
	glm::vec3 bodyMin = m_root.m_max;
	glm::vec3 bodyMax = m_root.m_min;

	VkCommandBuffer cmdb = nullptr;
	if (vkWaitForFences(instance->Device(), 1, &queue.m_fences[queue.m_currentCMDB], VK_TRUE, ~0ULL) == VK_SUCCESS)
	{
		cmdb = worker->m_computeCMDBs[queue.m_currentCMDB];

		if (!worker->m_recordingCmds)
		{
			VkCommandBufferBeginInfo beginI = {};
			beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			VK_CALL(vkBeginCommandBuffer(cmdb, &beginI));
			worker->m_recordingCmds = true;
		}
	}

	float leafSize = voxelSize * Engine::CHUNK_SIZE;
	uint32_t unbuiltCount = 0;

	struct TraversePosition
	{
		VoxelChunk* chunk = nullptr;
		size_t remainder = 0;
		uint32_t unbuiltCount = 0;
		bool returned = false;
	};

	TraversePosition* stack = new TraversePosition[maxDepth];
	m_root.UpdateDistance(observerPosition);
	stack[0] = { &m_root, 0, 0, false};
	int depth = 0;
	do
	{
		TraversePosition& pos = stack[depth];
		VoxelChunk& chunk = *(pos.chunk);
		if (pos.returned)//Branches would be the only thing to return
		{
			if (pos.unbuiltCount == unbuiltCount)//All children of branch are visible
			{
				chunk.ReleaseResources(instance, trash);
				chunk.m_built = false;
			}
			else
			{
				RenderChunk(chunk, render, bodyMin, bodyMax);
			}

			pos.returned = false;
			if (pos.remainder == 0)//Move up the higherarchy
			{
				depth--;
			}
			else//Move to the next cell horizontally
			{
				pos.chunk++;
				pos.remainder--;
			}
		}
		else
		{
			glm::vec3 size = chunk.m_max - chunk.m_min;

			bool canBranch = depth < (int)maxDepth - 1 && (size.x > leafSize || size.y > leafSize || size.z > leafSize);
			if (canBranch && ComputeError(chunk.m_distance, size.x * size.y + size.x * size.z + size.z * size.y) > E)//Branch
			{
				size_t subCount = chunk.m_subChunks.size();
				if (subCount == 0)
				{
					float minAxis = std::max(std::min(size.x, std::min(size.y, size.z)), leafSize);
					float desiredBranchSize = (1 << (maxDepth - (depth + 1))) * leafSize;
					
					if (desiredBranchSize < minAxis)
					{
						float t = (float)(depth + 1.0f) / (float)maxDepth;
						minAxis += std::lrint(t * (float)(desiredBranchSize - minAxis));
					}
#define AXISCOUNT(axis) axis <= leafSize ? 1U : std::max((uint32_t)std::ceilf(axis / minAxis), 2U)
					glm::uvec3 subDiv(AXISCOUNT(size.x), AXISCOUNT(size.y), AXISCOUNT(size.z));
					glm::vec3 subSize(size.x / (float)subDiv.x, size.y / (float)subDiv.y, size.z / (float)subDiv.z);
					subCount = (size_t)subDiv.x * subDiv.y * subDiv.z;
					chunk.m_subChunks.resize(subCount);
					uint32_t i = 0;
					for (uint32_t x = 0; x < subDiv.x; x++)
					{
						for (uint32_t y = 0; y < subDiv.y; y++)
						{
							for (uint32_t z = 0; z < subDiv.z; z++)
							{
								VoxelChunk& subChunk = chunk.m_subChunks[i];
								subChunk.m_min = {
									chunk.m_min.x + subSize.x * x,
									chunk.m_min.y + subSize.y * y,
									chunk.m_min.z + subSize.z * z };
								subChunk.m_max = { 
									subChunk.m_min.x + subSize.x,
									subChunk.m_min.y + subSize.y,
									subChunk.m_min.z + subSize.z };

								subChunk.UpdateDistance(observerPosition);
								i++;
							}
						}
					}
				}
				else
				{
					for (int i = 0; i < subCount; i++)
					{
						chunk.m_subChunks[i].UpdateDistance(observerPosition);
					}
				}

				VoxelChunk* subChunks = chunk.m_subChunks.data();
				std::sort(subChunks, subChunks + subCount);

				pos.returned = true;
				pos.unbuiltCount = unbuiltCount;
				depth++;
				stack[depth] = { subChunks, subCount - 1, 0, false };
			}
			else//Leaf
			{
				if (!chunk.m_built && cmdb)
				{
					chunk.Build(instance, cmdb, voxelSize, forms, formsCount, trash);
				}

				if (chunk.m_built)
				{
					RenderChunk(chunk, render, bodyMin, bodyMax);
					chunk.ReleaseSubResources(instance, trash);
				}
				else
				{
					unbuiltCount++;
					RenderDanglingBranches(instance, chunk, render, bodyMin, bodyMax);
				}

				if (pos.remainder == 0)//Move up the higherarchy
				{
					depth--;
				}
				else//Move to the next cell horizontally
				{
					pos.chunk++;
					pos.remainder--;
				}
			}
		}
	} while (depth >= 0);

	delete[] stack;
	m_lastRenderSize = render.size();
	if (m_lastRenderSize > 0)
	{
		BodyRenderPackage& nbrp = instance->m_render.emplace_back();
		nbrp.transform = const_cast<glm::mat4x4&>(m_transform);
		nbrp.chunks.swap(render);
		nbrp.min = bodyMin;
		nbrp.max = bodyMax;
	}

	instance->m_workers->push(worker);
}

void VoxelBody::Deallocate(Engine* instance)
{
	m_root.ReleaseResources(instance, m_trash[0]);
	m_root.ReleaseSubResources(instance, m_trash[0]);

	for (int i = 0; i < Engine::WORKER_CMDB_COUNT; i++)
		instance->DestroyResources(m_trash[i]);

	delete[] m_trash;
}

EXPORT VoxelBody* CreateVoxelBody(glm::vec3 min, glm::vec3 max)
{
	return new VoxelBody(min, max);
}

EXPORT void SetVoxelBodyTransform(VoxelBody* voxelBody, glm::mat4x4 transform)
{
	const_cast<glm::mat4x4&>(voxelBody->m_transform) = transform;
}

EXPORT void DestroyVoxelBody(Engine* instance, VoxelBody* voxelBody)
{
	if (voxelBody)
	{
		voxelBody->Deallocate(instance);
		delete voxelBody;
	}
}

EXPORT void VBTraverse(Engine* instance, VoxelBody* vb, glm::vec3 observerPosition, float E, float voxelSize, BodyForm* forms, uint32_t formsCount, uint32_t maxDepth)
{
	observerPosition = glm::inverse(const_cast<glm::mat4x4&>(vb->m_transform)) * glm::vec4(observerPosition, 1.0);
	vb->Traverse(instance, observerPosition, E, voxelSize, forms, formsCount, maxDepth);
}