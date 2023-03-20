#include "scene/gameobject.hh"
#include "engine/engine.hh"

#include <vector>

static uint32_t GameObject_NextUniqueID = 1;

GameObject::GameObject(const char* name) : assigned_name{name ? String::copy(name) : nullptr} {
	const_cast<uint32_t&>(unique_id) = GameObject_NextUniqueID++;
}

String GameObject::Name() const {
	if (assigned_name) { return String::view(assigned_name); }
	return String::format("%s#%u", Type().name, unique_id);
}

String GameObject::Name() {
	if (!assigned_name) { assigned_name = const_cast<const GameObject*>(this)->Name(); }
	return String::view(assigned_name);
}

bool GameObject::HasChildren() const {
	const ChildListNode* node = &child_list;
	while (node != nullptr) {
		for (size_t slot = 0; slot < CountOf(node->objects); slot++) {
			if (node->objects[slot] != nullptr) {
				return true;
			}
		}
		node = node->next;
	}
	return false;
}

uint32_t GameObject::NumChildren() const {
	uint32_t count = 0;
	const ChildListNode* node = &child_list;
	while (node != nullptr) {
		for (size_t slot = 0; slot < CountOf(node->objects); slot++) {
			if (node->objects[slot] != nullptr) {
				count++;
			}
		}
		node = node->next;
	}
	return count;
}

GameObject* GameObject::operator[](size_t idx) const {
	const ChildListNode* node = &child_list;
	while (node != nullptr && idx >= CountOf(node->objects)) {
		idx -= CountOf(node->objects);
		node = node->next;
	}
	if (node == nullptr || node->objects[idx]->deleted) {
		return nullptr;
	}
	return node->objects[idx];
}

GameObject::ChildIterator& GameObject::ChildIterator::operator++() {
	while (node != nullptr) {
		idx++;
		if (idx >= CountOf(node->objects)) {
			node = node->next;
			idx = 0;
		} else if (node->objects[idx] != nullptr && !(node->objects[idx]->deleted)) {
			break;
		}
	}
	return *this;
}

GameObject* GameObject::Add(GameObject* object) {
	if (ExpectFalse(object == nullptr)) { return nullptr; }
	object->parent = this;
	// We'll want to find the first free slot in any node, since slots can be freed up by deletions
	ChildListNode* node = &child_list;
	while (node != nullptr) {
		for (size_t slot = 0; slot < CountOf(node->objects); slot++) {
			if (node->objects[slot] == nullptr) {
				node->objects[slot] = object;
				return object;
			}
		}
		if (node->next == nullptr) {
			node->next = new ChildListNode();
		}
		node = node->next;
	}
	Panic("OOM in GameObject::Add: failed to allocate ChildListNode");
}

GameObject* GameObject::AddCopy(GameObject* blueprint) {
	void* src = reinterpret_cast<void*>(blueprint);
	void* dst = malloc(blueprint->Size());
	memcpy(dst, src, blueprint->Size());

	GameObject* copy = reinterpret_cast<GameObject*>(dst);
	copy->parent = this;
	copy->blueprint = blueprint;
	copy->assigned_name = String::copy(blueprint->assigned_name);
	const_cast<uint32_t&>(copy->unique_id) = GameObject_NextUniqueID++;

	memset(&copy->child_list, 0, sizeof(copy->child_list));
	for (GameObject& child : *blueprint) {
		copy->AddCopy(&child);
	}

	return this->Add(copy);
}

void GameObject::Delete() {
	for (GameObject& child : *this) {
		if (child.parent == this) {
			child.Delete();
		}
	}
	deleted = true;
}

void GameObject::GarbageCollect() {
	for (GameObject& child : *this) {
		if (child.parent == this) {
			child.GarbageCollect();
		}
	}
	if (deleted) {
		delete this;
	} else {
		// Clean up empty nodes
		ChildListNode* node = &child_list;
		ChildListNode* next = node->next;
		while (next != nullptr) {
			bool next_is_empty = true;
			for (size_t slot = 0; slot < CountOf(next->objects); slot++) {
				if (next->objects[slot] != nullptr) {
					next_is_empty = false;
					break;
				}
			}
			if (next_is_empty) {
				node->next = next->next;
				delete next;
				next = node->next;
			} else {
				node = next;
				next = next->next;
			}
		}
	}
}

GameObject::~GameObject() {
	// Delete any ChildListNodes allocated by Add()
	ChildListNode* node = child_list.next;
	while (node != nullptr) {
		ChildListNode* next = node->next;
		delete node;
		node = next;
	}
}

void GameObject::Recurse(std::function<void(GameObject&)> before, std::function<void(GameObject&)> after)
{
	if (before) { before(*this); }
	for (GameObject& child : *this) {
		child.Recurse(before, after);
	}
	if (after) { after(*this); }
}

void GameObject::RecursiveUpdate(Engine& engine) {
	Recurse([&](GameObject& obj) { obj.Update(engine); });
}

void GameObject::RecursiveUpdateTransforms() {
	Recurse([&](GameObject& obj) {
		if (obj.parent == nullptr) {
			const_cast<vec3&>(obj.world_position) = obj.position;
			const_cast<vec3&>(obj.world_scale) = obj.scale;
			const_cast<quat&>(obj.world_rotation) = obj.rotation;
			mat4 xform = glm::mat4_cast(obj.rotation) * glm::scale(glm::translate(obj.position), obj.scale);
			const_cast<mat4&>(obj.world_transform) = xform;
		} else {
			mat4 xform = glm::mat4_cast(obj.rotation) * glm::scale(glm::translate(obj.position), obj.scale);
			const_cast<mat4&>(obj.world_transform) = xform * obj.parent->world_transform;
			vec3 skew; vec4 perspective; // ignored, should be zero
			glm::decompose(obj.world_transform, const_cast<vec3&>(obj.world_scale),
				const_cast<quat&>(obj.world_rotation), const_cast<vec3&>(obj.world_position),
				skew, perspective);
		}
	});
}

void GameObject::RecursiveLateUpdate(Engine& engine) {
	Recurse(nullptr, [&](GameObject& obj) { obj.LateUpdate(engine); });
}

String GameObject::DebugName() {
	return String::format("%s <%jx> [%.02f %.02f %.02f]", Name().cstr, uintptr_t(this),
		position.x, position.y, position.z);
}
