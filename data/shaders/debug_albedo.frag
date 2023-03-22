#version 330 core

in vec2 VTexcoord0;

uniform vec2 FramebufferSize;
uniform sampler2D TexAlbedo;

layout(location = 0) out vec4 OutColor;

// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl#92059
vec4 srgbToLinear (vec4 sRGB) {
	bvec4 cutoff = lessThan(sRGB, vec4(0.04045));
	vec4 higher = pow((sRGB + vec4(0.055))/vec4(1.055), vec4(2.4));
	vec4 lower = sRGB/vec4(12.92);
	return mix(higher, lower, cutoff);
}

void main() {
	// NOTE: GLSL spec says all diffuse colour values are stored as sRGB
	vec4 diffuse = srgbToLinear(texture(TexAlbedo, VTexcoord0));
	OutColor = vec4(diffuse.rgb, 1.0);
}
