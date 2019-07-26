#version 430

layout(triangles, fractional_odd_spacing, cw) in;

layout(push_constant) uniform CameraConstants
{
	mat4 viewProjection;
	vec3 cameraPosition;
	float tessFactor;
};

layout(location = 0) in vec3 eNrm[];
layout(location = 1) in uint eId[];
layout(location = 2) in vec3 cpI[];
layout(location = 3) in vec3 cpO[];
layout(location = 4) in vec3 cpN[];

layout(location = 5) in patch vec3 cpC;

layout(location = 0) out vec3 preNrm;
layout(location = 1) out vec3 prePos;
layout(location = 2) out vec3 postPos;
layout(location = 3) out vec3 matBlend;
layout(location = 4) out flat uint matID[3];

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

	vec2 eX = texture(nrmHeightTex, vec3(prePos.y * rs.x, prePos.z * rs.y, id)).ba;
	vec2 eY = texture(nrmHeightTex, vec3(prePos.x * rs.x, prePos.z * rs.y, id)).ba;
	vec2 eZ = texture(nrmHeightTex, vec3(prePos.x * rs.x, prePos.y * rs.y, id)).ba;

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

	preNrm = normalize(eNrm[0] * uu +
		eNrm[1] * vv +
		eNrm[2] * ww +
		cpN[0] * u * v +
		cpN[1] * v * w +
		cpN[2] * w * u);

	prePos = P0 * uu * u +
		P1 * vv * v +
		P2 * ww * w +
		cpI[0] * uu3 * v + 
		cpO[0] * vv3 * u +
		cpI[1] * vv3 * w +
		cpO[1] * ww3 * v + 
		cpI[2] * ww3 * u +
		cpO[2] * uu3 * w + 
		cpC * 6.0 * w * u * v;

	vec3 blend = vec3(
		pow(abs(preNrm.x), 3.0),
		pow(abs(preNrm.y), 3.0),
		pow(abs(preNrm.z), 3.0));
	blend /= (blend.x + blend.y + blend.z);

	vec3 h = vec3(sampleNH(blend, eId[0]),
		sampleNH(blend, eId[1]),
		sampleNH(blend, eId[2]));

	vec3 bh = h * uvw + uvw;
	bh.x = 1.0 / (1.0 + pow(2.0, 50.0 * (0.5 - bh.x)));
	bh.y = 1.0 / (1.0 + pow(2.0, 50.0 * (0.5 - bh.y)));
	bh.z = 1.0 / (1.0 + pow(2.0, 50.0 * (0.5 - bh.z)));
	bh /= bh.x + bh.y + bh.z;

	float height = h.x * bh.x + h.y * bh.y + h.z * bh.z;
	matBlend = bh;
	matID[0] = eId[0];
	matID[1] = eId[1];
	matID[2] = eId[2];
	gl_Position = viewProjection * vec4(prePos + preNrm * height, 1.0);
}