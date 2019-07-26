using System;
using System.Collections;
using System.Collections.Generic;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using UnityEngine;

public struct VoxelBody : IComponentData
{
    [NativeDisableUnsafePtrRestriction]
    public IntPtr m_nativeBody;
}