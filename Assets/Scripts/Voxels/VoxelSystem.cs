using System;
using System.Collections;
using System.Collections.Generic;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using Unity.Jobs;
using UnityEngine;

namespace Voxulkan
{
    public class VoxelSystem : ComponentSystem
    {
        struct VoxelBodyTraverseJob : IJobForEach<VoxelBody>
        {
            [NativeDisableUnsafePtrRestriction]
            public IntPtr m_instance;
            [NativeDisableUnsafePtrRestriction]
            public IntPtr m_form;
            public Vector3 m_observerPosition;

            [NativeSetThreadIndex]
            int m_ThreadIndex;

            public void Execute(ref VoxelBody vb)
            {
                Native.VBTraverse(m_instance, vb.m_nativeBody, (byte)m_ThreadIndex, m_observerPosition, 10.0f, 32.0f, m_form);
            }
        }
        struct SubmitRenderJob : IJob
        {
            [NativeDisableUnsafePtrRestriction]
            public IntPtr m_instance;
            public void Execute()
            {
                Native.SubmitRender(m_instance);
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

        NativeSystem m_nativeSystem;
        IntPtr m_sphereForm;

        IntPtr m_testVB;
        JobHandle m_jobs = default;

        public void OnNativeInitialized(NativeSystem nativeSystem)
        {
            m_nativeSystem = nativeSystem;
            byte[] sphereFormShader = Native.LoadShaderBytes("SphereForm.comp");
            m_sphereForm = Native.CreateFormPipeline(m_nativeSystem.NativeInstance, sphereFormShader, sphereFormShader.Length);

            m_testVB = Native.CreateVoxelBody(Vector3.one * -128.0f, Vector3.one * 128.0f);
            Entity e = EntityManager.CreateEntity(typeof(VoxelBody));
            EntityManager.SetComponentData(e, new VoxelBody() { m_nativeBody = m_testVB });
        }

        protected override void OnUpdate()
        {
            IntPtr instance = m_nativeSystem.NativeInstance;
            if (m_jobs.IsCompleted && instance != IntPtr.Zero)
            {
                JobHandle traverseJob = new VoxelBodyTraverseJob() {
                    m_instance = instance,
                    m_form = m_sphereForm,
                    m_observerPosition = Camera.main.transform.position
                }.Schedule(this, m_jobs);

                JobHandle rJob = new SubmitRenderJob() { m_instance = instance }.Schedule(traverseJob);
                JobHandle qJob = new SubmitQueuesJob() { m_instance = instance }.Schedule(m_nativeSystem.QueueCount, 1, traverseJob);
                m_jobs = JobHandle.CombineDependencies(rJob, qJob);
            }
        }

        internal void OnNativeDeinitialized()
        {
            m_jobs.Complete();
            Native.DestroyVoxelBody(m_nativeSystem.NativeInstance, ref m_testVB);
            Native.Release(m_nativeSystem.NativeInstance, ref m_sphereForm);
        }
    }
}