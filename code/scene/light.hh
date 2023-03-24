#pragma once
#include "scene/gameobject.hh"
#include "scene/camera.hh"

struct DirectionalLight : Camera {
	static constexpr GameObjectType TypeTag = {"DirectionalLight"};
	const GameObjectType& Type() const override { return TypeTag; }
	const size_t Size() const override { return sizeof(*this); }

	DirectionalLight(): Camera{} {
		projection = ORTHOGRAPHIC;
		// No idea why these values work
		input.znear = 100.0f;
		input.zfar = -1000.0f;
		input.zoom = 500.0f;
	}

	uint32_t shadowmap_size = 4096;
	float shadow_bias_min = 0.0002f;
	float shadow_bias_max = 0.01f;
	uint8_t shadow_pcf_taps_x = 2;
	uint8_t shadow_pcf_taps_y = 2;

	vec3 color;
};

struct PointLight : GameObject {
	static constexpr GameObjectType TypeTag = {"PointLight"};
	const GameObjectType& Type() const override { return TypeTag; }
	const size_t Size() const override { return sizeof(*this); }

	vec3 color;
};

struct AmbientCube : GameObject {
	static constexpr GameObjectType TypeTag = {"AmbientCube"};
	const GameObjectType& Type() const override { return TypeTag; }
	const size_t Size() const override { return sizeof(*this); }

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
