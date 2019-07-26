#version 430
layout(location = 0) out vec4 pColor;

layout(location = 0) in vec3 preNrm;
layout(location = 1) in vec3 prePos;
layout(location = 2) in vec3 postPos;
layout(location = 3) in vec3 matBlend;
layout(location = 4) in flat uint matID[3];

layout(push_constant) uniform CameraConstants
{
	mat4 viewProjection;
	vec3 cameraPosition;
	float tessFactor;
};

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
layout(set = 0, binding = 2) uniform sampler2DArray colorSpec;

const float NRM_HARDNESS = 1.5;
const vec3 AMBIENT = vec3(0.14, 0.15, 0.25);
const vec3 SUN = normalize(vec3(1,1,1));

vec3 sampleNH(vec3 blend, vec3 nrm, uint id)
{
	MaterialAttribute att = attributes[id];
	vec2 rs = 1.0 / att.size;

	vec4 x = texture(nrmHeightTex, vec3(prePos.y * rs.x, prePos.z * rs.y, id));
	x.yz = (x.xy * 2.0 - 1.0) * att.tessHeight * NRM_HARDNESS;
	x.x = 0.0;

	vec4 y = texture(nrmHeightTex, vec3(prePos.x * rs.x, prePos.z * rs.y, id));
	y.xz = (y.xy * 2.0 - 1.0) * att.tessHeight * NRM_HARDNESS;
	y.y = 0.0;

	vec4 z = texture(nrmHeightTex, vec3(prePos.x * rs.x, prePos.y * rs.y, id));
	z.xy = (z.xy * 2.0 - 1.0) * att.tessHeight * NRM_HARDNESS;
	z.z = 0.0;

	return normalize(z.xyz * blend.z + y.xyz * blend.y + x.xyz * blend.x + nrm);
}

vec4 sampleCS(vec3 blend, uint id)
{
	MaterialAttribute att = attributes[id];
	vec4 x = texture(colorSpec, vec3(prePos.y / att.size.x, prePos.z / att.size.y, id));
	vec4 y = texture(colorSpec, vec3(prePos.x / att.size.x, prePos.z / att.size.y, id));
	vec4 z = texture(colorSpec, vec3(prePos.x / att.size.x, prePos.y / att.size.y, id));
	return (z * blend.z + y * blend.y + x * blend.x);
}

void main()
{
	vec3 nrm = normalize(preNrm);

	vec3 blend = vec3(
		pow(abs(nrm.x), 3.0),
		pow(abs(nrm.y), 3.0),
		pow(abs(nrm.z), 3.0));
	blend *= 1.0 / (blend.x + blend.y + blend.z);

	nrm = sampleNH(blend, nrm, matID[0]) * matBlend.x +
		sampleNH(blend, nrm, matID[1]) * matBlend.y +
		sampleNH(blend, nrm, matID[2]) * matBlend.z;

	vec4 color = sampleCS(blend, matID[0]) * matBlend.x +
		sampleCS(blend, matID[1]) * matBlend.y +
		sampleCS(blend, matID[2]) * matBlend.z;

	vec3 viewDir = normalize(cameraPosition - postPos);

	float diffuse = max(dot(nrm, SUN), 0.0);
	float specular = pow(max(dot(viewDir, reflect(-SUN, nrm)), 0.0), 2.5) * 0.5;
	pColor = vec4((AMBIENT + diffuse + specular) * color.rgb, 1.0);
}