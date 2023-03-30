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
uniform float StippleHardCutoff;
uniform float StippleSoftCutoff;

layout(location = 0) out vec3 OutAlbedo;
layout(location = 1) out vec2 OutNormal;
layout(location = 2) out vec3 OutMaterial;
layout(location = 3) out vec2 OutVelocity;

// https://gamedev.stackexchange.com/questions/92015/optimized-linear-to-srgb-glsl#92059
vec4 SRGBToLinear(vec4 srgb) {
	bvec4 cutoff = lessThan(srgb, vec4(0.04045));
	vec4 higher = pow((srgb + vec4(0.055)) / vec4(1.055), vec4(2.4));
	vec4 lower = srgb / vec4(12.92);
	return mix(higher, lower, cutoff);
}

float Dither2x2(vec2 position, float brightness) {
	int x = int(mod(position.x, 2.0));
	int y = int(mod(position.y, 2.0));
	int index = x + y * 2;
	float limit = 0.0;
	if (x < 8) {
		if (index == 0) limit = 0.25;
		if (index == 1) limit = 0.75;
		if (index == 2) limit = 1.00;
		if (index == 3) limit = 0.50;
	}
	return brightness < limit ? 0.0 : 1.0;
}

// Fast octahedral encoding method from Cigolle, Donow and Evangelakos, "A Survey of Efficient
// Representations for Independent Unit Vectors" (2014): https://jcgt.org/published/0003/02/01/
vec2 OctahedronNormalEncode(vec3 normal) {
	vec2 n = normal.xy * (1.0 / (abs(normal.x) + abs(normal.y) + abs(normal.z)));
	if (normal.z < 0.0) {
		float x = (1.0 - abs(n.y)) * (n.x >= 0.0 ? 1.0 : -1.0);
		float y = (1.0 - abs(n.x)) * (n.y >= 0.0 ? 1.0 : -1.0);
		n = vec2(x, y);
	}
	return n * 0.5 + 0.5;
}

void main() {
	// NOTE: GLSL spec says all albedo values are stored as sRGB
	vec4 albedo = ConstAlbedo * SRGBToLinear(texture(TexAlbedo, VTexcoord0));

	float alpha = albedo.a;
	if (alpha < StippleHardCutoff) {
		discard;
	}
	if (alpha < StippleSoftCutoff) {
		alpha = (alpha - StippleHardCutoff) / (StippleSoftCutoff - StippleHardCutoff);
		if (Dither2x2(gl_FragCoord.xy, alpha) < 0.5) {
			discard;
		}
	}

	OutAlbedo = albedo.rgb;

	vec3 tex_normal = texture(TexNormal, VTexcoord0).rgb;
	// If the model doesn't specify a normal map, we'll bind a 1x1 white texture to TexNormal. We
	// can and should just copy over the normal that core_transform.vert generates in this case.
	if (tex_normal == vec3(1)) {
		OutNormal = OctahedronNormalEncode(TangentBasisNormal[2]);
	} else {
		// FIXME: Is this broken on WebGL2? Retest once we have models with actual normal maps.
		vec3 normal = normalize(TangentBasisNormal * normalize(tex_normal * 2.0 - 1.0));
		OutNormal = OctahedronNormalEncode(normal);
	}

	float roughness = ConstRoughness * texture(TexOccRghMet, VTexcoord0).g;
	float metalness = ConstMetallic  * texture(TexOccRghMet, VTexcoord0).b;
	OutMaterial = vec3(0.0, roughness, metalness);
}
