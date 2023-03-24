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

struct RenderableMeshKey {
	GLuint gl_vertex_array;
	GLenum gl_primitive_type;
	constexpr bool operator==(const RenderableMeshKey& rhs) const {
		return gl_vertex_array == rhs.gl_vertex_array && gl_primitive_type == rhs.gl_primitive_type;
	}
};

struct RenderableMesh {
	GLuint gl_vertex_array;
	GLenum gl_primitive_type;
	uint32_t first_instance;
	uint32_t instance_count;
};

struct RenderableMeshInstanceData {
	mat4 world_transform;
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
