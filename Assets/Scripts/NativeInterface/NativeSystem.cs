using System;
using System.Collections;
using System.Collections.Generic;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using Unity.Jobs;
using UnityEngine;

namespace Voxulkan
{
    public class NativeSystem : JobComponentSystem
    {
        struct InvokeGCJob : IJob
        {
            [NativeDisableUnsafePtrRestriction]
            public IntPtr instance;
            public void Execute()
            {
                Native.InvokeGC(instance);
            }
        }

        VoxelSystem m_voxelSystem;
        IntPtr m_nativeInstance;
        byte m_queueCount;
        protected override void OnCreate()
        {
            Native.CreateVoxulkanInstance(ref m_nativeInstance);
            byte[] vertexShader = Native.LoadShaderBytes("Surface.vert");
            byte[] tessCtrlShader = Native.LoadShaderBytes("Surface.tesc");
            byte[] tessEvalShader = Native.LoadShaderBytes("Surface.tese");
            byte[] fragmentShader = Native.LoadShaderBytes("Surface.frag");
            Native.SetSurfaceShaders(m_nativeInstance, 
                vertexShader, vertexShader.Length,
                tessCtrlShader, tessCtrlShader.Length,
                tessEvalShader, tessEvalShader.Length,
                fragmentShader, fragmentShader.Length);
            byte[] surfaceAnalysis = Native.LoadShaderBytes("SurfaceAnalysis.comp");
            byte[] surfaceAssembly = Native.LoadShaderBytes("SurfaceAssembly.comp");
            Native.SetComputeShaders(m_nativeInstance, surfaceAnalysis, surfaceAnalysis.Length, surfaceAssembly, surfaceAssembly.Length);

            Resources.Load<VoxelMaterialDatabase>("Voxel Materials").SetInstanceResources(m_nativeInstance);

            Native.InitializeVoxulkanInstance(m_nativeInstance);
            m_queueCount = Native.GetQueueCount(m_nativeInstance);

            m_voxelSystem = World.GetOrCreateSystem<VoxelSystem>();
            m_voxelSystem.OnNativeInitialized(this);
        }

        protected override void OnDestroy()
        {
            m_voxelSystem.OnNativeDeinitialized();
            Native.DestroyVoxulkanInstance(ref m_nativeInstance);
        }

        protected override JobHandle OnUpdate(JobHandle inputDeps)
        {
            if (m_nativeInstance != IntPtr.Zero)
            {
                inputDeps = new InvokeGCJob() { instance = m_nativeInstance }.Schedule(inputDeps);//TODO: Maybe scedule less frequently
            }
            return inputDeps;
        }

        public IntPtr NativeInstance { get { return m_nativeInstance; } }
        public byte QueueCount { get { return m_queueCount; } }
        public static NativeSystem Active { get { return World.Active.GetExistingSystem<NativeSystem>(); } }
    }
}