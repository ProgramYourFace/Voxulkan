using System;
using System.Collections;
using System.Collections.Generic;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using UnityEngine;

namespace Voxulkan
{
    public struct VoxelBody : IComponentData
    {
        [NativeDisableUnsafePtrRestriction]
        public IntPtr m_nativeBody;
        public bool m_destroy;
    }
}