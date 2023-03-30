#pragma once
#include "scene/gameobject.hh"
#include "scene/camera.hh"

struct DirectionalLight : OrthographicCamera {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	// FIXME: No idea why these values work, look into it and tune them
	// Negative near plane is probably because we use a reverse Z setup for the shadowmap
	DirectionalLight(): OrthographicCamera{-1000.0f, 500.0f, 50.0f} {}

	uint32_t shadowmap_size = 4096;
	float shadow_bias_min = 0.0002f;
	float shadow_bias_max = 0.01f;
	uint8_t shadow_pcf_taps_x = 2;
	uint8_t shadow_pcf_taps_y = 2;

	vec3 color;

	virtual bool UpdateInput(Engine& engine) override;
	virtual void LateUpdate(Engine& engine) override;
};

struct PointLight : GameObject {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	vec3 color;
};

struct AmbientCube : GameObject {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	union {
		vec3 color[6];
		struct {
			vec3 xpos;
			vec3 xneg;
			vec3 ypos;
			vec3 yneg;
			vec3 zpos;
			vec3 zneg;
		};
	};
};
