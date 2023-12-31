#version 300 es
precision highp float;

in vec2 FragCoord01;

uniform float Time;
uniform vec2 FramebufferSize;
uniform sampler2D RTAlbedo;
uniform sampler2D RTNormal;
uniform sampler2D RTMaterial;
uniform sampler2D RTVelocity;
uniform sampler2D ShadowMap;
uniform sampler2D RTDepth;
uniform vec3 CameraPosition;
uniform vec3 LightPosition;
uniform vec3 LightColor;
uniform float ShadowBiasMin;
uniform float ShadowBiasMax;
uniform int ShadowPCFTapsX;
uniform int ShadowPCFTapsY;

uniform mat4 LocalToWorld;
uniform mat4 LocalToClip;
uniform mat4 LastLocalToClip;
uniform mat4 ClipToWorld;
uniform mat4 ShadowWorldToClip;

layout(location = 0) out vec4 OutColorHDR;

#define PI 3.14159265358979323846

#ifdef DEPTH_ZERO_TO_ONE
	#define DEPTH_ADJUST(z) z
#else // DEPTH_HALF_TO_ONE
	#define DEPTH_ADJUST(z) (2.0 * z - 1.0)
#endif

// Standard GLSL white noise function. Source usually cited as "On generating random numbers, with
// help of y= [(a+x)sin(bx)] mod 1", W.J.J. Rey (1998). The paper itself seems to be lost to time.
// Modified for better stability on some mobile GPUs/drivers by Andy Gryc.
// http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
highp float WhiteNoise(vec2 p) {
	highp vec2 ab = vec2(12.9898, 78.233);
	highp float c = 43758.5453;
	highp float dt = dot(p, ab);
	highp float sn = mod(dt, PI);
	return fract(sin(sn) * c);
}

// Base specular reflectance at normal incidence (Fresnel F0 term) for non-metallic materials.
// The constant 0.04 is used by both UE4 and the glTF 2.0 PBR model.
const vec3 UE_GLTF_Dielectric_F0 = vec3(0.04);

// Specular reflectance at normal incidence (Fresnel F0 term) as defined by the glTF 2.0 PBR model.
// UE4 uses a lookup table computed from some horrific integral equation.
vec3 GLTF_F0(vec3 albedo, float metalness) {
	return mix(UE_GLTF_Dielectric_F0, albedo, metalness);
}

// Lambertian diffuse used by UE4 (Cdiff/PI) with Cdiff remapping as per section B.3.5 of the glTF
// 2.0 spec. Indistinguishable from the LearnOpenGL diffuse factor (which depends on specular F).
// https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
vec3 UE_GLTF_Lambert_Diffuse(vec3 albedo, float metalness) {
	// FIXME: VX used this, but I think it was a misunderstanding of the glTF spec and/or Karis2013.
	// vec3 Cdiff = mix(albedo * (1.0 - UE_GLTF_Dielectric_F0), vec3(0), vec3(metalness));
	vec3 Cdiff = mix(albedo, vec3(0), vec3(metalness));
	return Cdiff/PI;
}

// UE4's modified version of Schlick's fast approximation of the Fresnel factor.
// https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
// https://doi.org/10.1111/1467-8659.1330233 (C. Schlick, 1994) section 4.2
vec3 UE_GLTF_Schlick_Fresnel(float HdotV, vec3 albedo, float metalness) {
	vec3 F0 = GLTF_F0(albedo, metalness);
	return F0 + (1.0 - F0) * pow(2.0, (-5.55473 * HdotV - 6.98316) * HdotV);
}

// Trowbridge-Reitz normal distribution function with Disney's roughness remapping (Burley, 2012).
// This function is used by both UE4 and the glTF 2.0 PBR model.
// https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf
float Disney_TrowbridgeReitz_NDF(float NdotH, float roughness) {
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float NdotH2 = NdotH * NdotH;
	float x = (NdotH2 * (alpha2 - 1.0) + 1.0);
	return alpha2 / (PI * x * x);
}

// Schlick's approximation of Bruce Smith's geometric attenuation term, modified by Disney to reduce
// extreme gain on shiny surfaces (r=(r+1)/2) and by Unreal to better fit Smith's model (k=r^2/2).
// TODO: Karis2013 says not to use this for IBL. Revisit if we ever add support for that.
// https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
float UE_Disney_Schlick_Smith_SpecG(float NdotV, float NdotL, float roughness) {
	float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
	float g1 = NdotL / ((NdotL * (1.0 - k)) + k);
	float g2 = NdotV / ((NdotV * (1.0 - k)) + k);
	return g1 * g2;
}

// Evaluates our slightly modified UE4 variant of the Cook-Torrance BRDF with the following terms:
// * Diffuse: Lambertian diffuse term used by UE4 with Cdiff remapping as per the glTF 2.0 PBR model
// * Specular D: Trowbridge-Reitz normal distribution function with Disney's roughness remapping
// * Specular F: UE4 variant of Schlick's approximation of the Fresnel factor
// * Specular G: UE4 variant of the Schlick/Smith geometric attenuation term
// Notation:
// * N is the surface normal of the fragment we're evaluating this function for (shading location)
// * V is the normalised vector from the fragment to the camera
// * L is the normalised vector from the fragment to the light
vec3 UE_CookTorrance_BRDF(vec3 N, vec3 V, vec3 L, vec3 light_color, vec3 albedo, float metalness, float roughness) {
	vec3 H = normalize(V + L);
	float NdotL = max(dot(N, L), 0.0);
	float NdotV = max(dot(N, V), 0.0);
	float HdotV = max(dot(H, V), 0.0);
	float NdotH = max(dot(N, H), 0.0);
	vec3  d  = UE_GLTF_Lambert_Diffuse(albedo, metalness);
	float sD = Disney_TrowbridgeReitz_NDF(NdotH, roughness);
	vec3  sF = UE_GLTF_Schlick_Fresnel(HdotV, albedo, metalness);
	float sG = UE_Disney_Schlick_Smith_SpecG(NdotV, NdotL, roughness);
	vec3 brdf = d + (sD * sF * sG) / max(4.0 * NdotL * NdotV, 0.001);
	return brdf * light_color * NdotL;
}

// Octahedral unit vector decoding algorithm by Rune Stubbe
// https://twitter.com/Stubbesaurus/status/937994790553227264
vec3 OctahedronNormalDecode(vec2 encoded) {
	vec2 f = encoded * 2.0 - 1.0;
	vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
	float t = clamp(-n.z, 0.0, 1.0);
	n.x += (n.x >= 0.0) ? -t : t;
	n.y += (n.y >= 0.0) ? -t : t;
	return normalize(n);
}

void main() {
	ivec2 fragcoord = ivec2(gl_FragCoord.xy);
	vec3 albedo   = texelFetch(RTAlbedo,   fragcoord, 0).rgb;
	vec2 velocity = texelFetch(RTVelocity, fragcoord, 0).rg;

	vec3 material = texelFetch(RTMaterial, fragcoord, 0).rgb;
	float occlusion = material.r;
	float roughness = max(material.g, 0.1); // zero breaks something in the lighting calculations
	float metalness = material.b;

	// Reconstruct world-space position from depth and inverse VP matrix
	float depth = DEPTH_ADJUST(texelFetch(RTDepth, fragcoord, 0).r);
	vec4 clip_pos = vec4(FragCoord01 * 2.0 - 1.0, depth, 1.0);
	vec4 unprojected_pos = ClipToWorld * clip_pos;
	vec3 world_pos = unprojected_pos.xyz / unprojected_pos.w;

	vec3 N = OctahedronNormalDecode(texelFetch(RTNormal, fragcoord, 0).rg);
	vec3 V = normalize(CameraPosition - world_pos);
	vec3 L = LightPosition; // already normalised for directional lights

	vec3 Lo = UE_CookTorrance_BRDF(N, V, L, LightColor, albedo, metalness, roughness);

	vec4 unprojected_shadow_pos = ShadowWorldToClip * unprojected_pos;
	vec3 shadow_pos = unprojected_shadow_pos.xyz / unprojected_shadow_pos.w;
	vec2 shadow_texcoord = shadow_pos.xy * 0.5 + 0.5;
	const float shadow_penumbra_size = 1.0;
	vec2 shadow_texel_size = (1.0 / vec2(textureSize(ShadowMap, 0))) * shadow_penumbra_size;
	float shadow_bias = max(ShadowBiasMax * (1.0 - dot(N, L)), ShadowBiasMin);

	float shadow = 0.0;
	if (shadow_texcoord.x >= 0.0 && shadow_texcoord.y >= 0.0 &&
		shadow_texcoord.x <= 1.0 && shadow_texcoord.y <= 1.0)
	{
		for (int ix = 0; ix < ShadowPCFTapsX; ix++) {
		for (int iy = 0; iy < ShadowPCFTapsY; iy++) {
			// PCF + random noise offset:
			vec2 offset = vec2((ix - (ShadowPCFTapsX / 2)), (iy - (ShadowPCFTapsY / 2)));
			offset *= shadow_penumbra_size;
			// FIXME: This can probably be simplified. It could also do with a toggle.
			offset.x += WhiteNoise(100.0 * world_pos.xy + mod(Time * 1.0, 100.0)) * 2.0 - 1.0;
			offset.y += WhiteNoise(100.0 * world_pos.xy + mod(Time * 2.0, 100.0)) * 2.0 - 1.0;
			float shadow_z = DEPTH_ADJUST(texture(ShadowMap, shadow_texcoord + offset * shadow_texel_size).r);
			if (shadow_z > shadow_pos.z + shadow_bias) {
				shadow += 1.0;
			}
		}}
	}
	shadow /= float(ShadowPCFTapsX * ShadowPCFTapsY);

	Lo = mix(vec3(0.0), Lo, 1.0 - shadow);

	// FIXME: Ad-hoc ambient value should be replaced with something better
	Lo = max(Lo, albedo * mix(0.07, 0.11, dot(N, vec3(0.0, 1.0, 0.0))));

	OutColorHDR = vec4(Lo, 1.0);
}
