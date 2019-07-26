#include "VoxelBody.h"
#include "..//Engine.h"
#include "..//Plugin.h"
#include <algorithm>
#include <sstream>

VoxelBody::VoxelBody(const glm::vec3& min, const glm::vec3& max)
{
	m_root.m_min = min;
	m_root.m_max = max;
	m_GPUConstants = new GPUBodyConstants();
}

inline float ComputeError(float distance, float size)
{
	return size / std::max(distance, 0.00001f);
}

void VoxelBody::AddChunkToRender(const VoxelChunk& chunk, std::vector<ChunkRenderPackage>& render, bool includeSub)
{
	if (chunk.m_indexCount != 0 &&
		chunk.m_vertexBuffer.m_gpuHandle != nullptr &&
		chunk.m_indexBuffer.m_gpuHandle != nullptr)
	{
		ChunkRenderPackage p;
		p.m_vertexBuffer = chunk.m_vertexBuffer.m_gpuHandle;
		p.m_indexBuffer = chunk.m_indexBuffer.m_gpuHandle;
		p.m_indexCount = chunk.m_indexCount;
		p.m_indexBuffer->Pin();
		p.m_vertexBuffer->Pin();
		render.push_back(p);
	}

	if (includeSub)
	{
		for (size_t i = 0; i < chunk.m_subChunks.size(); i++)
		{
			AddChunkToRender(chunk.m_subChunks[i], render, true);
		}
	}
}

void VoxelBody::Traverse(Engine* instance, const uint8_t& workerID, const glm::vec3& observerPosition, float E, float leafSize, ComputePipeline* form, int maxDepth)
{
	struct ChunkPosition
	{
		VoxelChunk* chunk = nullptr;
		size_t remainder = 0;
		uint32_t unbuiltCount = 0;
		bool returned = false;
	};

	WorkerResource& worker = instance->m_workers[workerID];
	if (!worker.m_recordingCmds)
	{
		VkCommandBufferBeginInfo beginI = {};
		beginI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CALL(vkBeginCommandBuffer(worker.m_CMDB, &beginI));
		worker.m_recordingCmds = true;
	}

	std::vector<ChunkRenderPackage> render(0);
	render.reserve(m_lastRenderSize);

	uint32_t unbuiltCount = 0;

	ChunkPosition* stack = new ChunkPosition[maxDepth];
	m_root.UpdateDistance(observerPosition);
	stack[0] = { &m_root, 0, 0, false};
	int depth = 0;
	do
	{
		ChunkPosition& pos = stack[depth];
		VoxelChunk& chunk = *(pos.chunk);
		if (pos.returned)//Branches would be the only thing to return
		{
			if (pos.unbuiltCount == unbuiltCount)//All children of branch are visible
			{
				chunk.ReleaseResources(instance);
				chunk.m_built = false;
			}
			else
			{
				AddChunkToRender(chunk, render);
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

			bool canBranch = depth < maxDepth - 1 && (size.x > leafSize || size.y > leafSize || size.z > leafSize);
			if (canBranch && ComputeError(chunk.m_distance, size.x * size.y * size.z) > E)//Branch
			{
				size_t subCount = chunk.m_subChunks.size();
				if (subCount == 0)
				{
					float minAxis = std::max(std::min(size.x, std::min(size.y, size.z)), leafSize);
					//float desiredBranchSize = (1 << (maxDepth - (depth + 1))) * leafSize;
					/*
					if (desiredBranchSize < minAxis)
					{
						float t = (float)(depth + 1.0f) / (float)maxDepth;
						minAxis += std::lrint(t * (float)(desiredBranchSize - minAxis));
					}*/
#define AXISCOUNT(axis) axis <= leafSize ? 1U : std::max((uint32_t)std::ceilf(axis / minAxis), 2U)
					glm::uvec3 subDiv(AXISCOUNT(size.x), AXISCOUNT(size.y), AXISCOUNT(size.z));
					glm::vec3 subSize(size.x / (float)subDiv.x, size.y / (float)subDiv.y, size.z / (float)subDiv.z);
					subCount = (size_t)subDiv.x * subDiv.y * subDiv.z;
					chunk.m_subChunks.resize(subCount);
					for (uint32_t x = 0; x < subDiv.x; x++)
					{
						for (uint32_t y = 0; y < subDiv.y; y++)
						{
							for (uint32_t z = 0; z < subDiv.z; z++)
							{
								VoxelChunk& subChunk = chunk.m_subChunks[x + y * subDiv.x + z * subDiv.x * subDiv.y];
								subChunk.m_min = {
									chunk.m_min.x + subSize.x * x,
									chunk.m_min.y + subSize.y * y,
									chunk.m_min.z + subSize.z * z };
								subChunk.m_max = { 
									subChunk.m_min.x + subSize.x,
									subChunk.m_min.y + subSize.y,
									subChunk.m_min.z + subSize.z };

								subChunk.UpdateDistance(observerPosition);
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
				if (!chunk.m_built)
				{
					if (chunk.m_stagingIdx == POOL_QUEUE_END)
					{
						chunk.BeginBuilding(instance, worker.m_CMDB, form);
					}
					else
					{
						chunk.ProcessBuilding(instance, worker.m_CMDB);
					}
				}

				if (chunk.m_built)
				{
					AddChunkToRender(chunk, render);
					chunk.ReleaseResources(instance, false, true);
				}
				else
				{
					unbuiltCount++;
					AddChunkToRender(chunk, render, true);
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
		m_GPUConstants->Pin();
		BodyRenderPackage& nbrp = worker.m_render.emplace_back();
		nbrp.m_constants = m_GPUConstants;
		nbrp.m_chunks.swap(render);
	}
}

void VoxelBody::Deallocate(Engine* instance)
{
	m_root.ReleaseResources(instance, true, true);
	instance->DestroyResource(m_GPUConstants);
}

EXPORT VoxelBody* CreateVoxelBody(glm::vec3 min, glm::vec3 max)
{
	return new VoxelBody(min, max);
}

EXPORT void DestroyVoxelBody(Engine* instance, VoxelBody*& voxelBody)
{
	voxelBody->Deallocate(instance);
	SAFE_DEL(voxelBody);
}

EXPORT void VBTraverse(Engine* instance, VoxelBody* vb, uint8_t workerID, glm::vec3 observerPosition, float E, float leafSize, ComputePipeline* form)
{
	vb->Traverse(instance, workerID, observerPosition, E, leafSize, form);
}