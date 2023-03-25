#version 300 es
precision highp float;

in vec2 VTexcoord0;
in mat3 TangentBasisNormal;

uniform vec2 FramebufferSize;
uniform sampler2D TexAlbedo;
uniform sampler2D TexNormal;
uniform sampler2D TexOccRghMet;
uniform vec4 ConstAlbedo;
uniform float ConstMetallic;
uniform float ConstRoughness;

layout(location = 0) out vec3 OutAlbedo;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec3 OutMaterial;
layout(location = 3) out vec2 OutVelocity;

// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl#92059
vec4 SRGBToLinear (vec4 srgb) {
	bvec4 cutoff = lessThan(srgb, vec4(0.04045));
	vec4 higher = pow((srgb + vec4(0.055)) / vec4(1.055), vec4(2.4));
	vec4 lower = srgb / vec4(12.92);
	return mix(higher, lower, cutoff);
}

void main() {
	// NOTE: GLSL spec says all albedo values are stored as sRGB
	vec3 albedo = ConstAlbedo.rgb * SRGBToLinear(texture(TexAlbedo, VTexcoord0)).rgb;
	OutAlbedo = albedo;

	vec3 tex_normal = texture(TexNormal, VTexcoord0).rgb;
	OutNormal = normalize(TangentBasisNormal * normalize(tex_normal * 2.0 - 1.0));

	float roughness = ConstRoughness * texture(TexOccRghMet, VTexcoord0).g;
	float metalness = ConstMetallic  * texture(TexOccRghMet, VTexcoord0).b;
	OutMaterial = vec3(0.0, roughness, metalness);
}
