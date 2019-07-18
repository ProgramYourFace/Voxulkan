#version 310 es
layout(location = 0) in vec3 vpos;
layout(location = 1) in vec3 vnrm;
layout(location = 2) in uint vid;

layout(location = 0) out highp vec4 color;
layout(location = 1) out highp vec3 normal;
layout(push_constant) uniform PushConstants { mat4 matrix; };

void main() {
    gl_Position = matrix * vec4(vpos.rgb, 1.0);
	normal = normalize(vnrm * 2.0 - 1.0);
    color = vec4(abs(normal),1.0);
}