using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;
using Unity.Entities;

namespace Voxulkan
{
    [RequireComponent(typeof(Camera))]
    public class NativeCamera : MonoBehaviour
    {
        public float tessellationFactor = 1.0f;

        IntPtr nativeCamera;
        new Camera camera;
        CommandBuffer commandBuffer;
        const CameraEvent NATIVE_INJECTION_POINT = CameraEvent.BeforeForwardOpaque;

        [StructLayout(LayoutKind.Sequential)]
        struct CameraConstants
        {
            public Matrix4x4 viewProjection;
            public Vector3 cameraPos;
            public float tessellationFactor;
        }

        [DllImport(Native.DLL)]
        static extern IntPtr GetRenderInjection();
        [DllImport(Native.DLL)]
        static extern IntPtr CreateNativeCamera(IntPtr instance);
        [DllImport(Native.DLL)]
        static extern void DestroyNativeCamera(IntPtr camera);
        [DllImport(Native.DLL)]
        static extern void SetCameraVP(IntPtr camera, CameraConstants constants);

        void Awake()
        {
            camera = GetComponent<Camera>();
            camera.allowMSAA = false;
            commandBuffer = new CommandBuffer();
            commandBuffer.name = "NativeSceneInjection";
            nativeCamera = CreateNativeCamera(NativeSystem.Active.NativeInstance);
            commandBuffer.IssuePluginEventAndData(GetRenderInjection(), 1, nativeCamera);
        }

        void OnPreRender()
        {
            CameraConstants constants = new CameraConstants();
            constants.viewProjection = camera.GetNativeViewProjection();
            constants.cameraPos = transform.position;
            constants.tessellationFactor = tessellationFactor;
            SetCameraVP(nativeCamera, constants);
        }

        void OnEnable()
        {
            camera.AddCommandBuffer(NATIVE_INJECTION_POINT, commandBuffer);
        }

        void OnDisable()
        {
            camera.RemoveCommandBuffer(NATIVE_INJECTION_POINT, commandBuffer);
        }

        void OnDestroy()
        {
            OnDisable();
            commandBuffer.Release();
            DestroyNativeCamera(nativeCamera);
            nativeCamera = IntPtr.Zero;
        }
    }
}