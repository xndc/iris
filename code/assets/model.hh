#pragma once

#include <vector>

#include "base/base.hh"
#include "base/string.hh"
#include "base/hash.hh"
#include "assets/texture.hh"
#include "assets/mesh.hh"
#include "assets/material.hh"
#include "scene/gameobject.hh"

struct Model {
	String display_name;
	String source_path;
	std::vector<Buffer*> buffers;
	std::vector<BufferView*> buffer_views;
	std::vector<Texture*> textures;
	std::vector<Sampler*> samplers;
	std::vector<Material*> materials;
	std::vector<Mesh*> meshes;
	GameObject* root_object;
	std::vector<GameObject*> objects;
};

Model* GetModelFromGLTF(uint64_t source_path_hash, const char* source_path);

static FORCEINLINE Model* GetModelFromGLTF(const char* source_path) {
	return GetModelFromGLTF(Hash64(source_path), source_path);
}
