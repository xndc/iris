#pragma once
#include "scene/gameobject.hh"

struct Engine;

struct Camera : GameObject {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	enum Projection {
		ORTHOGRAPHIC,
		// Reverse-Z non-infinite perspective projection
		PERSPECTIVE_REVZ,
		// Reverse-Z infinite perspective projection
		INFINITE_PERSPECTIVE_REVZ,
	};

	struct Input {
		// Near clipping plane
		float znear;
		// Far clipping plane, should be INFINITY for INFINITE_PERSPECTIVE projections
		float zfar;
		// Zoom factor for ORTHOGRAPHIC cameras
		// FIXME: This is in clip-space units per world-space units, right?
		float zoom;
		// Horizontal FOV in degrees at aspect ratio 4:3, for user-facing FOV controls
		float hfov_deg;
	};

	struct Derived {
		// View, projection and combined VP matrices. Projection is derived from the Input data.
		// View matrices are derived from this object's transform.
		mat4 view;
		mat4 inv_view;
		mat4 proj;
		mat4 inv_proj;
		mat4 vp;
		mat4 inv_vp;
		// Horizontal and vertical FOV in radians. Only relevant for perspective projections.
		float hfov_rad;
		float vfov_rad;
		// Inverse aspect ratio (height over width). Derived from engine state.
		float inv_aspect;
	};

	Projection projection;
	Input input;
	Derived this_frame;
	Derived last_frame;

	static Camera* NewOrthographic(float znear, float zfar, float zoom) {
		Camera* camera = new Camera();
		camera->projection = ORTHOGRAPHIC;
		camera->input.znear = znear;
		camera->input.zfar = zfar;
		camera->input.zoom = zoom;
		return camera;
	}

	static Camera* NewPerspective(float znear, float zfar, float hfov_deg) {
		Camera* camera = new Camera();
		camera->projection = PERSPECTIVE_REVZ;
		camera->input.znear = znear;
		camera->input.zfar = zfar;
		camera->input.hfov_deg = hfov_deg;
		return camera;
	}

	static Camera* NewInfPerspective(float znear, float hfov_deg) {
		Camera* camera = new Camera();
		camera->projection = INFINITE_PERSPECTIVE_REVZ;
		camera->input.znear = znear;
		camera->input.zfar = INFINITY;
		camera->input.hfov_deg = hfov_deg;
		return camera;
	}

	void LateUpdate(Engine& engine) override;
};
