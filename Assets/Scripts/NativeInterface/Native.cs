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
        public static extern bool IsInitialized();
        [DllImport(DLL)]
        public static extern void RegisterLogCallback(LogCallback callback);
        [DllImport(DLL)]
        public static extern void SetChunkShaders(byte[] vertexShader, int vsSize, byte[] fragmentShader, int fsSize);

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