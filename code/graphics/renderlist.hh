#pragma once
#include "base/base.hh"
#include "base/math.hh"
#include "base/hash.hh"
#include "graphics/opengl.hh"

#include <vector>
#include <unordered_map>

struct Engine;
struct GameObject;
struct Camera;
struct Mesh;
struct Material;

struct RenderableMeshKey {
	Mesh* mesh;
	Material* material;
	constexpr bool operator==(const RenderableMeshKey& rhs) const {
		return mesh == rhs.mesh && material == rhs.material;
	}
};

struct RenderableMesh {
	Mesh* mesh;
	Material* material;
	uint32_t first_instance;
	uint32_t instance_count;
};

struct RenderableMeshInstanceData {
	// Local-to-world (model) matrix. Needed for tangent/basis/normal computations.
	mat4 local_to_world;
	// Local-to-clip (MVP) transform. This is view-dependent, so we have one object per view.
	// NOTE: It would be nice not to have to precompute this, but we do frustum culling on the CPU
	// side at the moment. Revisit if we ever implement hierarchical Z-buffer occlusion.
	mat4 local_to_clip;
	mat4 last_local_to_clip;
};

struct RenderableDirectionalLight {
	vec3 position; // normalised
	vec3 color;
};

struct RenderablePointLight {
	vec3 position;
	vec3 color;
};

struct RenderableAmbientCube {
	vec3 position;
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

struct RenderListPerView {
	Camera* camera;
	std::unordered_map<RenderableMeshKey, RenderableMesh, Hash64T> meshes;
	std::vector<RenderableMeshInstanceData> mesh_instances;

	RenderListPerView() {
		meshes.reserve(1024);
	}

	void Clear() {
		meshes.clear();
		mesh_instances.clear();
	}

	void UpdateFromScene(const Engine& engine, GameObject* scene, Camera* camera);
};

struct RenderList {
	Camera* main_camera;
	std::vector<RenderListPerView> views;
	std::vector<RenderableDirectionalLight> directional_lights;
	std::vector<RenderablePointLight> point_lights;
	std::vector<RenderableAmbientCube> ambient_cubes;

	RenderList() {
		views.reserve(2);
		directional_lights.reserve(1);
		point_lights.reserve(128);
		ambient_cubes.reserve(1);
	}

	void Clear() {
		main_camera = nullptr;
		views.clear();
		directional_lights.clear();
		point_lights.clear();
		ambient_cubes.clear();
	}

	void UpdateFromScene(const Engine& engine, GameObject* scene, Camera* main_camera);
};
