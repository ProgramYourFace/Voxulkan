using System;
using System.Runtime.InteropServices;
using Unity.Entities;
using UnityEngine;

[InternalBufferCapacity(8)]
[StructLayout(LayoutKind.Sequential)]
public struct BodyForm : IBufferElementData
{
    public Vector3 min;
    public Vector3 max;
    public IntPtr formCompute;
}
