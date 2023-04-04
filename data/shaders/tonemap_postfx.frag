#version 300 es
precision highp float;

// Final tonemap and post-FX shader that reads from RTColorHDR and writes to the backbuffer.

in vec2 FragCoord01;

uniform vec2 FramebufferSize;
uniform sampler2D RTColorHDR;
uniform float TonemapExposure;

layout(location = 0) out vec3 OutColor;

#if defined(TONEMAP_HABLE)
// John Hable's tonemapping operator, also known as the Uncharted 2 operator:
// http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 HableSub(vec3 x) {
	const float A = 0.15;
	const float B = 0.50;
	const float C = 0.10;
	const float D = 0.20;
	const float E = 0.02;
	const float F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
vec3 Hable(vec3 color) {
	vec3 exposure_bias = vec3(2.0);
	vec3 white_scale = 1.0f / HableSub(vec3(11.2));
	return HableSub(color * exposure_bias) * white_scale;
}
#endif

#if defined(TONEMAP_ACES)
// ACES approximation by Stephen Hill (@self_shadow)
// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
	0.59719, 0.35458, 0.04823,
	0.07600, 0.90834, 0.01566,
	0.02840, 0.13383, 0.83777
);
// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
	 1.60475, -0.53108, -0.07367,
	-0.10208,  1.10813, -0.00605,
	-0.00327, -0.07276,  1.07602
);
vec3 RRTAndODTFit(vec3 v) {
	vec3 a = v * (v + 0.0245786f) - 0.000090537f;
	vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
	return a / b;
}
vec3 ACESFitted(vec3 color) {
	color = color * ACESInputMat;
	color = RRTAndODTFit(color);
	color = color * ACESOutputMat;
	return clamp(color, 0.0, 1.0);
}
#endif

void main() {
	vec3 hdr = TonemapExposure * texture(RTColorHDR, FragCoord01).rgb;

	#if defined(TONEMAP_REINHARD)
		vec3 ldr = hdr / (1.0 + hdr);
	#elif defined(TONEMAP_HABLE)
		vec3 ldr = Hable(hdr);
	#elif defined(TONEMAP_ACES)
		vec3 ldr = ACESFitted(hdr);
	#else // TONEMAP_LINEAR
		vec3 ldr = hdr;
	#endif

	OutColor = ldr;
}
