#version 310 es
layout(location = 0) out highp vec4 diffuseColor;

layout(location = 0) in highp vec4 vColor;
layout(location = 1) in highp vec3 vNormal;
void main()
{
	diffuseColor = vColor * dot(vNormal, vec3(-0.707, 0.0, -0.707));
}