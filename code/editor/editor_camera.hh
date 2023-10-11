#pragma once
#include "base/base.hh"
#include "scene/camera.hh"

struct EditorCamera : InfPerspectiveRevZCamera {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	EditorCamera() : InfPerspectiveRevZCamera{0.1f, 130.0f} {}

	float look_speed_horz = 0.002f;
	float look_speed_vert = 0.002f;
	vec3 move_speed = vec3(0.008f, 0.004f, 0.008f);

	// We want rotation to be in Euler angles for this camera, so we can easily update it
	vec2 camera_rotation = vec2(ToRadians(20.0f), ToRadians(-45.0f));

	bool cursor_locked = false;

	virtual void Update(Engine& engine) override;
};
