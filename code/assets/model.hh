#pragma once

#include <vector>

#include "base/base.hh"
#include "base/string.hh"
#include "assets/texture.hh"
#include "assets/mesh.hh"
#include "assets/material.hh"

struct Model {
	String display_name;
	String source_path;
	std::vector<Texture*> textures;
	std::vector<Material*> materials;
	struct MeshRecord {
		Mesh mesh;
		mat4 transform;
		Material* material;
	};
	std::vector<MeshRecord> meshes;
};
