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
        IntPtr nativeCamera;
        new Camera camera;
        CommandBuffer commandBuffer;
        const CameraEvent NATIVE_INJECTION_POINT = CameraEvent.BeforeForwardOpaque;

        [DllImport(Native.DLL)]
        public static extern IntPtr GetRenderInjection();
        [DllImport(Native.DLL)]
        public static extern IntPtr CreateNativeCamera(IntPtr instance);
        [DllImport(Native.DLL)]
        public static extern void DestroyNativeCamera(IntPtr camera);
        [DllImport(Native.DLL)]
        public static extern void SetCameraVP(IntPtr camera, Matrix4x4 mvp);

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
            SetCameraVP(nativeCamera, camera.GetNativeViewProjection());
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