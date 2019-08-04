#version 430

layout (vertices = 3) out;

layout(push_constant) uniform Constants
{
	mat4 mvp;
	mat4 model;
	vec3 cameraPosition;
	float tessFactor;
};

layout(location = 0) in vec4 inMPos[];
layout(location = 1) in vec4 inMNrm[];
layout(location = 2) in uint inID[];

layout(location = 0) out vec3 outMNrm[];
layout(location = 1) out uint outID[];
layout(location = 2) out patch vec4 outCP[10];

float GetTessLevel(vec3 pos, float size)
{
	vec4 c = mvp * vec4(pos, 1.0);
	return max(1.0, tessFactor * abs(size / c.w));
}

const uvec3 next = uvec3(1,2,0);

vec3 ComputeCP(vec3 pA, vec3 pB, vec3 nA) {
	return (2 * pA + pB - (dot((pB - pA), nA) * nA)) / 3.0f;
}

void main()
{
	gl_out[gl_InvocationID].gl_Position = inMPos[gl_InvocationID];
    outMNrm[gl_InvocationID] = inMNrm[gl_InvocationID].xyz;
    outID[gl_InvocationID] = inID[gl_InvocationID];

	uint nextID = next[gl_InvocationID];
	uint lastID = next[nextID];
	vec3 o0 = inMPos[gl_InvocationID].xyz;
	vec3 o1 = inMPos[nextID].xyz;
	gl_TessLevelOuter[lastID] = GetTessLevel((o0 + o1) * 0.5, distance(o0,o1));
	
	vec3 mb = vec3(0.0);
	mb[gl_InvocationID] = 1.0;
	mb[nextID] = 1.0 - abs(sign(inID[gl_InvocationID] - inID[nextID]));
	mb[lastID] = 1.0 - abs(sign(inID[gl_InvocationID] - inID[lastID]));

	uint CPI = gl_InvocationID * 3;
	outCP[CPI] = vec4(ComputeCP(o0, o1, inMNrm[gl_InvocationID].xyz), mb.x);
	outCP[CPI + 1] = vec4(ComputeCP(o1, o0, inMNrm[nextID].xyz), mb.y);
	vec3 delta = o1 - o0;
	vec3 nrmSum = inMNrm[nextID].xyz + inMNrm[gl_InvocationID].xyz;
	outCP[CPI + 2] = vec4(normalize(nrmSum - (delta * 2.0 * dot(delta, nrmSum) / dot(delta, delta))), mb.z);

	barrier();
	if(gl_InvocationID == 0)
	{
		float dist = min(inMPos[0].w, min(inMPos[1].w, inMPos[2].w));
		float facing = min(inMNrm[0].w, min(inMNrm[1].w, inMNrm[2].w));
		
		vec3 clipMask = max(abs(gl_in[0].gl_Position.xyz + gl_in[1].gl_Position.xyz + gl_in[2].gl_Position.xyz) - 2.0, vec3(0.0));
		float cull = min(step(1.0, dist) * (step(0.15, facing) + clipMask.x + clipMask.y + clipMask.z), 1.0) * -2.0 + 1.0;

		gl_TessLevelOuter[0] *= cull;
		gl_TessLevelOuter[1] *= cull;
		gl_TessLevelOuter[2] *= cull;

		gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) / 3.0;
	}
	else if(gl_InvocationID == 1)
	{
		vec3 center = (inMPos[0].xyz + inMPos[1].xyz + inMPos[2].xyz) / 3.0;
		outCP[9].xyz = (outCP[0].xyz + outCP[1].xyz + outCP[3].xyz + outCP[4].xyz + outCP[6].xyz + outCP[7].xyz) / 6.0;
		outCP[9].xyz += (outCP[9].xyz - center) / 2.0;
	}
}