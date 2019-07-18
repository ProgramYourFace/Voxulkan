using System;
using System.Collections;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Voxulkan
{

#if UNITY_EDITOR
    [UnityEditor.InitializeOnLoad]
#endif
    public static class Native
    {
#if UNITY_EDITOR
        static Native()
        {
            RegisterLogCallback(NativeLogger);
        }
#endif
        public const string DLL = "GfxPluginVoxulkan-Native";

        public delegate void LogCallback(string msg);

        [DllImport(DLL)]
        public static extern void RegisterLogCallback(LogCallback callback);
        [DllImport(DLL)]
        public static extern void SetSurfaceShaders(IntPtr instance,
            byte[] vertexShader, int vsSize,
            byte[] fragmentShader, int fsSize);
        [DllImport(DLL)]
        public static extern void SetComputeShaders(IntPtr instance,
            byte[] surfaceAnalysis, int analysisSize,
            byte[] surfaceAssembly, int assemblySize);
        [DllImport(DLL)]
        public static extern void CreateVoxulkanInstance(ref IntPtr instance);
        [DllImport(DLL)]
        public static extern void DestroyVoxulkanInstance(ref IntPtr instance);
        [DllImport(DLL)]
        public static extern void InitializeVoxulkanInstance(IntPtr instance);
        [DllImport(DLL)]
        public static extern void InvokeGC(IntPtr instance);


        [DllImport(DLL)]
        public static extern void ComputeTest(IntPtr instance, IntPtr compute);
        [DllImport(DLL)]
        public static extern IntPtr CreateFormPipeline(IntPtr instance,
            byte[] formShader, int shaderSize);
        [DllImport(DLL)]
        public static extern void Release(IntPtr instance, ref IntPtr resource);

        public static byte[] LoadShaderBytes(string shaderName)
        {
            return Resources.Load<TextAsset>("NativeShaders/" + shaderName).bytes;
        }
        public static Matrix4x4 GetNativeViewProjection(this Camera camera)
        {
            return GL.GetGPUProjectionMatrix(camera.projectionMatrix, false) * camera.worldToCameraMatrix;
        }
        public static void NativeLogger(string msg)
        {
            Debug.Log("<color=#0000ff>[" + DLL + "]: " + msg + "</color>");
        }
    }
}