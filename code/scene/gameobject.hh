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
	constexpr const char* type_name() const {
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
	GameObject* first_child = nullptr;
	GameObject* next_sibling = nullptr;

	GameObject* last_child() {
		GameObject* last = this->first_child;
		while (last->next_sibling != nullptr) {
			last = last->next_sibling;
		}
		return last;
	}

	struct ChildIterator {
		GameObject* parent;
		GameObject* current;
		GameObject* next;
		constexpr ChildIterator(GameObject* parent):
			parent{parent},
			current{parent ? parent->first_child : nullptr},
			next{current ? current->next_sibling : nullptr} {}
		ChildIterator& begin() {
			current = parent ? parent->first_child : nullptr;
			next = current ? current->next_sibling : nullptr;
			return *this;
		}
		const ChildIterator end() { return ChildIterator(nullptr); }
		ChildIterator& operator++() {
			current = next;
			next = current ? current->next_sibling : nullptr;
			return *this;
		}
		GameObject& operator*() { return *current; }
		bool operator!=(const ChildIterator& other) { return current != other.current; }
	};
	ChildIterator children() { return ChildIterator(this); }

	void add(GameObject* child) {
		if (ExpectFalse(child == nullptr)) { return; }
		if (this->first_child == nullptr) {
			this->first_child = child;
		} else {
			GameObject* last = last_child();
			if (last) {
				last->next_sibling = child;
			}
		}
		child->parent = this;
		child->next_sibling = nullptr;
	}

	void remove() {
		if (this->parent) {
			if (this->parent->first_child == this) {
				this->parent->first_child = this->next_sibling;
			}
			for (GameObject& obj : this->parent->children()) {
				if (obj.next_sibling == this) { obj.next_sibling = this->next_sibling; }
			}
		}
		for (GameObject& obj : children()) {
			obj.parent = this->parent;
		}
		memset(this, 0, sizeof(*this));
		delete this;
	}

	Type type;
	bool needs_update = true;

	const Transform& local() const { return m_local; }
	const mat4& world_matrix() const { return m_world_matrix; }
	const mat4& last_world_matrix() const { return m_last_world_matrix; }

	Transform& mut_local() {
		needs_update = true;
		return m_local;
	}

	void set_world_matrix(const mat4 world_matrix) {
		needs_update = false;
		m_last_world_matrix = world_matrix;
		m_world_matrix = world_matrix;
	}

	private:
	Transform m_local = {};
	mat4 m_world_matrix = mat4{1};
	mat4 m_last_world_matrix = mat4{1};
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
