#pragma once
#include "base/base.hh"
#include "base/debug.hh"
#include "base/hash.hh"
#include "base/string.hh"
#include "base/math.hh"

#include <functional>

struct Engine;

// Tag used to identify a particular subclass of GameObject.
struct GameObjectType {
	uint64_t hash;
	char name[64 - sizeof(hash)];
	constexpr GameObjectType(const char name[]): hash{Hash64(name)}, name{} {
		StringCopy(this->name, name, sizeof(this->name));
	}
	constexpr bool operator==(const GameObjectType& rhs) const { return hash == rhs.hash; }
	constexpr bool operator!=(const GameObjectType& rhs) const { return hash != rhs.hash; }
};

/* Represents an object or entity that is part of a scene graph. Each GameObject has a name, a
 * parent and an array of child GameObjects. Each subclass of GameObject has a type and a set of
 * virtual methods for responding to events.
 *
 * GameObjects should always be heap-allocated with new. They may call delete on themselves.
 *
 * In addition to parent-child relationships, GameObjects can also model links. Use AddLink() to
 * add a child GameObject as a link. This is similar to Prefabs or Blueprints in other engines.
 */
struct GameObject {
	// Unique tag for this GameObject subclass. Each subclass should have its own tag and Type().
	static constexpr GameObjectType TypeTag = {"GameObject"};
	virtual const GameObjectType& Type() const { return TypeTag; }

	// Name assigned to this GameObject. It doesn't have to be unique.
	String name;

	// Pointer to this object's direct parent, or nullptr if this object is the root of a scene.
	GameObject* parent = nullptr;

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

	// Internal tag used by Recurse() to determine if this object has been touched.
	uint8_t recurse_tag = 0;

	// If true, this GameObject has been marked for deletion.
	bool deleted : 1 = false;

	// Default constructor. Assigns an automatically generated name to the object.
	GameObject();
	GameObject(const char* name): name{String::copy(name)} {}

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

	// Add a GameObject to the subject as a link. Returns a pointer to the given object. Linked
	// GameObjects will not have their parent changed, and can be linked to from multiple objects
	// in the scene graph.
	// FIXME: Prevent calling code from creating recursive links, maybe?
	GameObject* AddLink(GameObject* object);

	// Add an already allocated GameObject to the subject. Returns a pointer to the given object.
	template<typename T> GameObject* AddLink(T& object) {
		return AddLink(static_cast<GameObject*>(object));
	}

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

	enum class RecurseMode {
		ParentBeforeChildren,
		ChildrenBeforeParent,
	};

	// Recursively calls a function for every object reachable from this one. Follows links if
	// requested, but treats linked-to objects as roots and only processes each root once.
	void Recurse(RecurseMode mode, bool follow_links, std::function<void(GameObject&)> func,
		bool internal_increment_tag = true);

	// Recursively calls Update for every object reachable from this one. Should be called once
	// from the engine's update phase, on a scene graph root. Follows links, but treats linked-to
	// objects as roots and only processes each root once.
	void RecursiveUpdate(Engine& engine);

	// Recursively updates the world-space transforms of every object reachable from this one.
	// Should be called once from the engine's update phase, on a scene graph root. Follows links,
	// but treats linked-to objects as roots and only processes each root once.
	void RecursiveUpdateTransforms();

	// Recursively calls LateUpdate for every object reachable from this one. Should be called once
	// from the engine's update phase, on a scene graph root. Follows links, but treats linked-to
	// objects as roots and only processes each root once.
	void RecursiveLateUpdate(Engine& engine);

	// Returns a debug string for this GameObject.
	virtual String DebugName() const {
		return String::format("%s <%jx> \"%s\" [%.02f %.02f %.02f]", Type().name, uintptr_t(this), name.cstr,
			position.x, position.y, position.z);
	}
};

struct Mesh;
struct Material;

struct MeshInstance : GameObject {
	static constexpr GameObjectType TypeTag = {"MeshInstance"};
	const GameObjectType& Type() const override { return TypeTag; }

	Mesh* mesh;
	Material* material;

	MeshInstance(Mesh* mesh, Material* material): GameObject{} {
		this->mesh = mesh;
		this->material = material;
	}
};
