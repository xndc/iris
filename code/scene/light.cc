#include "scene/light.hh"
#include "engine/engine.hh"

bool DirectionalLight::UpdateInput(Engine& engine) {
	input.inv_aspect = 1.0f;
	input.world_position = world_position;
	input.world_rotation = world_rotation;
	return !(input == last_input);
}

void DirectionalLight::LateUpdate(Engine& engine) {
	Camera::LateUpdate(engine);

	// Recompute view and VP. We don't care about the inverses.
	if (engine.cam_main) {
		vec3 center = engine.cam_main->position;
		vec3 eye = center - position;
		this_frame.view = glm::lookAt(eye, center, UPVECTOR);
		this_frame.vp = this_frame.proj * this_frame.view;
	}
}
