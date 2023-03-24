#include "graphics/renderlist.hh"
#include "engine/engine.hh"
#include "scene/gameobject.hh"
#include "scene/camera.hh"
#include "scene/light.hh"
#include "assets/mesh.hh"

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
				RenderableMeshKey key = {
					.gl_vertex_array = mi.mesh->gl_vertex_array,
					.gl_primitive_type = mi.mesh->ptype.gl_enum(),
				};
				RenderableMesh& rmesh = meshes[key];
				if (rmesh.gl_vertex_array == 0) {
					rmesh.gl_vertex_array = key.gl_vertex_array;
					rmesh.gl_primitive_type = key.gl_primitive_type;
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
			RenderableMeshKey key = {
				.gl_vertex_array = mi.mesh->gl_vertex_array,
				.gl_primitive_type = mi.mesh->ptype.gl_enum(),
			};
			RenderableMesh& rmesh = meshes[key];
			if (rmesh.first_instance == 0) {
				// Allocate a region inside mesh_instances for this mesh's instances
				rmesh.first_instance = next_mesh_instance_slot;
				next_mesh_instance_slot += rmesh.instance_count;
				// Reuse instance_count to keep track of next index inside allocated region
				rmesh.instance_count = 0;
			}
			mesh_instances[rmesh.first_instance + (rmesh.instance_count++)] = RenderableMeshInstanceData{
				.world_transform = obj.world_transform,
			};
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
