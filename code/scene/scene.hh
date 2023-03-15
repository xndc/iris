#pragma once

#include <span>

#include "scene/gameobject.hh"

struct Scene {
	// One array for each GameObject subclass
	GameObject* arrays  [GameObject::TypeCount] = {};
	uint32_t capacities [GameObject::TypeCount] = {};
	uint32_t counts     [GameObject::TypeCount] = {};

	// Use IterAll() to iterate over every GameObject in this scene.
	// TODO: This iterator is safe even if an object array gets reallocated, meaning it has to do
	// some pointer chasing. Would it be useful to have an UnsafeIterAll() for when we don't expect
	// any backing storage to be reallocated, e.g. during the loop in ReserveWithCount?
	struct AllObjectsIterator {
		Scene* scene;
		uint32_t arrayIdx = 0;
		uint32_t objectIdx = 0;
		FORCEINLINE constexpr AllObjectsIterator(Scene* scene, uint32_t arrayIdx) :
			scene{scene}, arrayIdx{arrayIdx} {}
		FORCEINLINE AllObjectsIterator begin() {
			uint32_t first_nonempty_array = 0;
			while (scene->counts[first_nonempty_array] == 0 && first_nonempty_array < CountOf(scene->arrays)) {
				first_nonempty_array++;
			}
			return AllObjectsIterator(scene, first_nonempty_array);
		}
		FORCEINLINE AllObjectsIterator end() { return AllObjectsIterator{scene, CountOf(scene->arrays)}; }
		FORCEINLINE GameObject& operator*() { return scene->arrays[arrayIdx][objectIdx]; }
		FORCEINLINE AllObjectsIterator& operator++() {
			objectIdx++;
			while (arrayIdx < CountOf(scene->arrays) && objectIdx >= scene->counts[arrayIdx]) {
				objectIdx = 0;
				arrayIdx++;
			}
			return *this;
		}
		FORCEINLINE bool operator!=(const AllObjectsIterator& rhs) {
			return arrayIdx != rhs.arrayIdx || objectIdx != rhs.objectIdx;
		}
	};
	FORCEINLINE constexpr AllObjectsIterator IterAll() {
		return AllObjectsIterator(this, 0);
	}

	// Use Iter<GameObjT>() to iterate over every GameObject of subtype GameObjT in this scene.
	// TODO: This iterator is safe even if an object array gets reallocated, meaning it has to do
	// some pointer chasing. Would it be useful to have an UnsafeIter<GameObjT>() for when we don't expect
	// any backing storage to be reallocated, e.g. while rendering?
	template<typename GameObjT> struct ObjectIterator {
		Scene* scene;
		uint32_t arrayIdx = 0;
		uint32_t objectIdx = 0;
		FORCEINLINE constexpr ObjectIterator(Scene* scene, uint32_t arrayIdx, uint32_t objectIdx) :
			scene{scene}, arrayIdx{arrayIdx}, objectIdx{objectIdx} {}
		FORCEINLINE ObjectIterator begin() { return ObjectIterator{scene, arrayIdx, 0}; }
		FORCEINLINE ObjectIterator end() { return ObjectIterator{scene, arrayIdx, scene->counts[GameObjT::type]}; }
		FORCEINLINE GameObjT& operator*() { return static_cast<GameObjT&>(scene->arrays[arrayIdx][objectIdx]); }
		FORCEINLINE ObjectIterator& operator++() { objectIdx++; return *this; }
		FORCEINLINE bool operator!=(const ObjectIterator& rhs) { return objectIdx != rhs.objectIdx; }
	};
	template<typename GameObjT> FORCEINLINE constexpr ObjectIterator<GameObjT> Iter() {
		return ObjectIterator<GameObjT>(this, GameObjT::type, 0);
	}

	void ReserveWithCount(GameObject::Type type, uint32_t type_size, uint32_t new_count);

	FORCEINLINE void ReserveExtraSlots(GameObject::Type type, uint32_t type_size, uint32_t slots) {
		uint32_t new_count = counts[type] + slots;
		if (ExpectFalse(new_count > capacities[type])) {
			ReserveWithCount(type, type_size, new_count);
		}
	}

	template<typename GameObjT> FORCEINLINE GameObjT& New() {
		ReserveExtraSlots(GameObjT::type, sizeof(GameObjT), 1);
		uint32_t new_index = counts[GameObjT::type]++;
		return static_cast<GameObjT&>(arrays[GameObjT::type][new_index]);
	}

	template<typename GameObjT> FORCEINLINE std::span<GameObjT> New(uint32_t count) {
		ReserveExtraSlots(GameObjT::type, sizeof(GameObjT), count);
		uint32_t start_index = counts[GameObjT::type];
		counts[GameObjT::type] += count;
		return std::span<GameObjT>(static_cast<GameObjT*>(&arrays[GameObjT::type][start_index]), count);
	}
};
