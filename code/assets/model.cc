#include "assets/model.hh"
#include "assets/asset_loader.hh"

#include <unordered_map>

#include <SDL.h>
#include <parson.h>

#include "base/debug.hh"
#include "base/filesystem.hh"

static bool ModelLoader_Initialised = false;
std::unordered_map<uint64_t, Model> ModelLoader_Cache = {};

void InitModelLoader() {
	if (ModelLoader_Initialised) { return; }
	ModelLoader_Cache.reserve(32);
	ModelLoader_Initialised = true;
}

Model* GetModelFromGLTF(uint64_t source_path_hash, const char* source_path) {
	Model& model = ModelLoader_Cache[source_path_hash];
	if (model.source_path != nullptr) { return &model; }

	float ticks_per_msec = float(SDL_GetPerformanceFrequency()) / 1000.0f;
	uint64_t time_get_start = SDL_GetPerformanceCounter();

	model.source_path = String::copy(source_path);

	char* last_slash = nullptr;
	for (char& chr : model.source_path) {
		if (chr == '/' || chr == '\\') { last_slash = &chr; }
	}
	CHECK_NOTNULL_F(last_slash, "Invalid path: %s", source_path);
	model.display_name = String::view(&last_slash[1]);
	uint32_t gltf_directory_chars = (uint32_t)(last_slash - model.source_path.cstr);
	String gltf_directory = String(gltf_directory_chars);
	memcpy(gltf_directory.mut(), model.source_path.cstr, gltf_directory_chars);
	gltf_directory.mut()[gltf_directory_chars] = '\0';

	LOG_F(INFO, "Loading model from path %s", model.source_path.cstr);
	LOG_F(INFO, "Model display name: %s", model.display_name.cstr);
	LOG_F(INFO, "GLTF root directory: %s", gltf_directory.cstr);

	JSON_Value* rootval = json_parse_file_with_comments(source_path);
	if (!rootval) {
		LOG_F(ERROR, "Failed to parse GLTF JSON file: %s", source_path);
		return &model;
	}
	JSON_Object* root = json_value_get_object(rootval);

	String gltf_version = String::view(json_object_dotget_string(root, "asset.version"));
	if (!gltf_version || gltf_version != "2.0") {
		LOG_F(ERROR, "Can only load glTF 2.0 models; model version is %s", gltf_version.cstr);
	}

	// Extract GLTF buffers:
	JSON_Array* jbuffers = json_object_get_array(root, "buffers");
	auto buffer_sizes = std::vector<size_t>(json_array_get_count(jbuffers));
	auto buffer_datas = std::vector<uint8_t*>(json_array_get_count(jbuffers));
	for (size_t ibuf = 0; ibuf < json_array_get_count(jbuffers); ibuf++) {
		JSON_Object* jbuf = json_array_get_object(jbuffers, ibuf);
		// TODO: This can be a data URI, maybe we should support that?
		const char* uri = json_object_get_string(jbuf, "uri");
		size_t size = (size_t) json_object_get_number(jbuf, "byteLength");
		if (uri && size) {
			String src = String::format("%s/%s", gltf_directory.cstr, uri);
			buffer_sizes[ibuf] = size;
			buffer_datas[ibuf] = (uint8_t*)(ReadFile(src).leak().cstr);
		}
		if (!buffer_datas[ibuf]) {
			buffer_sizes[ibuf] = 0;
			LOG_F(WARNING, "Failed to read buffer %zu (%s) from model", ibuf, uri);
		}
	}

	// Convert accessors to Buffer objects:
	JSON_Array* jaccessors = json_object_get_array(root, "accessors");
	JSON_Array* jbufferviews = json_object_get_array(root, "bufferViews");
	auto buffers = std::vector<Buffer>(json_array_get_count(jaccessors));
	for (uint32_t iacc = 0; iacc < json_array_get_count(jaccessors); iacc++) {
		JSON_Object* jacc = json_array_get_object(jaccessors, iacc);
		if (!json_object_has_value(jacc, "bufferView") || json_object_has_value(jacc, "sparse")) {
			LOG_F(WARNING, "Unable to load accessor %u (sparse accessors are not supported)", iacc);
			continue;
		}
		uint32_t ibv = (uint32_t) json_object_get_number(jacc, "bufferView");
		JSON_Object* jbv = json_array_get_object(jbufferviews, ibv);
		// Retrieve accessor properties:
		const char* jtype  = json_object_get_string(jacc, "type");
		GLenum jcomptype   = (GLenum) json_object_get_number(jacc, "componentType");
		uint32_t jcount    = (uint32_t) json_object_get_number(jacc, "count");
		uint32_t joffset1  = (uint32_t) json_object_get_number(jacc, "byteOffset"); // default 0
		// Retrive bufferview properties:
		uint32_t ibuf      = (uint32_t) json_object_get_number(jbv, "buffer");
		uint32_t joffset2  = (uint32_t) json_object_get_number(jbv, "byteOffset"); // default 0
		// Probably don't need to read byteStride, since we can infer it from etype/ctype?
		// Set up and and stage buffer:
		buffers[iacc].etype = ElementType::FromGLTFType(jtype);
		buffers[iacc].ctype = ComponentType::FromGLEnum(jcomptype);
		buffers[iacc].elements = jcount;
		buffers[iacc].cpu_buffer = static_cast<uint8_t*>(malloc(buffers[iacc].Size()));
		memcpy(buffers[iacc].cpu_buffer, &buffer_datas[ibuf][joffset1 + joffset2], buffers[iacc].Size());

		// FIXME: Upload buffer to GPU!

		LOG_F(INFO, "-> buf=%u bv=%u acc=%u: %u bytes @ %p, %u elements, etype %s ctype %s",
			ibuf, ibv, iacc, buffers[iacc].Size(), buffers[iacc].cpu_buffer, buffers[iacc].elements,
			buffers[iacc].etype.GLTFType(), buffers[iacc].ctype.Name());
	}

	// Extract samplers:
	JSON_Array* jsamplers  = json_object_get_array(root, "samplers");
	auto samplers = std::vector<Sampler*>(json_array_get_count(jsamplers));
	auto sampler_params = std::vector<SamplerParams>(json_array_get_count(jsamplers));
	auto sampler_needs_mips = std::vector<bool>(json_array_get_count(jsamplers));
	// Set sampler parameters:
	for (uint32_t ismp = 0; ismp < json_array_get_count(jsamplers); ismp++) {
		JSON_Object* jsmp = json_array_get_object(jsamplers, ismp);
		GLenum min_filter = (GLenum) json_object_get_number(jsmp, "minFilter");
		GLenum mag_filter = (GLenum) json_object_get_number(jsmp, "magFilter");
		GLenum wrap_s = (GLenum) json_object_get_number(jsmp, "wrapS");
		GLenum wrap_t = (GLenum) json_object_get_number(jsmp, "wrapT");
		// GLTF uses OpenGL enums so we don't have to translate
		sampler_params[ismp].min_filter = min_filter ? min_filter : GL_LINEAR;
		sampler_params[ismp].mag_filter = mag_filter ? mag_filter : GL_LINEAR;
		sampler_params[ismp].wrap_s = wrap_s ? wrap_s : GL_REPEAT;
		sampler_params[ismp].wrap_t = wrap_t ? wrap_t : GL_REPEAT;
		// We need to keep track, for each sampler, whether or not it needs associated textures to
		// be mipmap complete. If a texture is ever sampled with one of the MIPMAP samplers, we'll
		// need to generate mips for it in the next step.
		sampler_needs_mips[ismp] =
			min_filter == GL_NEAREST_MIPMAP_NEAREST || min_filter == GL_LINEAR_MIPMAP_NEAREST ||
			min_filter == GL_NEAREST_MIPMAP_LINEAR  || min_filter == GL_LINEAR_MIPMAP_LINEAR;
		// Upload sampler to OpenGL
		samplers[ismp] = GetSampler(sampler_params[ismp]);

		auto enum_to_str = [](GLenum e) { switch(e) {
			case GL_NEAREST: return "NEAREST";
			case GL_LINEAR: return "LINEAR";
			case GL_NEAREST_MIPMAP_NEAREST: return "NEAREST_MIPMAP_NEAREST";
			case GL_LINEAR_MIPMAP_NEAREST: return "LINEAR_MIPMAP_NEAREST";
			case GL_NEAREST_MIPMAP_LINEAR: return "NEAREST_MIPMAP_LINEAR";
			case GL_LINEAR_MIPMAP_LINEAR: return "LINEAR_MIPMAP_LINEAR";
			case GL_REPEAT: return "REPEAT";
			case GL_MIRRORED_REPEAT: return "MIRRORED_REPEAT";
			case GL_CLAMP_TO_BORDER: return "CLAMP_TO_BORDER";
			case 0: return "0"; default: return "<?>";
		}};
		LOG_F(INFO, "-> smp=%u min=%s mag=%s wrapS=%s wrapT=%s mips=%u gl=%u", ismp, enum_to_str(min_filter),
			enum_to_str(mag_filter), enum_to_str(wrap_s), enum_to_str(wrap_t), bool(sampler_needs_mips[ismp]),
			samplers[ismp]->gl_sampler);
	}

	// Create Texture objects from GLTF images:
	JSON_Array* jimages = json_object_get_array(root, "images");
	JSON_Array* jtextures = json_object_get_array(root, "textures");
	auto textures = std::vector<Texture*>(json_array_get_count(jimages));
	for (uint32_t iimg = 0; iimg < json_array_get_count(jimages); iimg++) {
		JSON_Object* jimg = json_array_get_object(jimages, iimg);

		bool texture_needs_mips = false;
		for (uint32_t itex = 0; itex < json_array_get_count(jtextures); itex++) {
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler") &&
				uint32_t(json_object_get_number(jtex, "source")) == iimg)
			{
				uint32_t ismp = uint32_t(json_object_get_number(jtex, "sampler"));
				if (sampler_needs_mips[ismp]) { texture_needs_mips = true; break; }
			}
		}

		const char* uri = json_object_get_string(jimg, "uri");
		if (uri) {
			String src = String::format("%s/%s", gltf_directory.cstr, uri);
			textures[iimg] = GetTexture(src, texture_needs_mips);
			LOG_F(INFO, "-> img=%u %ux%u levels=%u gl=%u %s", iimg, textures[iimg]->width, textures[iimg]->height,
				textures[iimg]->num_levels, textures[iimg]->gl_texture, uri);
		} else {
			LOG_F(WARNING, "Unable to load image %u (images stored in buffers not supported)", iimg);
		}
	}

#if 0

	// Extract materials:
	JSON_Array* jmaterials = json_object_get_array(root, "materials");
	size_t materialCount   = json_array_get_count(jmaterials);
	Material* materials    = Alloc(materialCount, Material);
	for (size_t imat = 0; imat < materialCount; imat++) {
		Material* m = &materials[imat];
		InitMaterial(m);
		JSON_Object* jmat = json_array_get_object(jmaterials, imat);
		JSON_Object* jmr = json_object_get_object(jmat, "pbrMetallicRoughness");
		// Extract material factors:
		JSON_Array* jdiffusefac = json_object_get_array(jmr, "baseColorFactor");
		if (jdiffusefac) {
			m->const_diffuse[0] = (float) json_array_get_number(jdiffusefac, 0);
			m->const_diffuse[1] = (float) json_array_get_number(jdiffusefac, 1);
			m->const_diffuse[2] = (float) json_array_get_number(jdiffusefac, 2);
			m->const_diffuse[3] = (float) json_array_get_number(jdiffusefac, 3);
		}
		if (json_object_has_value(jmr, "metallicFactor")) {
			m->const_metallic = (float) json_object_get_number(jmr, "metallicFactor");
		}
		if (json_object_has_value(jmr, "roughnessFactor")) {
			m->const_roughness = (float) json_object_get_number(jmr, "roughnessFactor");
		}
		// Extract material textures:
		JSON_Object* jdiffusetex = json_object_get_object(jmr, "baseColorTexture");
		JSON_Object* jmetrghtex  = json_object_get_object(jmr, "metallicRoughnessTexture");
		JSON_Object* jnormaltex  = json_object_get_object(jmat, "normalTexture");
		JSON_Object* jocctex     = json_object_get_object(jmat, "occlusionTexture");
		if (jdiffusetex) {
			int itex = (int) json_object_get_number(jdiffusetex, "index");
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
				int iimg = (int) json_object_get_number(jtex, "source");
				int ismp = (int) json_object_get_number(jtex, "sampler");
				m->tex_diffuse = textures[iimg];
				m->smp_diffuse = samplers[ismp];
			}
		}
		if (jmetrghtex) {
			int itex = (int) json_object_get_number(jmetrghtex, "index");
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
				int iimg = (int) json_object_get_number(jtex, "source");
				int ismp = (int) json_object_get_number(jtex, "sampler");
				m->tex_occ_rgh_met = textures[iimg];
				m->smp_occ_rgh_met = samplers[ismp];
			}
		}
		if (jnormaltex) {
			int itex = (int) json_object_get_number(jnormaltex, "index");
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
				int iimg = (int) json_object_get_number(jtex, "source");
				int ismp = (int) json_object_get_number(jtex, "sampler");
				m->tex_normal = textures[iimg];
				m->smp_normal = samplers[ismp];
			}
		}
		if (jocctex) {
			int itex = (int) json_object_get_number(jocctex, "index");
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
				int iimg = (int) json_object_get_number(jtex, "source");
				int ismp = (int) json_object_get_number(jtex, "sampler");
				m->tex_occlusion = textures[iimg];
				m->smp_occlusion = samplers[ismp];
			}
		}
		// Extract alpha mode: (default is OPAQUE, i.e. no blending or stippling)
		const char* jalphamode = json_object_get_string(jmat, "alphaMode");
		if (json_object_has_value(jmat, "alphaCutoff")) {
			float jalphacutoff = (float) json_object_get_number(jmat, "alphaCutoff");
			m->stipple_hard_cutoff = jalphacutoff;
			m->stipple_soft_cutoff = jalphacutoff;
		}
		if (jalphamode) {
			if      (strcmp(jalphamode, "MASK")  == 0) { m->stipple = true; }
			else if (strcmp(jalphamode, "BLEND") == 0) { m->blend   = true; }
		}
		// Extract cull mode:
		bool jdoublesided = json_object_get_boolean(jmat, "doubleSided");
		if (jdoublesided) { m->cull = false; }
	}

	// Extract nodes and count meshes (GLTF primitives):
	JSON_Array* jnodes  = json_object_get_array(root, "nodes");
	JSON_Array* jmeshes = json_object_get_array(root, "meshes");
	size_t meshCount = 0;
	size_t nodeCount = json_array_get_count(jnodes);
	GLTFNode* nodes  = Alloc(nodeCount, GLTFNode);
	for (size_t inode = 0; inode < nodeCount; inode++) {
		GLTFNode* node = &nodes[inode];
		node->parent = NULL;
	}
	for (size_t inode = 0; inode < nodeCount; inode++) {
		GLTFNode* node = &nodes[inode];
		node->local = mat4(1);
		node->scene = mat4(1);
		JSON_Object* jnode = json_array_get_object(jnodes, inode);
		// Update this node's children:
		JSON_Array* jchildren = json_object_get_array(jnode, "children");
		if (jchildren) {
			for (size_t ichild = 0; ichild < json_array_get_count(jchildren); ichild++) {
				int ichildnode = (int) json_array_get_number(jchildren, ichild);
				nodes[ichildnode].parent = node;
			}
		}
		// Extract or generate local transform matrix:
		JSON_Array* jmat = json_object_get_array(jnode, "matrix");
		if (jmat) {
			// Both GLM and GLSL use column-major order for matrices, so just use a loop:
			for (int ipos = 0; ipos < 16; ipos++) {
				((float*)&node->local)[ipos] = (float) json_array_get_number(jmat, ipos);
			}
		} else {
			quat r = quat(1, 0, 0, 0);
			vec3 t = vec3(0);
			vec3 s = vec3(1);
			JSON_Array* jr = json_object_get_array(jnode, "rotation");
			JSON_Array* jt = json_object_get_array(jnode, "translation");
			JSON_Array* js = json_object_get_array(jnode, "scale");
			if (jr) {
				r[0] = (float) json_array_get_number(jr, 0); // x
				r[1] = (float) json_array_get_number(jr, 1); // y
				r[2] = (float) json_array_get_number(jr, 2); // z
				r[3] = (float) json_array_get_number(jr, 3); // w
			}
			if (jt) {
				t[0] = (float) json_array_get_number(jt, 0);
				t[1] = (float) json_array_get_number(jt, 1);
				t[2] = (float) json_array_get_number(jt, 2);
			}
			if (js) {
				s[0] = (float) json_array_get_number(js, 0);
				s[1] = (float) json_array_get_number(js, 1);
				s[2] = (float) json_array_get_number(js, 2);
			}
			// FIXME: Is there a specific order this has to be done in?
			node->local = glm::translate(node->local, t);
			node->local *= glm::mat4_cast(r);
			node->local = glm::scale(node->local, s);
		}
		// Count primitives:
		if (json_object_has_value(jnode, "mesh")) {
			int igltfmesh = (int) json_object_get_number(jnode, "mesh");
			JSON_Object* jmesh = json_array_get_object(jmeshes, igltfmesh);
			meshCount += json_array_get_count(json_object_get_array(jmesh, "primitives"));
		}
	}

	// Compute the scene-space transform matrix for each node:
	for (size_t inode = 0; inode < nodeCount; inode++) {
		GLTFNode* node = &nodes[inode];
		// The correct order is probably node.local * node.parent->scene for each node.
		// We'll do it iteratively instead of recursively, though.
		// FIXME: Is this actually right?
		node->scene = node->local;
		GLTFNode* parent = node->parent;
		while (parent != NULL) {
			node->scene *= parent->local; // correct order?
			parent = parent->parent;
		}
	}

	// Extract meshes (GLTF primitives) from the node structure:
	Mesh* meshes = Alloc(meshCount, Mesh);
	mat4* meshTransforms = Alloc(meshCount, mat4);
	Material** meshMaterials = Alloc(meshCount, Material*);
	memset(meshes, 0, meshCount * sizeof(Mesh));
	size_t imesh = 0;
	for (size_t inode = 0; inode < nodeCount; inode++) {
		GLTFNode* node = &nodes[inode];
		JSON_Object* jnode = json_array_get_object(jnodes, inode);
		if (json_object_has_value(jnode, "mesh")) {
			int igltfmesh = (int) json_object_get_number(jnode, "mesh");
			JSON_Object* jmesh = json_array_get_object(jmeshes, igltfmesh);
			JSON_Array* jprims = json_object_get_array(jmesh, "primitives");
			for (size_t iprim = 0; iprim < json_array_get_count(jprims); iprim++) {
				// Store Mesh object for this primitive:
				JSON_Object* jprim = json_array_get_object(jprims, iprim);
				JSON_Object* jattr = json_object_get_object(jprim, "attributes");
				Mesh* mesh = &meshes[imesh];
				glGenVertexArrays(1, &mesh->gl_vertex_array);
				meshTransforms[imesh] = node->scene;
				// Debug:
				uint32_t meshVertexCount = 0;
				uint32_t meshIndexCount = 0;
				// Read type:
				mesh->type = GL_TRIANGLES;
				mesh->gl_component_count = 3;
				if (json_object_has_value(jprim, "mode")) {
					mesh->type = (int) json_object_get_number(jprim, "mode");
				}
				// Read material:
				if (json_object_has_value(jprim, "material")) {
					int imat = (int) json_object_get_number(jprim, "material");
					meshMaterials[imesh] = &materials[imat];
				}
				// Read and upload indices:
				if (json_object_has_value(jprim, "indices")) {
					int iacc = (int) json_object_get_number(jprim, "indices");
					Accessor& acc = accessors[iacc];
					glGenBuffers(1, &mesh->gl_element_array);
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->gl_element_array);
					glBufferData(GL_ELEMENT_ARRAY_BUFFER,
						(GLsizei)(acc.count * acc.stride), acc.data(), GL_STATIC_DRAW);
					mesh->gl_element_count = acc.count;
					mesh->gl_element_type = acc.glComponentType();
					meshIndexCount += acc.count;
				}
				// Read and upload attributes:
				glBindVertexArray(mesh->gl_vertex_array);
				#define X(name, location, glslName, gltfName) { \
					if (json_object_has_value(jattr, gltfName)) { \
						int iacc = (int) json_object_get_number(jattr, gltfName); \
						Accessor& acc = accessors[iacc]; \
						GLuint vbo; \
						glGenBuffers(1, &vbo); \
						glBindBuffer(GL_ARRAY_BUFFER, vbo); \
						glBufferData(GL_ARRAY_BUFFER, \
							(GLsizei)(acc.count * acc.stride), acc.data(), GL_STATIC_DRAW); \
						glEnableVertexAttribArray(location); \
						glVertexAttribPointer(location, acc.componentCount(), \
							acc.glComponentType(), false, acc.stride, NULL); \
						if (location == 0) { meshVertexCount += acc.count; } \
					} \
				}
				XM_PROGRAM_ATTRIBUTES
				#undef X
				mesh->gl_vertex_count = meshVertexCount;
				// Compute bounding box:
				// FIXME: All GLTF primitives have a POSITION attribute, right?
				int iPosAcc = (int) json_object_get_number(jattr, "POSITION");
				Accessor& posAcc = accessors[iPosAcc];
				CHECK_EQ_F(posAcc.componentCount(), 3);
				vec3 aabbMin = { INFINITY,  INFINITY,  INFINITY};
				vec3 aabbMax = {-INFINITY, -INFINITY, -INFINITY};
				for (size_t i = 0; i < posAcc.count; i++) {
					// This looks pretty scary, but it really just casts a particular set of three
					// floats in our positions buffer to a vec3.
					aabbMin = glm::min(*(vec3*)&(((float*)posAcc.data())[i*3]), aabbMin);
					aabbMax = glm::max(*(vec3*)&(((float*)posAcc.data())[i*3]), aabbMax);
				}
				mesh->aabbCenter = (aabbMin + aabbMax) / vec3(2);
				mesh->aabbHalfExtent = aabbMax - mesh->aabbCenter;
				mesh->hasAABB = true;
				LOG_F(INFO, "Mesh 0x%jx: %lu vert %lu ind, VAO %u, EBO %u, AABB ctr (%.02f %.02f %.02f) ext (%.02f %.02f %.02f)",
					mesh, meshVertexCount, meshIndexCount, mesh->gl_vertex_array, mesh->gl_element_array,
					mesh->aabbCenter.x, mesh->aabbCenter.y, mesh->aabbCenter.z,
					mesh->aabbHalfExtent.x, mesh->aabbHalfExtent.y, mesh->aabbHalfExtent.z);
				imesh++;
			}
		}
	}
	meshCount = imesh;

	// Free temporary storage:
	for (size_t i = 0; i < bufferCount; i++) {
		free(buffers[i]);
	}
	Free(accessors);
	Free(buffers);
	Free(bufferSizes);
	Free(samplers);
	Free(samplerNeedsMips);
	Free(nodes);

	// Fill out model fields:
	// TODO: Add an atomic lock to the model so we can load it on another thread.
	model->textureCount = textureCount;
	model->textures = textures;
	model->materialCount = materialCount;
	model->materials = materials;
	model->meshCount = meshCount;
	model->meshTransforms = meshTransforms;
	model->meshMaterials = meshMaterials;
	model->meshes = meshes;
#endif

	json_value_free(rootval);

	return &model;
}
