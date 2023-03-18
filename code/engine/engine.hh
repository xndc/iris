#pragma once
#include "base/base.hh"
#include "scene/camera.hh"
#include "engine/metrics.hh"

enum class VSync: int8_t {
	ADAPTIVE = -1,
	DISABLED = 0,
	FULLRATE = 1,
	HALFRATE = 2,
};

struct Tonemapper {
	enum Type {
		LINEAR,
		REINHARD,
		HABLE,
		ACES,
	} type = LINEAR;
	float exposure = 1.0f;
	Tonemapper() = default;
	Tonemapper(Type type, float exposure): type{type}, exposure{exposure} {}
};

enum class DebugVisBuffer {
	FINAL,
	GBUF_COLOR,
	GBUF_NORMAL,
	GBUF_MATERIAL,
	GBUF_VELOCITY,
	WORLD_POSITION,
	DEPTH_RAW,
	DEPTH_LINEAR,
	SHADOWMAP,
};

struct FrameState {
	uint64_t n;  // frame number
	float t, dt; // time at frame start, delta-time from prev frame

	float t_poll;   // after operating system is polled for events
	float t_update; // after asset loading ops are processed and GameObjects are updated
	float t_render; // after drawcalls are submitted to GPU

	// Mouse inputs, which we need to diff across frames:
	// TODO: We should have a dedicated input system for this sort of thing.
	float mouse_x;
	float mouse_y;
	float mouse_dx;
	float mouse_dy;

	uint32_t total_drawcalls;
	uint32_t total_polys_rendered;
	uint32_t total_polys_culled;

	// If true, all timing fata for this frame will be discarded. Used to avoid breaking the
	// in-game stats display when the game is paused.
	bool ignore_for_timing = false;

	FrameState() = default;
	FrameState(const FrameState& last, float t): n{last.n + 1}, t{t}, dt{t - last.t} {}
};

// Engine configuration properties and state.
struct Engine {
	FrameState this_frame;
	FrameState last_frame;

	// High-performance timestamp retrieved when the engine starts up.
	uint64_t initial_t;
	MetricBuffer metrics_poll   = MetricBuffer(360);
	MetricBuffer metrics_update = MetricBuffer(360);
	MetricBuffer metrics_render = MetricBuffer(360);
	MetricBuffer metrics_swap   = MetricBuffer(360);

	uint32_t display_w = 1280;
	uint32_t display_h = 720;

	// znear=0.5f results in reasonably high depth precision even without clip-control support
	Camera* cam_main = Camera::inf_perspective(0.5f, 90.0f);

	Camera* cam_shadow = Camera::orthographic(100.0f, -1000.0f, 500.0f);
	uint32_t shadowmap_size = 4096;
	float shadow_bias_min = 0.0002f;
	float shadow_bias_max = 0.01f;
	uint8_t shadow_pcf_taps_x = 2;
	uint8_t shadow_pcf_taps_y = 2;

	VSync vsync = VSync::ADAPTIVE;
	Tonemapper tonemapper = Tonemapper(Tonemapper::HABLE, 16.0f);

	// Multiplier for TAA sampling jitter offsets. Original offsets are between [-1,1].
	// Higher multipliers increase both blur and visible jitter on specular surfaces.
	// Going too low results in TAA becoming useless (sampled positions are almost the same).
	float taa_sample_offset_mul = 0.2f;
	// Offset from current sample to use in neighbourhood clamping, in [0,1]. Higher values result
	// in slightly more effective TAA at the cost of extra blur.
	float taa_clamp_sample_dist = 0.5f;
	// Lerp factor for blending between the historical buffer and the current frame. Higher values
	// assign more weight to the historical buffer, resulting in better TAA at the cost of extra
	// blur and more time needed to resolve the image.
	float taa_feedback_factor = 0.85f;

	// Strength for the sharpening post-filter. Relevant range is [0, 0.1].
	// FIXME: The current implementation is quite bad, so it's best to keep this disabled.
	float sharpen_strength = 0.0f;

	DebugVisBuffer debugvis_buffer = DebugVisBuffer::FINAL;

	// Bitfield section:

	bool pause_on_focus_loss : 1 = true;
	bool clear_colour_buffers : 1 = true;

	// Supposed to fix "Peter Panning" by rendering only backfaces into the shadow map.
	// Doesn't seem to make a difference; we don't get any Peter Panning anyway.
	bool shadow_render_only_backfaces : 1 = true;
	// Add random offsets when sampling. Results in noisy shadows that we soften through TAA.
	bool shadow_noisy_sampling : 1 = true;

	// Enable the Temporal Anti-Aliasing filter. Smooths the image at the cost of some blur.
	bool taa_enabled : 1 = true;
	// If enabled, use a Halton pattern for the jitter. If disabled, use a simple 2-sample pattern.
	bool taa_halton_jitter = true;

	bool debugvis_light_gizmos : 1 = false;
	bool debugvis_light_volumes : 1 = false;
	bool debugvis_mesh_aabbs : 1 = false;
	bool debugvis_white_world : 1 = false;
};
