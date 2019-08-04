using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;
using Unity.Entities;

namespace Voxulkan
{
    public struct NativeCameraComponent : IComponentData
    {
        public IntPtr cameraHandle;
    }

    [RequireComponent(typeof(Camera))]
    public class NativeCamera : MonoBehaviour
    {
        public float tessellationFactor = 1.0f;
        public float LODThreshold = 100.0f;

        IntPtr handle;
        Entity entity;

        new Camera camera;
        CommandBuffer commandBuffer;
        const CameraEvent NATIVE_INJECTION_POINT = CameraEvent.BeforeForwardOpaque;

        [StructLayout(LayoutKind.Sequential)]
        struct CameraView
        {
            public Matrix4x4 viewProjection;
            public Vector3 cameraPos;
            public float tessellationFactor;
        }

        [DllImport(Native.DLL)]
        static extern IntPtr GetRenderInjection();
        [DllImport(Native.DLL)]
        static extern IntPtr CreateCameraHandle(IntPtr instance);
        [DllImport(Native.DLL)]
        static extern void SetCameraView(IntPtr camera, CameraView constants);

        void Awake()
        {
            camera = GetComponent<Camera>();
            camera.allowMSAA = false;
            commandBuffer = new CommandBuffer();
            commandBuffer.name = "NativeSceneInjection";
            handle = CreateCameraHandle(NativeSystem.Active.NativeInstance);
            commandBuffer.IssuePluginEventAndData(GetRenderInjection(), 1, handle);
        }

        void LateUpdate()
        {
            World.Active.GetOrCreateSystem<VoxelSystem>().m_LODThreshold = Mathf.Max(0.1f,LODThreshold);
            CameraView constants = new CameraView();
            constants.viewProjection = camera.GetNativeViewProjection();
            constants.cameraPos = transform.position;
            constants.tessellationFactor = tessellationFactor;
            SetCameraView(handle, constants);
        }

        void OnEnable()
        {
            EntityManager em = World.Active.EntityManager;
            entity = em.CreateEntity(typeof(NativeCameraComponent));
            em.SetComponentData(entity, new NativeCameraComponent() { cameraHandle = handle });
            camera.AddCommandBuffer(NATIVE_INJECTION_POINT, commandBuffer);
        }

        void OnDisable()
        {
            if (World.Active != null)
            {
                EntityManager em = World.Active.EntityManager;
                if (em != null && em.Exists(entity))
                    em.DestroyEntity(entity);
            }
            camera.RemoveCommandBuffer(NATIVE_INJECTION_POINT, commandBuffer);
        }

        void OnDestroy()
        {
            OnDisable();
            commandBuffer.Release();
            Native.ReleaseHandle(NativeSystem.Active.NativeInstance, ref handle);
        }
    }
}