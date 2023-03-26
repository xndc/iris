#pragma once
#include "base/base.hh"
#include "base/debug.hh"
#include "base/hash.hh"
#include "base/string.hh"
#include "base/math.hh"

#include <functional>

struct Engine;

struct GameObjectBase {
	virtual constexpr size_t Size() const = 0;
};

/* Represents an object or entity that is part of a scene graph. Each GameObject has a name, a
 * parent and an array of child GameObjects. Each subclass of GameObject has a type and a set of
 * virtual methods for responding to events.
 *
 * GameObjects should always be heap-allocated with new. They may call delete on themselves.
 */
struct GameObject: GameObjectBase {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	// Pointer to this object's direct parent, or nullptr if this object is the root of a scene.
	GameObject* parent = nullptr;

	// Pointer to the object this one was copied from, if one exists.
	GameObject* blueprint = nullptr;

	struct ChildListNode {
		GameObject* objects[15] = {nullptr};
		struct ChildListNode* next = nullptr;
	};
	// GameObjects contain an unrolled linked list of child pointers, to avoid excessive pointer
	// chasing. Use begin() and end() to traverse the list and operator[] to index into it.
	ChildListNode child_list;

	// Local position of this object (relative to its parent).
	vec3 position = vec3(0);

	// Local scale of this object (relative to its parent).
	vec3 scale = vec3(1);

	// Local rotation/orientation of this object as a quaternion (relative to its parent).
	quat rotation = quat(1, 0, 0, 0);

	// World-space position of this object (relative to its ultimate parent). This is a read-only
	// property that is set based on the local transform in LateUpdate.
	const vec3 world_position = vec3(0);

	// World-space scale of this object (relative to its ultimate parent). This is a read-only
	// property that is set based on the local transform after Update.
	const vec3 world_scale = vec3(1);

	// World-space rotation of this object (relative to its ultimate parent). This is a read-only
	// property that is set based on the local transform after Update.
	quat world_rotation = quat(1, 0, 0, 0);

	// World-space transformation matrix of this object (relative to its ultimate parent). This is a
	// read-only property that is set based on the local transform after Update.
	mat4 world_transform = mat4(1);

	// Name assigned to this object, or nullptr if none. Use Name() to get a printable version.
	String assigned_name = nullptr;

	// Unique number assigned to this object. Set by the base constructor, shouldn't be changed.
	const uint32_t unique_id = 0;

	// If true, this GameObject has been marked for deletion.
	bool deleted : 1 = false;

	GameObject(const char* name = nullptr);

	// Returns the name assigned to this object, or an auto-generated one.
	String Name() const;
	String Name();

	// Determines if this object has any direct children.
	bool HasChildren() const;

	// Counts how many direct children this object has.
	uint32_t NumChildren() const;

	// Returns the nth child of this object. Returns nullptr if the child does not exist or has been
	// marked for deletion. Child indices are always stable.
	GameObject* operator[](size_t idx) const;

	struct ChildIterator {
		ChildListNode* node;
		uint32_t idx = 0;
		constexpr ChildIterator(ChildListNode* node): node{node} {}
		constexpr GameObject& operator*() { return *(node->objects[idx]); }
		constexpr bool operator!=(const ChildIterator& rhs) {
			if (node == nullptr && rhs.node == nullptr) { return false; }
			return node != rhs.node || idx != rhs.idx;
		}
		ChildIterator& operator++();
	};
	ChildIterator begin() { return ChildIterator(HasChildren() ? &child_list : nullptr); }
	const ChildIterator end() { return ChildIterator(nullptr); }

	// Add an already allocated GameObject to the subject. Returns a pointer to the given object.
	GameObject* Add(GameObject* object);

	// Add an already allocated GameObject to the subject. Returns a pointer to the given object.
	template<typename T> T* Add(T* object) {
		return static_cast<T*>(Add(static_cast<GameObject*>(object)));
	}

	// Allocate and construct a GameObject (or a subclass) as a child of the subject.
	template<typename T, class... Args> T* AddNew(Args&&... args) {
		T* object = new T(args...);
		return static_cast<T*>(Add(static_cast<GameObject*>(object)));
	}

	// Add a GameObject tree to the subject by copying it. Returns a pointer to the copy.
	// This needs to know how much space to allocate, so it can only be used as a template.
	GameObject* AddCopy(GameObject* blueprint);

	// Marks this GameObject for deletion. It will no longer be returned by iterators or operator[].
	// This will also mark any child GameObjects for deletion.
	void Delete();

	// Delete this GameObject if it's been previously marked for deletion. Recurse to find any child
	// GameObjects that need to be deleted.
	void GarbageCollect();

	// Deallocates any extra buffers that this GameObject owns. If you're writing a subclass of
	// GameObject that owns buffers, override and extend this destructor to free them.
	virtual ~GameObject();

	// Called during the update phase of each frame. You can modify the local transform here.
	virtual void Update(Engine& engine) {}

	// Called during the update phase of each frame, after all Update() calls have returned and the
	// object's world-space transform has been computed.
	virtual void LateUpdate(Engine& engine) {}

	// Recursively calls a function for every object reachable from this one. Calls one function
	// before recursing over this object's children, and one function after.
	void Recurse(std::function<void(GameObject&)> before, std::function<void(GameObject&)> after = nullptr);

	// Recursively calls Update for every object reachable from this one. Should be called once
	// from the engine's update phase, on a scene graph root.
	void RecursiveUpdate(Engine& engine);

	// Recursively updates the world-space transforms of every object reachable from this one.
	// Should be called once from the engine's update phase, on a scene graph root.
	void RecursiveUpdateTransforms();

	// Recursively calls LateUpdate for every object reachable from this one. Should be called once
	// from the engine's update phase, on a scene graph root.
	void RecursiveLateUpdate(Engine& engine);

	// Returns a debug string for this GameObject.
	virtual String DebugName();
};

struct Mesh;
struct Material;

struct MeshInstance : GameObject {
	// Returns the size of this object. Subclasses must include this exact definition.
	virtual constexpr size_t Size() const override { return sizeof(*this); }

	Mesh* mesh;
	Material* material;

	MeshInstance(Mesh* mesh, Material* material): GameObject{} {
		this->mesh = mesh;
		this->material = material;
	}
};
