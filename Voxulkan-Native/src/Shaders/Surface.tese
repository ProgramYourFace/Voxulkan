#version 430

layout(triangles, fractional_odd_spacing, cw) in;

layout(push_constant) uniform Constants
{	
	mat4 mvp;
	mat4 model;
	vec3 cameraPosition;
	float tessFactor;
};

layout(location = 0) in vec3 inMNrm[];
layout(location = 1) in uint inID[];
layout(location = 2) in patch vec4 inCP[10];

layout(location = 0) out vec3 modelPos;
layout(location = 1) out vec3 modelNrm;
layout(location = 2) out vec3 worldPos;
layout(location = 3) out vec3 worldNrm;
layout(location = 4) out vec3 matBlend;
layout(location = 5) out flat uint matID[3];

struct MaterialAttribute
{
	vec4 color;
	vec2 size;
	float tessHeight;
	float tessCenter;
};
layout(set = 0, binding = 0) uniform restrict readonly MaterialAttributes
{
	MaterialAttribute attributes[256];
};
layout(set = 0, binding = 1) uniform sampler2DArray nrmHeightTex;

#define P0 gl_in[0].gl_Position.xyz
#define P1 gl_in[1].gl_Position.xyz
#define P2 gl_in[2].gl_Position.xyz
#define uvw gl_TessCoord

float sampleNH(vec3 blend, uint id)
{
	MaterialAttribute att = attributes[id];
	vec2 rs = 1.0 / att.size;

	vec2 eX = texture(nrmHeightTex, vec3(modelPos.y * rs.x, modelPos.z * rs.y, id)).ba;
	vec2 eY = texture(nrmHeightTex, vec3(modelPos.x * rs.x, modelPos.z * rs.y, id)).ba;
	vec2 eZ = texture(nrmHeightTex, vec3(modelPos.x * rs.x, modelPos.y * rs.y, id)).ba;

	float x = eX.x + eX.y / 255.0;
	float y = eY.x + eY.y / 255.0;
	float z = eZ.x + eZ.y / 255.0;

	return (z * blend.z + y * blend.y + x * blend.x) * att.tessHeight - att.tessCenter;
}

void main()
{
	float u = uvw[0];
	float v = uvw[1];
	float w = uvw[2];
	float uu = u * u;
	float vv = v * v;
	float ww = w * w;
	float uu3 = uu * 3.0;
	float vv3 = vv * 3.0;
	float ww3 = ww * 3.0;

	modelNrm = normalize(inMNrm[0] * uu +
		inMNrm[1] * vv +
		inMNrm[2] * ww +
		inCP[2].xyz * u * v +
		inCP[5].xyz * v * w +
		inCP[8].xyz * w * u);

	modelPos = P0 * uu * u +
		P1 * vv * v +
		P2 * ww * w +
		inCP[0].xyz * uu3 * v + 
		inCP[1].xyz * vv3 * u +
		inCP[3].xyz * vv3 * w +
		inCP[4].xyz * ww3 * v + 
		inCP[6].xyz * ww3 * u +
		inCP[7].xyz * uu3 * w + 
		inCP[9].xyz * 6.0 * w * u * v;

	vec3 blend = vec3(
		pow(abs(modelNrm.x), 3.0),
		pow(abs(modelNrm.y), 3.0),
		pow(abs(modelNrm.z), 3.0));
	blend /= (blend.x + blend.y + blend.z);

	vec3 blends[] = vec3[3](vec3(inCP[0].w, inCP[1].w, inCP[2].w),
		vec3(inCP[3].w, inCP[4].w, inCP[5].w),
		vec3(inCP[6].w, inCP[7].w, inCP[8].w));

	vec3 h = vec3(sampleNH(blend, inID[0]),
		sampleNH(blend, inID[1]),
		sampleNH(blend, inID[2]));
	float height = h.x * u + h.y * v + h.z * w;

	h = h * uvw + uvw;
	uint cID = h.x > h.y ? h.x > h.z ? 0 : 2 : h.y > h.z ? 1 : 2;

	matID[0] = inID[0];
	matID[1] = inID[1];
	matID[2] = inID[2];
	matBlend = blends[cID];
	worldPos = modelPos + modelNrm * height;
	gl_Position = mvp * vec4(worldPos, 1.0);
	worldPos = (model * vec4(worldPos, 1.0)).xyz;
	worldNrm = (model * vec4(modelNrm, 0.0)).xyz;
}