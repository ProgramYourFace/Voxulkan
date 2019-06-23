#version 310 es
layout(location = 0) out highp vec4 diffuseColor;

layout(location = 0) in highp vec4 color;
void main()
{
	diffuseColor = vec4(color.rgb, 1.0);
}