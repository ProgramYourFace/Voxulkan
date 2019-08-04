#version 430
layout(location = 0) out vec4 pColor;

layout(location = 0) in vec3 modelPos;
layout(location = 1) in vec3 modelNrm;
layout(location = 2) in vec3 worldPos;
layout(location = 3) in vec3 worldNrm;
layout(location = 4) in vec3 matBlend;
layout(location = 5) in flat uint matID[3];

layout(push_constant) uniform Constants
{
	mat4 mvp;
	mat4 model;
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

	vec4 x = texture(nrmHeightTex, vec3(modelPos.y * rs.x, modelPos.z * rs.y, id));
	x.yz = (x.xy * 2.0 - 1.0) * att.tessHeight * NRM_HARDNESS;
	x.x = 0.0;

	vec4 y = texture(nrmHeightTex, vec3(modelPos.x * rs.x, modelPos.z * rs.y, id));
	y.xz = (y.xy * 2.0 - 1.0) * att.tessHeight * NRM_HARDNESS;
	y.y = 0.0;

	vec4 z = texture(nrmHeightTex, vec3(modelPos.x * rs.x, modelPos.y * rs.y, id));
	z.xy = (z.xy * 2.0 - 1.0) * att.tessHeight * NRM_HARDNESS;
	z.z = 0.0;

	return normalize(z.xyz * blend.z + y.xyz * blend.y + x.xyz * blend.x + nrm);
}

vec4 sampleCS(vec3 blend, uint id)
{
	MaterialAttribute att = attributes[id];
	vec4 x = texture(colorSpec, vec3(modelPos.y / att.size.x, modelPos.z / att.size.y, id));
	vec4 y = texture(colorSpec, vec3(modelPos.x / att.size.x, modelPos.z / att.size.y, id));
	vec4 z = texture(colorSpec, vec3(modelPos.x / att.size.x, modelPos.y / att.size.y, id));
	return (z * blend.z + y * blend.y + x * blend.x);
}

void main()
{
	vec3 blend = normalize(modelNrm);
	blend = vec3(
		pow(abs(blend.x), 3.0),
		pow(abs(blend.y), 3.0),
		pow(abs(blend.z), 3.0));
	blend *= 1.0 / (blend.x + blend.y + blend.z);

	vec3 mb = matBlend / (matBlend.x + matBlend.y + matBlend.z);

	vec3 nrm = normalize(worldNrm);
	nrm = sampleNH(blend, nrm, matID[0]) * mb.x +
		sampleNH(blend, nrm, matID[1]) * mb.y +
		sampleNH(blend, nrm, matID[2]) * mb.z;
	vec4 color = sampleCS(blend, matID[0]) * mb.x +
		sampleCS(blend, matID[1]) * mb.y +
		sampleCS(blend, matID[2]) * mb.z;

	vec3 viewDir = normalize(cameraPosition - worldPos);

	float diffuse = max(dot(nrm, SUN), 0.0);
	float specular = pow(max(dot(viewDir, reflect(-SUN, nrm)), 0.0), 2.5) * 0.5;
	pColor = vec4((AMBIENT + diffuse + specular) * color.rgb, 1.0);
}