#version 300 es
precision highp float;

in vec2 FragCoord01;

uniform sampler2D RTColorHDR;
uniform sampler2D RTVelocity;
uniform sampler2D RTPersistTAA;
uniform sampler2D RTDepth;
uniform vec2 FramebufferSize;
uniform vec2 Jitter;
uniform vec2 LastJitter;
uniform float TAAFeedbackFactor;

layout(location = 0) out vec3 OutPersistTAA;

// This is largely based on "Temporal Reprojection Anti-Aliasing in INSIDE". References:
// * Temporal Reprojection Anti-Aliasing in INSIDE (GDC 2016)
//   https://www.gdcvault.com/play/1022970/Temporal-Reprojection-Anti-Aliasing-in
// * High Quality Temporal Supersampling (SIGGRAPH 2014, Unreal Engine)
//   https://de45xmedrsdbp.cloudfront.net/Resources/files/TemporalAA_small-59732822.pdf
// * Graphics Gems from CryENGINE 3 (SIGGRAPH 2013)

#ifdef DEPTH_ZERO_TO_ONE
	#define DEPTH_ADJUST(z) z
#else // DEPTH_HALF_TO_ONE
	#define DEPTH_ADJUST(z) (2.0 * z - 1.0)
#endif

void main() {
	// Retrieve unjittered input fragment colour:
	vec2 uv_unjittered = FragCoord01 + vec2(0.5) * Jitter;
	vec3 input_color = texture(RTColorHDR, uv_unjittered).rgb;

	// Find closest-to-camera fragment in 3x3 region around input fragment:
	// This apparently results in nicer results around edges. Some materials call this "velocity dilation".
	float closest_depth = 0.0;
	ivec2 closest_pixel = ivec2(gl_FragCoord.xy);
	for (int y = -2; y <= 2; y++) {
		for (int x = -2; x <= 2; x++) {
			ivec2 pixel = ivec2(gl_FragCoord.xy) + ivec2(x, y);
			float depth = DEPTH_ADJUST(texelFetch(RTDepth, pixel, 0).r);
			if (depth > closest_depth) {
				closest_depth = depth;
				closest_pixel = pixel;
			}
		}
	}

	// Retrieve UV-space velocity of selected fragment (from gbuffer.frag):
	vec2 velocity = texelFetch(RTVelocity, closest_pixel, 0).rg;
	// Unjittering here makes everything slightly less blurry
	vec2 uv_hist = FragCoord01 - velocity + vec2(0.5) * LastJitter;

	// Reproject fragment into TAA persistence buffer and retrieve accumulated value for it:
	vec3 accumulated_color = texture(RTPersistTAA, uv_hist).rgb;

	// Reject garbage accumulation buffer values:
	// FIXME: VX rejected accumulated values if uv_hist wasn't in the (0,1) range, but surely this was wrong?
	if (closest_depth == 0.0 || isnan(accumulated_color) != bvec3(false)) {
		accumulated_color = input_color;
	}

	// Neighbourhood clipping:
	vec3 nbp1 = texture(RTColorHDR, FragCoord01 + vec2(+1, +0) / vec2(FramebufferSize)).rgb;
	vec3 nbp2 = texture(RTColorHDR, FragCoord01 + vec2(-1, +0) / vec2(FramebufferSize)).rgb;
	vec3 nbp3 = texture(RTColorHDR, FragCoord01 + vec2(+0, +1) / vec2(FramebufferSize)).rgb;
	vec3 nbp4 = texture(RTColorHDR, FragCoord01 + vec2(+0, -1) / vec2(FramebufferSize)).rgb;
	vec3 nbx1 = texture(RTColorHDR, FragCoord01 + vec2(+1, +1) / vec2(FramebufferSize)).rgb;
	vec3 nbx2 = texture(RTColorHDR, FragCoord01 + vec2(+1, -1) / vec2(FramebufferSize)).rgb;
	vec3 nbx3 = texture(RTColorHDR, FragCoord01 + vec2(-1, +1) / vec2(FramebufferSize)).rgb;
	vec3 nbx4 = texture(RTColorHDR, FragCoord01 + vec2(-1, -1) / vec2(FramebufferSize)).rgb;
	vec3 minp = min(input_color, min(nbp1, min(nbp2, min(nbp3, nbp4))));
	vec3 maxp = max(input_color, max(nbp1, max(nbp2, max(nbp3, nbp4))));
	vec3 minx = min(minp, min(nbx1, min(nbx2, min(nbx3, nbx4))));
	vec3 maxx = max(maxp, max(nbx1, max(nbx2, max(nbx3, nbx4))));
	// I don't know what Pedersen means by "blend". Average?
	vec3 aabb_min = (minp + minx) * 0.5;
	vec3 aabb_max = (maxp + maxx) * 0.5;
	// FIXME: Go back to the presentation and document what these are
	vec3 p_clip = 0.5 * (aabb_max + aabb_min);
	vec3 e_clip = 0.5 * (aabb_max - aabb_min);
	vec3 v_clip = accumulated_color - p_clip;
	vec3 v_unit = v_clip / e_clip;
	vec3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));
	if (ma_unit > 1.0) {
		accumulated_color = p_clip + v_clip / ma_unit;
	}

	// Final blend:
	OutPersistTAA = vec3(mix(input_color, accumulated_color, vec3(TAAFeedbackFactor)));
}
