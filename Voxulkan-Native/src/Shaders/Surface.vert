#version 430
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNrm;
layout(location = 2) in uint vId;

layout(location = 0) out vec3 cNrm;
layout(location = 1) out uint cId;

void main()
{
	gl_Position = vec4(vPos.rgb, 1.0);
	cNrm = normalize(vNrm * 2.0 - 1.0);
	cId = vId;
}
