#version 300 es
precision highp float;

in vec2 FragCoord01;

uniform sampler2D RTAlbedo;
uniform sampler2D RTNormal;
uniform sampler2D RTMaterial;
uniform sampler2D RTVelocity;
uniform sampler2D ShadowMap;
uniform sampler2D RTDepth;
uniform mat4 ClipToView;

layout(location = 0) out vec3 Out;

#ifdef DEPTH_ZERO_TO_ONE
	#define DEPTH_ADJUST(z) z
#else // DEPTH_HALF_TO_ONE
	#define DEPTH_ADJUST(z) (2.0 * z - 1.0)
#endif

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
	float depth = DEPTH_ADJUST(texelFetch(RTDepth, fragcoord, 0).r);

	#if defined(DEBUG_VIS_GBUF_COLOR)
		Out = texelFetch(RTAlbedo, fragcoord, 0).rgb;

	#elif defined(DEBUG_VIS_GBUF_NORMAL)
		Out = OctahedronNormalDecode(texelFetch(RTNormal, fragcoord, 0).rg);

	#elif defined(DEBUG_VIS_GBUF_MATERIAL)
		Out = texelFetch(RTMaterial, fragcoord, 0).rgb;

	#elif defined(DEBUG_VIS_GBUF_VELOCITY)
		vec2 velocity = texelFetch(RTVelocity, fragcoord, 0).rg;
		Out = vec3(vec2(0.5) + velocity * vec2(10.0), 0.0);

	#elif defined(DEBUG_VIS_DEPTH_LINEAR)
		vec4 fragpos_clip = vec4(FragCoord01 * vec2(2.0) - vec2(1.0), depth, 1.0);
		vec4 fragpos_view = ClipToView * fragpos_clip;
		Out = vec3(fragpos_view.w);

	#elif defined(DEBUG_VIS_DEPTH_RAW)
		// Encode raw depth into RGBA channels:
		// https://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
		vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * depth;
		enc = fract(enc);
		enc -= enc.yzww * vec4(1.0/255.0, 1.0/255.0, 1.0/255.0, 0.0);
		Out = enc.xyz;
	#endif
}
