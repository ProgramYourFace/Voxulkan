using System.Collections;
using System.Collections.Generic;
using UnityEditor;
using UnityEditorInternal;
using UnityEngine;

namespace Voxulkan
{
    [CustomEditor(typeof(VoxelMaterialDatabase))]
    public class VoxelMaterialDatabaseEditor : Editor
    {
        ReorderableList matList;
        private void OnEnable()
        {
            matList = new ReorderableList(serializedObject, serializedObject.FindProperty("m_materials"));
            const float M = 2.0f;
            float LH = EditorGUIUtility.singleLineHeight + M;
            matList.elementHeight = LH * 6 + M;
            matList.displayAdd = true;
            matList.displayRemove = true;
            matList.drawHeaderCallback = rect => {
                EditorGUI.LabelField(rect, "Voxel Materials", EditorStyles.boldLabel);
            };

            matList.drawElementCallback = (Rect rect, int index, bool isActive, bool isFocused) =>
            {
                var element = matList.serializedProperty.GetArrayElementAtIndex(index);
                rect.height = EditorGUIUtility.singleLineHeight;
                rect.y += M;
                SerializedProperty nameP = element.FindPropertyRelative("name");
                if (isActive)
                {
                    string head = index + ":";
                    float width = 8.0f * head.Length;
                    EditorGUI.LabelField(new Rect(rect.x, rect.y, width, rect.height), index + ":");
                    nameP.stringValue = EditorGUI.TextField(new Rect(rect.x + width, rect.y, rect.width - width, rect.height), nameP.stringValue);
                }
                else
                    EditorGUI.LabelField(rect, index+ ": " + nameP.stringValue);

                rect.y += LH;
                EditorGUI.PropertyField(rect, element.FindPropertyRelative("map"));

                rect.y += LH;
                EditorGUI.PropertyField(rect, element.FindPropertyRelative("attributes.tint"));
                rect.y += LH;
                EditorGUI.PropertyField(rect, element.FindPropertyRelative("attributes.size"), true);
                rect.y += LH;
                EditorGUI.PropertyField(rect, element.FindPropertyRelative("attributes.tessHeight"), true);
                rect.y += LH;
                EditorGUI.PropertyField(rect, element.FindPropertyRelative("attributes.tessCenter"), true);
            };
        }
        public override void OnInspectorGUI()
        {
            base.OnInspectorGUI();
            serializedObject.Update();
            matList.DoLayoutList();
            serializedObject.ApplyModifiedProperties();
        }
    }
}