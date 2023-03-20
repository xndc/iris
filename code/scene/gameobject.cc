#include "scene/gameobject.hh"
#include "engine/engine.hh"

#include <vector>

GameObject::GameObject() {
	static uint32_t next_idx = 1;
	name = String::format("%s #%u", this->Type().name, next_idx++);
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

GameObject* GameObject::AddLink(GameObject* object) {
	if (ExpectFalse(object == nullptr)) { return nullptr; }
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

GameObject* GameObject::Add(GameObject* object) {
	if (ExpectFalse(object == nullptr)) { return nullptr; }
	object->parent = this;
	return AddLink(object);
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

void GameObject::Recurse(RecurseMode mode, bool follow_links, std::function<void(GameObject&)> func,
	bool internal_increment_tag)
{
	static uint8_t new_recurse_tag = 0;
	if (internal_increment_tag) {
		new_recurse_tag++;
	}
	recurse_tag = new_recurse_tag;
	if (mode == RecurseMode::ParentBeforeChildren) {
		func(*this);
	}
	for (GameObject& child : *this) {
		if (child.recurse_tag != new_recurse_tag && (follow_links || child.parent == parent)) {
			child.Recurse(mode, follow_links, func, false);
		}
	}
	if (mode == RecurseMode::ChildrenBeforeParent) {
		func(*this);
	}
}

void GameObject::RecursiveUpdate(Engine& engine) {
	Recurse(RecurseMode::ChildrenBeforeParent, true, [&](GameObject& obj) { obj.Update(engine); });
}

void GameObject::RecursiveUpdateTransforms() {
	Recurse(RecurseMode::ParentBeforeChildren, true, [&](GameObject& obj) {
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
	Recurse(RecurseMode::ChildrenBeforeParent, true, [&](GameObject& obj) { obj.LateUpdate(engine); });
}
