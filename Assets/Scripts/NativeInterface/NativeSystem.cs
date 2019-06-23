using System.Collections;
using System.Collections.Generic;
using Unity.Entities;
using UnityEngine;

namespace Voxulkan
{
    public class NativeSystem : ComponentSystem
    {
        [System.Runtime.InteropServices.DllImport(Native.DLL)]
        public static extern void TEST_RunCompute(byte[] shader, int size);
        protected override void OnStartRunning()
        {
            byte[] vertexShader = Native.LoadShaderBytes("Chunk.vert");
            byte[] fragmentShader = Native.LoadShaderBytes("Chunk.frag");
            Native.SetChunkShaders(vertexShader, vertexShader.Length, fragmentShader, fragmentShader.Length);
        }

        protected override void OnStopRunning()
        {
        }

        protected override void OnUpdate()
        {
            if (Input.GetKeyDown(KeyCode.T))
            {
                byte[] computeShader = Native.LoadShaderBytes("SphereVolume.comp");
                TEST_RunCompute(computeShader, computeShader.Length);
            }
        }
    }
}