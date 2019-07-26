using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using System.IO;
using System;

namespace Voxulkan
{
    public class VoxelMapGenerator : EditorWindow
    {
        public int filterRadius = 2;

        [MenuItem("Window/Voxel Map Generator")]
        static void Init()
        {
            VoxelMapGenerator window = (VoxelMapGenerator)EditorWindow.GetWindow(typeof(VoxelMapGenerator), true, "Voxel Map Generator", true);
            window.minSize = new Vector2(200, 100);
            window.maxSize = window.minSize;
            window.Show();
        }

        private void OnGUI()
        {
            filterRadius = EditorGUILayout.IntSlider("Nrm Filter Radius",filterRadius, 1, 5);

            if (GUILayout.Button("Generate"))
            {
                Texture2D colorMap = null;
                Texture2D heightMap = null;

                string cPath = "";

                foreach (UnityEngine.Object obj in Selection.GetFiltered(typeof(UnityEngine.Object), SelectionMode.Assets))
                {
                    string path = AssetDatabase.GetAssetPath(obj);
                    if (!string.IsNullOrEmpty(path) && File.Exists(path) && obj.GetType() == typeof(Texture2D))
                    {
                        string n = obj.name.ToLower();
                        if (n.Contains("albedo") || n.Contains("color"))
                        {
                            colorMap = (Texture2D)obj;
                            cPath = Path.GetDirectoryName(path);
                        }
                        else if (n.Contains("height"))
                        {
                            heightMap = (Texture2D)obj;
                        }
                    }
                }

                if (colorMap && heightMap)
                {
                    cPath += "\\" + Path.GetFileName(cPath) + "_VMap.bytes";
                    Generate(cPath, colorMap, heightMap);
                    Debug.Log(cPath);
                    AssetDatabase.Refresh();
                }
            }
        }

        [System.Runtime.InteropServices.DllImport(Native.DLL)]
        public static extern void FillMapData(Color32[] colorSpecData, uint csSize, Color32[] nrmHeightData, uint nhSize, byte[] byteData);

        public void Generate(string path, Texture2D colorMap, Texture2D heightMap)
        {
            //Vector2Int csSize = new Vector2Int(colorMap.width, colorMap.height);
            Vector2Int nhSize = new Vector2Int(heightMap.width, heightMap.height);
            Color32[] csData = colorMap.GetPixels32();
            Color[] hData = heightMap.GetPixels();

            Color32[] nhData = new Color32[hData.Length];

            for (int x = 0; x < nhSize.x; x++)
            {
                for (int y = 0; y < nhSize.y; y++)
                {
                    int i = GetNHIndex(x, y, nhSize);
                    float h = hData[i].r;

                    Vector3 nrm = Vector3.zero;
                    for (int ox = -filterRadius; ox <= filterRadius; ox++)
                    {
                        for (int oy = -filterRadius; oy <= filterRadius; oy++)
                        {
                            if (ox == 0 && oy == 0)
                                continue;

                            nrm += ComputeNrm(h, hData[GetNHIndex(x + ox, y + oy, nhSize)].r, ox, oy, nhSize);
                        }
                    }

                    nrm = nrm.normalized + new Vector3(1.0f, 1.0f, 0.0f);
                    nrm.Scale(new Vector3(127.5f, 127.5f, 0.0f));

                    int hi = Mathf.Clamp(Mathf.RoundToInt(h * 256 * 256), 0, ushort.MaxValue);
                    byte scale = (byte)Mathf.Clamp((hi / 256), 0, 255);
                    byte frac = (byte)Mathf.Clamp((hi % 256), 0, 255);
                    if (hi == ushort.MaxValue)
                    {
                        scale = 255;
                        frac = 255;
                    }

                    Color32 nrmH = new Color32((byte)Mathf.Clamp(Mathf.RoundToInt(nrm.x), 0, 255),
                        (byte)Mathf.Clamp(Mathf.RoundToInt(nrm.y), 0, 255),
                        scale,
                        frac);

                    nhData[i] = nrmH;
                }
            }

            byte[] data = new byte[(csData.Length + nhData.Length) * 4];
            FillMapData(csData, (uint)csData.Length, nhData, (uint)nhData.Length, data);
            File.WriteAllBytes(path, data);
        }

        Vector3 ComputeNrm(float center, float other, int ox, int oy, Vector2Int size)
        {
            Vector3 delta = new Vector3((float)ox / size.x, (float)oy / size.y, (other - center));
            Vector3 cross = Vector3.Cross(Vector3.forward, delta);
            return Vector3.Cross(delta, cross).normalized;
        }

        int GetNHIndex(int x, int y, Vector2Int size)
        {
            x = x < 0 ? x + size.x : x >= size.x ? x - size.x : x;
            y = y < 0 ? y + size.y : y >= size.y ? y - size.y : y;

            return x + y * size.x;
        }
    }
}