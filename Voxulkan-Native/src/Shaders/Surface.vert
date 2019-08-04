#version 430
layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNrm;
layout(location = 2) in uint vID;

layout(location = 0) out vec4 outMPos;
layout(location = 1) out vec4 outMNrm;
layout(location = 2) out uint outID;

layout(push_constant) uniform Constants
{
	mat4 mvp;
	mat4 model;
	vec3 cameraPosition;
	float tessFactor;
};

#define CLIP_SCALE 1.1

void main()
{
	vec4 clip = mvp * vec4(vPos.rgb, 1.0);
	clip.xyz /= clip.w;
	gl_Position = vec4(
	step(CLIP_SCALE, clip.x)-step(clip.x, -CLIP_SCALE),
	step(CLIP_SCALE, clip.y)-step(clip.y, -CLIP_SCALE),
	step(CLIP_SCALE, clip.z)-step(clip.z, 0),1.0);

	outID = vID;
	outMPos.xyz = vPos;
	outMNrm.xyz = normalize(vNrm * 2.0 - 1.0);
	vec3 viewDir = (model * vec4(vPos, 1.0)).xyz - cameraPosition;
	float viewDist = length(viewDir);
	viewDir /= viewDist;
	outMNrm.w = dot((model * vec4(outMNrm.xyz, 0.0)).xyz, viewDir);
	outMPos.w = viewDist;
}
