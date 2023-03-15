#pragma once

#include "base/math.hh"

struct Scene;
struct Mesh;

struct Transform {
	vec3 position;
	vec3 scale;
	quat rotation;
};


struct GameObject {
	enum Type {
		GAME_OBJECT,
		SCENE_LINK,
		AMBIENT_CUBE,
		DIRECTIONAL_LIGHT,
		POINT_LIGHT,
		MESH_INSTANCE,
		TypeCount,
	};
	static const Type type = GAME_OBJECT;

	GameObject* parent = nullptr;
	bool needsTransformUpdate = true;

	FORCEINLINE const Transform& GetLocal() const { return this->local; }
	FORCEINLINE const mat4& GetWorldMatrix() const { return this->worldMatrix; }
	FORCEINLINE const mat4& GetLastWorldMatrix() const { return this->lastWorldMatrix; }

	FORCEINLINE Transform& MutLocal() {
		needsTransformUpdate = true;
		return this->local;
	}

	FORCEINLINE void SetWorldMatrix(const mat4 newWorldMatrix) {
		needsTransformUpdate = false;
		this->lastWorldMatrix = this->worldMatrix;
		this->worldMatrix = newWorldMatrix;
	}

	private:
	Transform local = {};
	mat4 worldMatrix = mat4{1};
	mat4 lastWorldMatrix = mat4{1};
};

struct SceneLink : GameObject {
	static const Type type = SCENE_LINK;
	Scene* scene;
};

struct AmbientCube : GameObject {
	static const Type type = AMBIENT_CUBE;
	union {
		vec3 colors[6];
		struct {
			vec3 xpos, xneg;
			vec3 ypos, yneg;
			vec3 zpos, zneg;
		};
	};
};

struct DirectionalLight : GameObject {
	static const Type type = DIRECTIONAL_LIGHT;
	vec3 color;
};

struct PointLight : GameObject {
	static const Type type = POINT_LIGHT;
	vec3 color;
};

struct MeshInstance : GameObject {
	static const Type type = MESH_INSTANCE;
	Mesh* mesh;
};
