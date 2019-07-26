#version 430

layout (vertices = 3) out;

layout(push_constant) uniform CameraConstants
{
	mat4 viewProjection;
	vec3 cameraPosition;
	float tessFactor;
};

layout(location = 0) in vec3 cNrm[];
layout(location = 1) in uint cId[];

layout(location = 0) out vec3 eNrm[];
layout(location = 1) out uint eId[];
layout(location = 2) out vec3 cpI[];
layout(location = 3) out vec3 cpO[];
layout(location = 4) out vec3 cpN[];

layout(location = 5) out patch vec3 cpC;

float GetTessLevel(vec3 pos, float size)
{
	vec4 c = viewProjection * vec4(pos, 1.0);
	return max(1.0, tessFactor * abs(size / c.w));
}

const uvec3 next = uvec3(1,2,0);
const uvec3 other = uvec3(2,0,1);

#define P(i) gl_in[i].gl_Position.xyz
#define Pi  gl_in[0].gl_Position.xyz
#define Pj  gl_in[1].gl_Position.xyz
#define Pk  gl_in[2].gl_Position.xyz

vec3 ComputeCP(vec3 pA, vec3 pB, vec3 nA) {
	return (2 * pA + pB - (dot((pB - pA), nA) * nA)) / 3.0f;
}

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    eNrm[gl_InvocationID] = cNrm[gl_InvocationID];
    eId[gl_InvocationID] = cId[gl_InvocationID];

	uint nextID = next[gl_InvocationID];
	vec3 o0 = P(gl_InvocationID);
	vec3 o1 = P(nextID);
	gl_TessLevelOuter[other[gl_InvocationID]] = GetTessLevel((o0 + o1) * 0.5, distance(o0,o1));
	
	cpI[gl_InvocationID] = ComputeCP(o0, o1, cNrm[gl_InvocationID]);
	cpO[gl_InvocationID] = ComputeCP(o1, o0, cNrm[nextID]);
	vec3 delta = o1 - o0;
	vec3 nrmSum = cNrm[nextID] + cNrm[gl_InvocationID];
	cpN[gl_InvocationID] = normalize(nrmSum - (delta * 2.0 * dot(delta, nrmSum) / dot(delta, delta)));

	barrier();
	if(gl_InvocationID == 0)
	{
		gl_TessLevelInner[0] = (gl_TessLevelOuter[0] + gl_TessLevelOuter[1] + gl_TessLevelOuter[2]) / 3.0;
	}
	else if(gl_InvocationID == 1)
	{
		vec3 center = (Pi + Pj + Pk) / 3.0;
		cpC = (cpI[0] + cpO[0] + cpI[1] + cpO[1] + cpI[2] + cpO[2]) / 6.0;
		cpC += (cpC - center) / 2.0;
	}
}