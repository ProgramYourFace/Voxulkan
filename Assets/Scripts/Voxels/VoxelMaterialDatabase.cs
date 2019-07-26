using System.Collections;
using System.Collections.Generic;
using System;
using UnityEngine;
using System.Runtime.InteropServices;

namespace Voxulkan
{
    [Serializable]
    [StructLayout(LayoutKind.Sequential, Size = 32)]
    public struct VoxelMaterialAttributes
    {
        public Color tint;
        public Vector2 size;
        public float tessHeight;
        public float tessCenter;
    }
    [Serializable]
    public struct VoxelMaterial
    {
        public string name;
        public TextAsset map;
        public VoxelMaterialAttributes attributes;
    }
    [CreateAssetMenu(fileName = "Voxel Materials", menuName = "Voxel Material/Voxel Material Database", order = 300)]
    public class VoxelMaterialDatabase : ScriptableObject
    {
        [SerializeField] private Vector2Int m_colorSpecRes = new Vector2Int(1024,1024);
        [SerializeField] private Vector2Int m_nrmHeightRes = new Vector2Int(1024, 1024);
        [SerializeField] [HideInInspector] private VoxelMaterial[] m_materials = new VoxelMaterial[1];

        public void SetInstanceResources(IntPtr instance)
        {
            int csStride = m_colorSpecRes.x * m_colorSpecRes.y * 4;
            int nhStride = m_nrmHeightRes.x * m_nrmHeightRes.y * 4;
            byte[] colorSpec = new byte[csStride * m_materials.Length];
            byte[] nrmHeight = new byte[nhStride * m_materials.Length];
            VoxelMaterialAttributes[] attribs = new VoxelMaterialAttributes[m_materials.Length];

            for (int i = 0; i < m_materials.Length; i++)
            {
                VoxelMaterial vm = m_materials[i];
                attribs[i] = vm.attributes;
                if (vm.map)
                {
                    byte[] mb = vm.map.bytes;

                    Array.Copy(mb, 0, colorSpec, csStride * i, csStride);
                    Array.Copy(mb, csStride, nrmHeight, nhStride * i, nhStride);
                    Resources.UnloadAsset(vm.map);
                }
            }

            Native.SetMaterialResources(instance, attribs, (uint)attribs.Length * 32,
                colorSpec, (uint)m_colorSpecRes.x, (uint)m_colorSpecRes.x,
                nrmHeight, (uint)m_nrmHeightRes.x, (uint)m_nrmHeightRes.x,
                (uint)m_materials.Length);
        }
    }
}