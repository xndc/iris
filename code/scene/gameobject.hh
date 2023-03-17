#pragma once

#include "base/base.hh"
#include "base/debug.hh"
#include "base/math.hh"

struct Mesh;
struct Material;

struct Transform {
	vec3 position = vec3(0);
	vec3 scale = vec3(1);
	quat rotation = quat(1, 0, 0, 0);
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
	constexpr const char* GetTypeName() const {
		switch (type) {
			case GAME_OBJECT:       return "GameObject";
			case SCENE_LINK:        return "SceneLink";
			case AMBIENT_CUBE:      return "AmbientCube";
			case DIRECTIONAL_LIGHT: return "DirectionalLight";
			case POINT_LIGHT:       return "PointLight";
			case MESH_INSTANCE:     return "MeshInstance";
			case TypeCount:         Unreachable();
		}
		Unreachable(); // suppresses warning on MSVC
	}

	GameObject(): type{GAME_OBJECT} {}
	GameObject(Type type): type{type} {}

	GameObject* parent = nullptr;
	GameObject* firstChild = nullptr;
	GameObject* nextSibling = nullptr;

	FORCEINLINE GameObject* GetLastChild() {
		GameObject* lastChild = this->firstChild;
		while (lastChild->nextSibling != nullptr) {
			lastChild = lastChild->nextSibling;
		}
		return lastChild;
	}

	struct ChildIterator {
		GameObject* parent;
		GameObject* current;
		GameObject* next;
		FORCEINLINE constexpr ChildIterator(GameObject* parent):
			parent{parent},
			current{parent ? parent->firstChild : nullptr},
			next{current ? current->nextSibling : nullptr} {}
		FORCEINLINE ChildIterator& begin() {
			current = parent ? parent->firstChild : nullptr;
			next = current ? current->nextSibling : nullptr;
			return *this;
		}
		FORCEINLINE const ChildIterator end() { return ChildIterator(nullptr); }
		FORCEINLINE ChildIterator& operator++() {
			current = next;
			next = current ? current->nextSibling : nullptr;
			return *this;
		}
		FORCEINLINE GameObject& operator*() { return *current; }
		FORCEINLINE bool operator!=(const ChildIterator& other) { return current != other.current; }
	};
	FORCEINLINE ChildIterator Children() { return ChildIterator(this); }

	void Add(GameObject* child) {
		if (ExpectFalse(child == nullptr)) { return; }
		if (this->firstChild == nullptr) {
			this->firstChild = child;
		} else {
			GameObject* lastChild = GetLastChild();
			if (lastChild) {
				lastChild->nextSibling = child;
			}
		}
		child->parent = this;
		child->nextSibling = nullptr;
	}

	void Delete() {
		if (this->parent) {
			if (this->parent->firstChild == this) {
				this->parent->firstChild = this->nextSibling;
			}
			for (GameObject& obj : this->parent->Children()) {
				if (obj.nextSibling == this) { obj.nextSibling = this->nextSibling; }
			}
		}
		for (GameObject& obj : Children()) {
			obj.parent = this->parent;
		}
		memset(this, 0, sizeof(*this));
		delete this;
	}

	Type type;
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
	SceneLink(): GameObject{SCENE_LINK} {}
	SceneLink(GameObject* scene): SceneLink{} { this->scene = scene; }
	GameObject* scene;
};

struct AmbientCube : GameObject {
	AmbientCube(): GameObject{AMBIENT_CUBE} {}
	AmbientCube(vec3 colors[6]): AmbientCube{} { memcpy(&this->colors, colors, sizeof(this->colors)); }
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
	DirectionalLight(): GameObject{DIRECTIONAL_LIGHT} {}
	DirectionalLight(vec3 color): DirectionalLight{} { this->color = color; }
	vec3 color;
};

struct PointLight : GameObject {
	PointLight(): GameObject{POINT_LIGHT} {}
	PointLight(vec3 color): PointLight{} { this->color = color; }
	vec3 color;
};

struct MeshInstance : GameObject {
	MeshInstance(): GameObject{MESH_INSTANCE} {}

	Mesh* mesh;
	Material* material;

	MeshInstance(Mesh* mesh, Material* material): MeshInstance{} {
		this->mesh = mesh;
		this->material = material;
	}
};
