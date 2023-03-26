#pragma once
#include "base/base.hh"
#include "scene/camera.hh"

struct EditorCamera : InfPerspectiveRevZCamera {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	EditorCamera() :
		// znear=0.5f results in reasonably high depth precision even without clip-control support
		InfPerspectiveRevZCamera{0.5f, 130.0f} {}

	float look_speed_horz = 0.002f;
	float look_speed_vert = 0.002f;
	vec3 move_speed = vec3(0.008f, 0.004f, 0.008f);

	// We want rotation to be in Euler angles for this camera, so we can easily update it
	vec3 camera_rotation = vec3(ToRadians(20.0f), ToRadians(-45.0f), ToRadians(0.0f));

	bool cursor_locked = false;

	virtual void Update(Engine& engine) override;
	virtual void LateUpdate(Engine& engine) override;
};
