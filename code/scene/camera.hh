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
		// World-space object position and rotation. These are part of GameObject, but we want to
		// mirror them in Input for clarity and so we can more easily check if the input changes.
		quat world_rotation;
		vec3 world_position;
		// Inverse aspect ratio (height over width). Derived from engine state.
		float inv_aspect;
		bool operator==(const Input& rhs) const { return memcmp(this, &rhs, sizeof(*this)) == 0; }
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
	};

	Projection projection;
	Input input;
	Input last_input;
	Derived this_frame;
	Derived last_frame;

	Camera(Projection projection, float znear, float zfar, float hfov_deg_or_zoom) {
		this->projection = projection;
		this->input.znear = znear;
		switch (projection) {
			case ORTHOGRAPHIC: {
				this->input.zfar = zfar;
				this->input.zoom = hfov_deg_or_zoom;
			} break;
			case PERSPECTIVE_REVZ: {
				this->input.zfar = zfar;
				this->input.hfov_deg = hfov_deg_or_zoom;
			} break;
			case INFINITE_PERSPECTIVE_REVZ: {
				this->input.zfar = INFINITY;
				this->input.hfov_deg = hfov_deg_or_zoom;
			} break;
		}
		this->last_input = this->input;
	}

	// Updates the camera's Input struct. Should be run after updating world transforms. Doesn't
	// modify last_input. Returns true if the newly computed input has changed from last_input.
	bool UpdateInput(Engine& engine);

	virtual void LateUpdate(Engine& engine) override;
};

struct OrthographicCamera : Camera {
	OrthographicCamera(float znear, float zfar, float zoom) :
		Camera{ORTHOGRAPHIC, znear, zfar, zoom} {}
};

struct PerspectiveRevZCamera : Camera {
	PerspectiveRevZCamera(float znear, float zfar, float hfov_deg) :
		Camera{PERSPECTIVE_REVZ, znear, zfar, hfov_deg} {}
};

struct InfPerspectiveRevZCamera : Camera {
	InfPerspectiveRevZCamera(float znear, float hfov_deg) :
		Camera{INFINITE_PERSPECTIVE_REVZ, znear, INFINITY, hfov_deg} {}
};
