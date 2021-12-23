using System;
using System.Collections;
using System.Collections.Generic;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using Unity.Jobs;
using Unity.Jobs.LowLevel.Unsafe;
using UnityEngine;

namespace Voxulkan
{
    public class VoxelCMDBSystem : EntityCommandBufferSystem { }

    [UpdateAfter(typeof(NativeSystem))]
    public class VoxelSystem : JobComponentSystem
    {
        struct VoxelBodyTraverseJob : IJobParallelFor
        {
            [NativeDisableUnsafePtrRestriction]
            [ReadOnly] public IntPtr m_instance;
            [ReadOnly] public Vector3 m_observerPosition;
            [ReadOnly] public float m_errorThreshold;
            public EntityCommandBuffer.Concurrent m_cmdb;

            [NativeDisableParallelForRestriction]
            [DeallocateOnJobCompletion]
            public NativeArray<Entity> m_entities;
            [NativeDisableParallelForRestriction]
            public ComponentDataFromEntity<VoxelBody> m_voxelBodies;
            [NativeDisableParallelForRestriction]
            public BufferFromEntity<BodyForm> m_formsBuffers;

            public void Execute(int index)
            {
                Entity entity = m_entities[index];
                VoxelBody vb = m_voxelBodies[entity];
                if (vb.m_destroy)
                {
                    Native.DestroyVoxelBody(m_instance, vb.m_nativeBody);
                    vb.m_nativeBody = IntPtr.Zero;
                    m_cmdb.DestroyEntity(index, entity);
                }
                else
                {
                    DynamicBuffer<BodyForm> forms = m_formsBuffers[entity];
                    if (forms.Length == 0)
                        return;
                    unsafe
                    {
                        Native.VBTraverse(m_instance,
                            vb.m_nativeBody,
                            m_observerPosition,
                            m_errorThreshold,
                            1.0f,
                            forms.GetUnsafePtr(),
                            (uint)forms.Length,
                            10);
                    }
                }
            }
        }
        struct OcclusionQueryJob : IJobParallelFor
        {
            [NativeDisableUnsafePtrRestriction]
            public IntPtr m_instance;
            [NativeDisableParallelForRestriction]
            [DeallocateOnJobCompletion]
            public NativeArray<NativeCameraComponent> m_cameras;

            public void Execute(int index)
            {
                Native.QueryOcclusion(m_instance, m_cameras[index].cameraHandle);
            }
        }
        struct ClearRenderJob : IJob
        {
            [NativeDisableUnsafePtrRestriction]
            public IntPtr m_instance;
            public void Execute()
            {
                Native.ClearRender(m_instance);
            }
        }
        struct SubmitQueuesJob : IJobParallelFor
        {
            [NativeDisableUnsafePtrRestriction]
            public IntPtr m_instance;
            public void Execute(int index)
            {
                Native.SubmitQueue(m_instance, (byte)index);
            }
        }

        public float m_LODThreshold = 100.0f;

        NativeSystem m_nativeSystem = null;
        IntPtr m_sphereForm;

        VoxelCMDBSystem m_cmdbSystem = null;
        JobHandle m_updateJob;

        public void OnNativeInitialized(NativeSystem nativeSystem)
        {
            m_cmdbSystem = World.GetOrCreateSystem<VoxelCMDBSystem>();
            m_nativeSystem = nativeSystem;
            byte[] sphereFormShader = Native.LoadShaderBytes("SphereForm.comp");
            m_sphereForm = Native.CreateFormPipeline(m_nativeSystem.NativeInstance, sphereFormShader, sphereFormShader.Length);

            BodyForm[] forms = new BodyForm[1];
            forms[0].formCompute = m_sphereForm;
            forms[0].min = -Vector3.one * -900;
            forms[0].max = Vector3.one * 900;
            CreateVoxelBody(Vector3.one * -900, Vector3.one * 900, Vector3.forward * 1000.0f, Quaternion.Euler(50, 12, 42), forms);
            CreateVoxelBody(Vector3.one * -900, Vector3.one * 900, Vector3.back * 1000.0f, Quaternion.Euler(-25, 25, 10), forms);
            CreateVoxelBody(Vector3.one * -900, Vector3.one * 900, Vector3.left * 1000.0f, Quaternion.Euler(50, 12, 42), forms);
            CreateVoxelBody(Vector3.one * -900, Vector3.one * 900, Vector3.right * 1000.0f, Quaternion.Euler(-25, 25, 10), forms);
        }

        public Entity CreateVoxelBody(Vector3 min, Vector3 max, Vector3 position, Quaternion rotation, BodyForm[] forms = null)
        {
            Entity entity = EntityManager.CreateEntity(typeof(VoxelBody), typeof(BodyForm));
            if (forms != null)
            {
                DynamicBuffer<BodyForm> bForms = EntityManager.GetBuffer<BodyForm>(entity);
                for (int i = 0; i < forms.Length; i++)
                    bForms.Add(forms[i]);
            }
            VoxelBody vb = new VoxelBody();
            vb.m_nativeBody = Native.CreateVoxelBody(min, max);
            Native.SetVoxelBodyTransform(vb.m_nativeBody, Matrix4x4.TRS(position, rotation, Vector3.one));
            vb.m_destroy = false;
            EntityManager.SetComponentData(entity, vb);
            return entity;
        }

        public void DeleteVoxelBody(Entity body)
        {
            VoxelBody vb = EntityManager.GetComponentData<VoxelBody>(body);
            vb.m_destroy = true;
            EntityManager.SetComponentData(body, vb);
        }

        protected override JobHandle OnUpdate(JobHandle inputDeps)
        {
            if (m_nativeSystem == null)
                return inputDeps;

            EntityQuery vbQuery = World.Active.EntityManager.CreateEntityQuery(typeof(VoxelBody), typeof(BodyForm));

            IntPtr instance = m_nativeSystem.NativeInstance;
            JobHandle queryJob;
            JobHandle traverseJob = new VoxelBodyTraverseJob()
            {
                m_instance = instance,
                m_observerPosition = Camera.main.transform.position,
                m_errorThreshold = m_LODThreshold,
                m_cmdb = m_cmdbSystem.CreateCommandBuffer().ToConcurrent(),
                m_entities = vbQuery.ToEntityArray(Allocator.TempJob, out queryJob),
                m_voxelBodies = GetComponentDataFromEntity<VoxelBody>(false),//TODO: See about making read only
                m_formsBuffers = GetBufferFromEntity<BodyForm>(false)
            }.Schedule(vbQuery.CalculateEntityCount(), 1, JobHandle.CombineDependencies(inputDeps, queryJob));

            m_cmdbSystem.AddJobHandleForProducer(traverseJob);

            EntityQuery cameraQuery = World.Active.EntityManager.CreateEntityQuery(typeof(NativeCameraComponent));

            
            JobHandle occlusionQueryJob = new OcclusionQueryJob()
            {
                m_instance = instance,
                m_cameras = cameraQuery.ToComponentDataArray<NativeCameraComponent>(Allocator.TempJob, out queryJob)
            }.Schedule(cameraQuery.CalculateEntityCount(), 1, JobHandle.CombineDependencies(traverseJob, queryJob));//, m_nativeSystem.GCJob

    JobHandle submitQueueJob = new SubmitQueuesJob()
            {
                m_instance = instance
            }.Schedule(m_nativeSystem.QueueCount, 1, traverseJob);

            m_updateJob = new ClearRenderJob()
            {
                m_instance = instance
            }.Schedule(JobHandle.CombineDependencies(occlusionQueryJob, submitQueueJob));

            vbQuery.Dispose();
            cameraQuery.Dispose();

            return m_updateJob;
        }
        public void OnNativeDeinitialized()
        {
            DeleteAllVoxelBodies();
            Native.Release(m_nativeSystem.NativeInstance, ref m_sphereForm);
            m_nativeSystem = null;
        }
        public void DeleteAllVoxelBodies()
        {
            m_updateJob.Complete();
            EntityQuery eq = World.Active.EntityManager.CreateEntityQuery(typeof(VoxelBody));
            NativeArray<VoxelBody> vbs = eq.ToComponentDataArray<VoxelBody>(Allocator.TempJob);
            for (int i = 0; i < vbs.Length; i++)
            {
                Native.DestroyVoxelBody(m_nativeSystem.NativeInstance, vbs[i].m_nativeBody);
            }
            vbs.Dispose();
            eq.Dispose();
        }
    }
}