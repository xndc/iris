#include "graphics/renderlist.hh"
#include "engine/engine.hh"
#include "scene/gameobject.hh"
#include "scene/camera.hh"
#include "scene/light.hh"
#include "assets/mesh.hh"

static bool CollideAABBFrustum(vec3 aabb_center, vec3 aabb_half_extents, mat4 local_to_clip, float zn, float zf) {
	// See https://fgiesen.wordpress.com/2010/10/17/view-frustum-culling/
	// Using "method 3" for now, since we don't compute world-space frustum planes yet.
	vec3 center = aabb_center, half = aabb_half_extents;
	vec4 p[] = {
		{ center[0] + half[0], center[1] + half[1], center[2] + half[2], 1 },
		{ center[0] + half[0], center[1] + half[1], center[2] - half[2], 1 },
		{ center[0] + half[0], center[1] - half[1], center[2] + half[2], 1 },
		{ center[0] + half[0], center[1] - half[1], center[2] - half[2], 1 },
		{ center[0] - half[0], center[1] + half[1], center[2] + half[2], 1 },
		{ center[0] - half[0], center[1] + half[1], center[2] - half[2], 1 },
		{ center[0] - half[0], center[1] - half[1], center[2] + half[2], 1 },
		{ center[0] - half[0], center[1] - half[1], center[2] - half[2], 1 },
	};
	for (int i = 0; i < countof(p); i++) {
		p[i] = local_to_clip * p[i];
	}
	// Plane equations: -w <= x <= w, -w <= y <= w, zn <= w <= zf
	bool cullX = true; // true if x is not in [-w, w]
	bool cullY = true; // true if y is not in [-w, w]
	bool cullW = true; // true if w is not in [zn, zf]
	for (int i = 0; i < countof(p); i++) {
		if (-p[i][3] <= p[i][0] && p[i][0] <= p[i][3]) { cullX = false; }
		if (-p[i][3] <= p[i][1] && p[i][1] <= p[i][3]) { cullY = false; }
		if (zn <= p[i][3] && p[i][3] <= zf) { cullW = false; }
	}
	return !(cullX && cullY && cullW);
}

static bool MeshInstanceShouldBeRendered(const MeshInstance& mi, const Camera& camera, const mat4 local_to_clip) {
	// Invalid AABB means it should always be rendered
	if (mi.mesh->aabb_half_extents == vec3(0)) { return true; }
	// Frustum-cull using AABB and MVP transform
	if (CollideAABBFrustum(mi.mesh->aabb_center, mi.mesh->aabb_half_extents, local_to_clip,
		camera.input.znear, camera.input.zfar)) { return true; }
	return false;
}

void RenderListPerView::UpdateFromScene(const Engine& engine, GameObject* scene, Camera* camera) {
	// TODO: Should reuse generated renderlist objects when possible; for now, just clear them
	Clear();

	this->camera = camera;

	uint32_t num_mesh_instances = 0;

	scene->Recurse([&](GameObject& obj) {
		switch (obj.Type().hash) {
			// For mesh instances, we have a two step process:
			// 1. Figure out how much space to reserve in RenderListPerView::mesh_instances here
			// 2. Copy instance world transforms into the vector in the next pass
			// There's certainly better ways to handle meshes, but this will do for now.
			case MeshInstance::TypeTag.hash: {
				MeshInstance& mi = static_cast<MeshInstance&>(obj);
				if (!mi.mesh || mi.mesh->gl_vertex_array == 0) { return; }
				RenderableMeshKey key = { .mesh = mi.mesh, .material = mi.material };
				RenderableMesh& rmesh = meshes[key];
				if (rmesh.mesh == nullptr) {
					rmesh.mesh = key.mesh;
					rmesh.material = key.material;
					rmesh.first_instance = 0;
					rmesh.instance_count = 0;
				}
				rmesh.instance_count++;
				num_mesh_instances++;
			} break;

			default:;
		}
	});

	mesh_instances.resize(num_mesh_instances);
	uint32_t next_mesh_instance_slot = 0;

	scene->Recurse([&](GameObject& obj) {
		// Copy instance world transforms into the just-allocated slots in mesh_instances
		if (obj.Type() == MeshInstance::TypeTag) {
			MeshInstance& mi = static_cast<MeshInstance&>(obj);
			if (!mi.mesh || mi.mesh->gl_vertex_array == 0) { return; }
			RenderableMeshKey key = { .mesh = mi.mesh, .material = mi.material };
			RenderableMesh& rmesh = meshes[key];
			if (rmesh.first_instance == 0) {
				// Allocate a region inside mesh_instances for this mesh's instances
				rmesh.first_instance = next_mesh_instance_slot;
				next_mesh_instance_slot += rmesh.instance_count;
				// Reuse instance_count to keep track of next index inside allocated region
				rmesh.instance_count = 0;
			}
			// Compute local-to-clip (MVP) transform for this instance
			mat4 local_to_clip = camera->this_frame.vp * mi.world_transform;
			if (MeshInstanceShouldBeRendered(mi, *camera, local_to_clip)) {
				mesh_instances[rmesh.first_instance + (rmesh.instance_count++)] = RenderableMeshInstanceData{
					.local_to_world = mi.world_transform,
					.local_to_clip = local_to_clip,
					.last_local_to_clip = camera->last_frame.vp * mi.world_transform,
				};
			}
		}
	});
}

void RenderList::UpdateFromScene(const Engine& engine, GameObject* scene, Camera* main_camera) {
	// TODO: Should reuse generated renderlist objects when possible; for now, just clear them
	Clear();

	this->main_camera = main_camera;

	RenderListPerView& main_view = views.emplace_back();
	main_view.camera = main_camera;

	scene->Recurse([&](GameObject& obj) {
		switch (obj.Type().hash) {
			case DirectionalLight::TypeTag.hash: {
				DirectionalLight& light = static_cast<DirectionalLight&>(obj);
				RenderableDirectionalLight& r = directional_lights.emplace_back();
				r.color = light.color;
				// FIXME: We probably want this to come from the light's rotation, like every other
				// engine does, rather than its position. But this is a bit simpler to implement.
				r.position = glm::normalize(light.world_position);
				// Directional lights are shadowcasters, so we need to consider another view.
				RenderListPerView& light_view = views.emplace_back();
				light_view.camera = static_cast<Camera*>(&light);
			} break;

			case PointLight::TypeTag.hash: {
				PointLight& light = static_cast<PointLight&>(obj);
				RenderablePointLight& r = point_lights.emplace_back();
				r.color = light.color;
				r.position = light.world_position;
			} break;

			case AmbientCube::TypeTag.hash: {
				AmbientCube& cube = static_cast<AmbientCube&>(obj);
				RenderableAmbientCube& r = ambient_cubes.emplace_back();
				memcpy(&r.color, &cube.color, sizeof(r.color));
				r.position = cube.world_position;
			} break;

			default:;
		}
	});

	for (RenderListPerView& view : views) {
		view.UpdateFromScene(engine, scene, view.camera);
	}
}
