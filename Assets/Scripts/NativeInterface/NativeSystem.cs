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

        IntPtr m_nativeInstance;
        protected override void OnCreate()
        {
            Native.CreateVoxulkanInstance(ref m_nativeInstance);
            byte[] vertexShader = Native.LoadShaderBytes("Surface.vert");
            byte[] fragmentShader = Native.LoadShaderBytes("Surface.frag");
            Native.SetSurfaceShaders(m_nativeInstance, vertexShader, vertexShader.Length, fragmentShader, fragmentShader.Length);
            Native.InitializeVoxulkanInstance(m_nativeInstance);
        }

        protected override void OnDestroy()
        {
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
        public static NativeSystem Active { get { return World.Active.GetExistingSystem<NativeSystem>(); } }
    }
}