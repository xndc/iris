#version 300 es
precision highp float;

in vec2 FragCoord01;

uniform sampler2D RTDepth;
uniform mat4 ClipToView;

#ifdef DEPTH_ZERO_TO_ONE
	#define DEPTH_ADJUST(z) z
#else // DEPTH_HALF_TO_ONE
	#define DEPTH_ADJUST(z) (2.0 * z - 1.0)
#endif

layout(location = 0) out vec4 Out;

void main() {
	ivec2 fragcoord = ivec2(gl_FragCoord.xy);
	float depth = DEPTH_ADJUST(texelFetch(RTDepth, fragcoord, 0).r);

	#if defined(DEBUG_VIS_DEPTH_LINEAR)
		vec4 fragpos_clip = vec4(FragCoord01 * vec2(2.0) - vec2(1.0), depth, 1.0);
		vec4 fragpos_view = ClipToView * fragpos_clip;
		Out = vec4(fragpos_view.w);
	#elif defined(DEBUG_VIS_DEPTH_RAW)
		// Encode raw depth into RGBA channels:
		// https://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
		vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * depth;
		enc = fract(enc);
		enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
		Out = enc;
	#endif
}
