#version 310 es
layout(location = 0) in highp vec3 vpos;
layout(location = 1) in highp vec4 vcol;
layout(location = 0) out highp vec4 color;
layout(push_constant) uniform PushConstants { mat4 matrix; };
void main() {
    gl_Position = matrix * vec4(vpos, 1.0);
    color = vcol;
}