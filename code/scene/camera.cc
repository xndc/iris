#include "scene/camera.hh"
#include "engine/engine.hh"
#include "base/math.hh"

// Convert horizontal FOV to vertical FOV angles, working in radians.
// Needs an inverse (height/width) aspect ratio to be provided, e.g. (3.0f/4.0f) for a 4:3 ratio.
static float HorizontalToVerticalFOV(float hfov_rad, float inv_aspect) {
	return atanf(tanf(hfov_rad / 2.0f) * inv_aspect);
}

// Generate orthographic projection and inverse-projection matrices.
// Needs an inverse (height/width) aspect ratio to be provided, e.g. (3.0f/4.0f) for a 4:3 ratio.
static void ProjMatrixOrthographic(float zoom, float znear, float zfar, float inv_aspect,
	mat4* out_projection, mat4* out_inv_projection)
{
	float zh = zoom * inv_aspect;
	*out_projection = glm::ortho(-zoom, zoom, -zh, zh, znear, zfar);
	// FIXME: Inverse should be computed from the original parameters, not like this.
	*out_inv_projection = glm::inverse(*out_projection);
}

// Generate perspective reverse-Z (1 to 0) projection and inverse-projection matrices.
// Needs an inverse (height/width) aspect ratio to be provided, e.g. (3.0f/4.0f) for a 4:3 ratio.
static void ProjMatrixPerspectiveRev(float vfov_rad, float znear, float zfar, float inv_aspect,
	mat4* out_projection, mat4* out_inv_projection)
{
	float f = 1.0f / tanf(vfov_rad / 2.0f);
	float a = zfar / (znear - zfar);
	float b = znear * a;
	float hw = inv_aspect;
	// FIXME: Get rid of the transpose here
	*out_projection = glm::transpose(glm::mat4{
		f * hw, 0.0f,   0.0f,    0.0f,
		0.0f,   f,      0.0f,    0.0f,
		0.0f,   0.0f,   -a-1.0f, -b,
		0.0f,   0.0f,   -1.0f,   0.0f
	});
	// FIXME: Inverse should be computed from the original parameters, not like this.
	*out_inv_projection = glm::inverse(*out_projection);
}

// Generate infinite-perspective reverse-Z (1 to 0) projection and inverse-projection matrices.
// Needs an inverse (height/width) aspect ratio to be provided, e.g. (3.0f/4.0f) for a 4:3 ratio.
static void ProjMatrixInfPerspectiveRev(float vfov_rad, float znear, float inv_aspect,
	mat4* out_projection, mat4* out_inv_projection)
{
	float f = 1.0f / tanf(vfov_rad / 2.0f);
	float n = znear;
	float hw = inv_aspect;
	// FIXME: Get rid of the transpose here
	*out_projection = glm::transpose(glm::mat4{
		f * hw, 0.0f,   0.0f,   0.0f,
		0.0f,   f,      0.0f,   0.0f,
		0.0f,   0.0f,   0.0f,   n,
		0.0f,   0.0f,   -1.0f,  0.0f
	});
	// FIXME: Inverse should be computed from the original parameters, not like this.
	*out_inv_projection = glm::inverse(*out_projection);
}

void Camera::update(const Engine& engine) {
	last_frame = this_frame;
	this_frame.inv_aspect = float(engine.display_h) / float(engine.display_w);

	if (projection != ORTHOGRAPHIC) {
		this_frame.hfov_rad = ToRadians(input.hfov_deg);
		this_frame.vfov_rad = HorizontalToVerticalFOV(this_frame.hfov_rad, this_frame.inv_aspect);
	}

	switch (projection) {
		case ORTHOGRAPHIC: {
			ProjMatrixOrthographic(input.zoom, input.znear, input.zfar, this_frame.inv_aspect,
				&this_frame.proj, &this_frame.inv_proj);
		} break;
		case PERSPECTIVE_REVZ: {
			ProjMatrixPerspectiveRev(this_frame.vfov_rad, input.znear, input.zfar, this_frame.inv_aspect,
				&this_frame.proj, &this_frame.inv_proj);
		} break;
		case INFINITE_PERSPECTIVE_REVZ: {
			ProjMatrixInfPerspectiveRev(this_frame.vfov_rad, input.znear, this_frame.inv_aspect,
				&this_frame.proj, &this_frame.inv_proj);
		} break;
	}

	// FIXME: Use world-space transform (which we don't compute yet) instead of local
	this_frame.view = glm::translate(glm::mat4_cast(local().rotation), -local().position);
	this_frame.vp = this_frame.proj * this_frame.view;

	#if 0 // FIXME: Allow camera rotation to be specified in Euler angles; would look something like this:
	this_frame.view = glm::translate(
		glm::rotate(rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)) *
		glm::rotate(rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)) *
		glm::rotate(rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)),
		-position);
	#endif

	// FIXME: Inverses should be computed from transform, not like this
	this_frame.inv_view = glm::inverse(this_frame.view);
	this_frame.inv_vp = glm::inverse(this_frame.vp);
}
