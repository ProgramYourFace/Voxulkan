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
            byte[] tessCtrlShader, int tcSize,
            byte[] tessEvalShader, int teSize,
            byte[] fragmentShader, int fsSize);
        [DllImport(DLL)]
        public static extern void SetComputeShaders(IntPtr instance,
            byte[] surfaceAnalysis, int analysisSize,
            byte[] surfaceAssembly, int assemblySize);
        [DllImport(DLL)]
        public static extern void SetMaterialResources(IntPtr instance, VoxelMaterialAttributes[] attributes, uint attribsByteCount,
            byte[] csData, uint csWidth, uint csHeight,
            byte[] nhData, uint nhWidth, uint nhHeight,
            uint materialCount);

        [DllImport(DLL)]
        public static extern void QueryOcclusion(IntPtr instance, IntPtr camera, byte workerIndex);
        [DllImport(DLL)]
        public static extern void ClearRender(IntPtr instance);
        [DllImport(DLL)]
        public static extern void SubmitQueue(IntPtr instance, byte queueIndex);
        [DllImport(DLL)]
        public static extern byte GetQueueCount(IntPtr instance);
        [DllImport(DLL)]
        public static extern void CreateVoxulkanInstance(ref IntPtr instance);
        [DllImport(DLL)]
        public static extern void DestroyVoxulkanInstance(ref IntPtr instance);
        [DllImport(DLL)]
        public static extern void InitializeVoxulkanInstance(IntPtr instance);
        [DllImport(DLL)]
        public static extern void InvokeGC(IntPtr instance);


        [DllImport(DLL)]
        public static extern IntPtr CreateVoxelBody(Vector3 min, Vector3 max);
        [DllImport(DLL)]
        public static extern void DestroyVoxelBody(IntPtr instance, IntPtr voxelBody);
        [DllImport(DLL)]
        public static extern void SetVoxelBodyTransform(IntPtr voxelBody, Matrix4x4 transform);
        [DllImport(DLL)]
        public static extern unsafe void VBTraverse(IntPtr instance, IntPtr vb, byte workerId, Vector3 observerPosition, float E, float voxelSize, void* forms, uint formsCount, uint maxDepth = 10);

        [DllImport(DLL)]
        public static extern IntPtr CreateFormPipeline(IntPtr instance,
            byte[] formShader, int shaderSize);
        [DllImport(DLL)]
        public static extern void Release(IntPtr instance, ref IntPtr resource);
        [DllImport(DLL)]
        public static extern void ReleaseHandle(IntPtr instance, ref IntPtr resourceHandle);

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