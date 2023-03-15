#include "scene/scene.hh"

void Scene::ReserveWithCount(GameObject::Type type, uint32_t type_size, uint32_t new_count) {
	if (ExpectFalse(capacities[type] == 0)) {
		// Allocate backing storage in increments of 4 KiB pages
		uint32_t new_size = ((new_count * type_size) + 4095) / 4096 * 4096;
		capacities[type] = new_size / type_size;
		arrays[type] = (GameObject*) calloc(new_size, 1);
		return;
	}

	uint32_t old_size = ((capacities[type] * type_size) + 4095) / 4096 * 4096;
	while (new_count > capacities[type]) {
		capacities[type] *= 2;
	}
	uint32_t new_size = ((capacities[type] * type_size) + 4095) / 4096 * 4096;
	auto* new_array = (GameObject*) calloc(new_size, 1);

	// GameObjects can contain pointers to other objects in the same Scene. We'll have to
	// rewrite any pointers into the array that we're reallocating.
	char* new_array_fst = reinterpret_cast<char*>(new_array);
	char* old_array_fst = reinterpret_cast<char*>(arrays[type]);
	char* old_array_pte = reinterpret_cast<char*>(arrays[type]) + old_size;
	for (GameObject& obj : IterAll()) {
		char* parent_ptr = reinterpret_cast<char*>(obj.parent);
		if (parent_ptr >= old_array_fst && parent_ptr < old_array_pte) {
			char* new_parent_ptr = (parent_ptr - old_array_fst) + new_array_fst;
			obj.parent = reinterpret_cast<GameObject*>(new_parent_ptr);
		}
	}

	memcpy(new_array, arrays[type], old_size);
	free(arrays[type]);
	arrays[type] = new_array;
}
