#include "editor/editor_camera.hh"
#include "engine/engine.hh"
#include <SDL.h>
#include <imgui.h>

void EditorCamera::Update(Engine &engine) {
	int dx, dy;
	uint32_t buttons = SDL_GetRelativeMouseState(&dx, &dy);

	if (buttons & SDL_BUTTON_RMASK) {
		if (!cursor_locked) {
			cursor_locked = true;
			SDL_SetRelativeMouseMode(SDL_TRUE);
		}
	} else {
		if (cursor_locked) {
			cursor_locked = false;
			SDL_SetRelativeMouseMode(SDL_FALSE);
		}
	}

	if (cursor_locked || engine.this_frame.n == 1) {
		// Rotation around X axis is pitch/vertical, should be clamped to 90 degrees
		const vec2 xclamp = vec2(ToRadians(-90.0f), ToRadians(90.0f));
		camera_rotation.x = Clamp(camera_rotation.x + float(dy) * look_speed_vert, xclamp.x, xclamp.y);

		// Rotation around Y axis is yaw/horizontal, should be unrestricted
		camera_rotation.y += float(dx) * look_speed_horz;
		if (camera_rotation.y < -(2 * M_PI)) camera_rotation.y += 2 * M_PI;
		if (camera_rotation.y > +(2 * M_PI)) camera_rotation.y -= 2 * M_PI;

		mat4 rot_x = glm::rotate(camera_rotation.x, vec3(1, 0, 0));
		mat4 rot_y = glm::rotate(camera_rotation.y, vec3(0, 1, 0));
		rotation = quat_cast(rot_x * rot_y);
	}

	int numkeys;
	const uint8_t* keys = SDL_GetKeyboardState(&numkeys);

	vec3 dpos = vec3(0);
	if (keys[SDL_SCANCODE_W]) { dpos.z -= 1; }
	if (keys[SDL_SCANCODE_S]) { dpos.z += 1; }
	if (keys[SDL_SCANCODE_A]) { dpos.x -= 1; }
	if (keys[SDL_SCANCODE_D]) { dpos.x += 1; }
	if (keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_LSHIFT]) { dpos.y -= 1; }
	if (keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_SPACE])  { dpos.y += 1; }

	if (dpos != vec3(0)) {
		dpos *= move_speed * engine.this_frame.dt;
		if (keys[SDL_SCANCODE_LALT]) { dpos *= 10.0f; }

		// Move camera towards view direction on XZ plane, but not on Y; this is nicer to control
		position = glm::rotate(position, camera_rotation.y, vec3(0, 1, 0));
		position += vec3(dpos.x, 0.0f, dpos.z);
		position = glm::rotate(position, -camera_rotation.y, vec3(0, 1, 0));
		position.y += dpos.y;
	}
}
