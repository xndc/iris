#include "assets/model.hh"
#include "assets/asset_loader.hh"

#include <unordered_map>

#include <SDL.h>
#include <parson.h>

#include "base/debug.hh"
#include "base/filesystem.hh"
#include "graphics/defaults.hh"
#include "scene/gameobject.hh"

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
	LOG_F(INFO, "-> directory [%s] name [%s]", gltf_directory.cstr, model.display_name.cstr);

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

	// Read buffers from disk:
	// Note that GLTF buffers may contain both vertex and index data, so we can't directly upload
	// them to OpenGL because of WebGL2 limitations. We'll have to extract bits of them manually.
	JSON_Array* jbuffers = json_object_get_array(root, "buffers");
	auto buffer_sizes = std::vector<size_t>(json_array_get_count(jbuffers));
	auto buffer_datas = std::vector<uint8_t*>(json_array_get_count(jbuffers));
	for (uint32_t igbuf = 0; igbuf < json_array_get_count(jbuffers); igbuf++) {
		JSON_Object* jbuf = json_array_get_object(jbuffers, igbuf);
		// TODO: This can be a data URI, maybe we should support that?
		const char* uri = json_object_get_string(jbuf, "uri");
		uint32_t size = (uint32_t) json_object_get_number(jbuf, "byteLength");
		if (uri && size) {
			String src = String::format("%s/%s", gltf_directory.cstr, uri);
			buffer_sizes[igbuf] = size;
			buffer_datas[igbuf] = reinterpret_cast<uint8_t*>(ReadFile(src).leak_mut());
		}
		if (!buffer_datas[igbuf]) {
			LOG_F(WARNING, "Failed to read buffer %u (%s) from model", igbuf, uri);
		}
	}

	// Convert GLTF buffer-views to Buffer objects:
	JSON_Array* jbufferviews = json_object_get_array(root, "bufferViews");
	auto buffers = std::vector<Buffer*>(json_array_get_count(jbufferviews));
	auto gl_buffers = std::vector<GLuint>(buffers.size());
	glGenBuffers(gl_buffers.size(), gl_buffers.data());
	for (uint32_t ibuf = 0; ibuf < json_array_get_count(jbufferviews); ibuf++) {
		JSON_Object* jbv = json_array_get_object(jbufferviews, ibuf);
		uint32_t igbuf = uint32_t(json_object_get_number(jbv, "buffer"));
		uint32_t offset = uint32_t(json_object_get_number(jbv, "byteOffset"));
		// TODO: GLTF also defines a byteStride. Do we need to store this somewhere, or can it
		// always be inferred from the accessor properties?
		buffers[ibuf] = new Buffer();
		buffers[ibuf]->size = json_object_get_number(jbv, "byteLength");
		buffers[ibuf]->cpu_buffer = &buffer_datas[igbuf][offset];
		buffers[ibuf]->gpu_handle = gl_buffers[igbuf];
	}

	// Convert GLTF accessors to BufferView objects:
	JSON_Array* jaccessors = json_object_get_array(root, "accessors");
	auto buffer_views = std::vector<BufferView*>(json_array_get_count(jaccessors));
	for (uint32_t ibv = 0; ibv < json_array_get_count(jaccessors); ibv++) {
		JSON_Object* jacc = json_array_get_object(jaccessors, ibv);
		if (!json_object_has_value(jacc, "bufferView") || json_object_has_value(jacc, "sparse")) {
			LOG_F(WARNING, "Unable to load accessor %u (sparse accessors are not supported)", ibv);
			continue;
		}

		uint32_t ibuf = uint32_t(json_object_get_number(jacc, "bufferView"));
		const char* gltf_etype = json_object_get_string(jacc, "type");
		GLenum gl_ctype = GLenum(json_object_get_number(jacc, "componentType"));
		uint32_t gltf_count = (uint32_t) json_object_get_number(jacc, "count");
		uint32_t gltf_offset = (uint32_t) json_object_get_number(jacc, "byteOffset"); // default 0

		buffer_views[ibv] = new BufferView();
		buffer_views[ibv]->buffer = buffers[ibuf];
		buffer_views[ibv]->etype = ElementType::from_gltf_type(gltf_etype);
		buffer_views[ibv]->ctype = ComponentType::from_gl_enum(gl_ctype);
		buffer_views[ibv]->elements = gltf_count;
		buffer_views[ibv]->offset = gltf_offset;

		// The buffer will be uploaded to the GPU once we've gone through all the meshes to see if
		// this is a vertex buffer or an index buffer.
		JSON_Object* jbv = json_array_get_object(jbufferviews, ibuf);
		uint32_t igbuf = uint32_t(json_object_get_number(jbv, "buffer"));
		LOG_F(INFO, "-> buf=%u bv=%u acc=%u: size=%u elements=%u etype=%s ctype=%s cpu=%p",
			igbuf, ibuf, ibv, buffer_views[ibv]->size(), buffer_views[ibv]->elements,
			buffer_views[ibv]->etype.gltf_type(), buffer_views[ibv]->ctype.name(),
			&buffers[ibuf]->cpu_buffer[buffer_views[ibv]->offset]);
	}

	// Extract samplers:
	JSON_Array* jsamplers  = json_object_get_array(root, "samplers");
	auto samplers = std::vector<Sampler*>(json_array_get_count(jsamplers));
	auto sampler_needs_mips = std::vector<bool>(json_array_get_count(jsamplers));
	// Set sampler parameters:
	for (uint32_t ismp = 0; ismp < json_array_get_count(jsamplers); ismp++) {
		JSON_Object* jsmp = json_array_get_object(jsamplers, ismp);
		GLenum min_filter = (GLenum) json_object_get_number(jsmp, "minFilter");
		GLenum mag_filter = (GLenum) json_object_get_number(jsmp, "magFilter");
		GLenum wrap_s = (GLenum) json_object_get_number(jsmp, "wrapS");
		GLenum wrap_t = (GLenum) json_object_get_number(jsmp, "wrapT");
		// GLTF uses OpenGL enums so we don't have to translate
		SamplerParams sampler_params;
		sampler_params.min_filter = min_filter ? min_filter : GL_LINEAR;
		sampler_params.mag_filter = mag_filter ? mag_filter : GL_LINEAR;
		sampler_params.wrap_s = wrap_s ? wrap_s : GL_REPEAT;
		sampler_params.wrap_t = wrap_t ? wrap_t : GL_REPEAT;
		// We need to keep track, for each sampler, whether or not it needs associated textures to
		// be mipmap complete. If a texture is ever sampled with one of the MIPMAP samplers, we'll
		// need to generate mips for it in the next step.
		sampler_needs_mips[ismp] =
			min_filter == GL_NEAREST_MIPMAP_NEAREST || min_filter == GL_LINEAR_MIPMAP_NEAREST ||
			min_filter == GL_NEAREST_MIPMAP_LINEAR  || min_filter == GL_LINEAR_MIPMAP_LINEAR;
		// Upload sampler to OpenGL
		samplers[ismp] = GetSampler(sampler_params);

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

	// Extract materials:
	JSON_Array* jmaterials = json_object_get_array(root, "materials");
	auto materials = std::vector<Material*>(json_array_get_count(jmaterials));
	for (uint32_t imat = 0; imat < json_array_get_count(jmaterials); imat++) {
		materials[imat] = new Material();
		Material& m = *materials[imat];
		JSON_Object* jmat = json_array_get_object(jmaterials, imat);

		// Base material properties:
		switch (Hash64(json_object_get_string(jmat, "alphaMode"))) {
			case Hash64("MASK"):  m.blend_mode = BlendMode::Stippled;    break;
			case Hash64("BLEND"): m.blend_mode = BlendMode::Transparent; break;
			default: m.blend_mode = BlendMode::Opaque;
		}
		if (json_object_has_value(jmat, "alphaCutoff")) {
			m.stipple_hard_cutoff = float(json_object_get_number(jmat, "alphaCutoff"));
			m.stipple_soft_cutoff = m.stipple_hard_cutoff;
		}
		if (json_object_get_boolean(jmat, "doubleSided")) {
			m.face_culling_mode = GL_NONE;
		}
		LOG_F(INFO, "-> material=%u [%p] %s cutoff=%.02f cull=%s", imat, &m,
			m.blend_mode == BlendMode::Stippled ? "stippled" :
			m.blend_mode == BlendMode::Transparent ? "transparent" : "opaque",
			m.stipple_hard_cutoff,
			m.face_culling_mode == GL_BACK ? "back" :
			m.face_culling_mode == GL_FRONT ? "front" :
			m.face_culling_mode == GL_FRONT_AND_BACK ? "both" : "none");

		// Base material textures:
		JSON_Object* jnormaltex = json_object_get_object(jmat, "normalTexture");
		if (jnormaltex) {
			int itex = (int) json_object_get_number(jnormaltex, "index");
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
				SamplerBinding& normal = m.samplers[m.num_samplers++];
				normal.name = DefaultUniforms::TexNormal.name;
				normal.texture = textures[uint32_t(json_object_get_number(jtex, "source"))];
				normal.sampler = samplers[uint32_t(json_object_get_number(jtex, "sampler"))];
				LOG_F(INFO, "-> material=%u %s gltex=%u", imat, normal.name, normal.texture->gl_texture);
			}
		}
		JSON_Object* jocctex = json_object_get_object(jmat, "occlusionTexture");
		if (jocctex) {
			int itex = (int) json_object_get_number(jocctex, "index");
			JSON_Object* jtex = json_array_get_object(jtextures, itex);
			if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
				SamplerBinding& occlusion = m.samplers[m.num_samplers++];
				occlusion.name = DefaultUniforms::TexOcclusion.name;
				occlusion.texture = textures[uint32_t(json_object_get_number(jtex, "source"))];
				occlusion.sampler = samplers[uint32_t(json_object_get_number(jtex, "sampler"))];
				LOG_F(INFO, "-> material=%u %s gltex=%u", imat, occlusion.name, occlusion.texture->gl_texture);
			}
		}

		// PBR metallic-roughness material properties:
		JSON_Object* jmr = json_object_get_object(jmat, "pbrMetallicRoughness");
		if (jmr) {
			JSON_Array* jbaseColorFactor = json_object_get_array(jmr, "baseColorFactor");
			if (jbaseColorFactor) {
				UniformValue& const_albedo = m.uniforms[m.num_uniforms++];
				const_albedo.name = DefaultUniforms::ConstAlbedo.name;
				const_albedo.etype = ElementType::VEC4;
				const_albedo.ctype = ComponentType::F32;
				const_albedo.vec4.f32.r = float(json_array_get_number(jbaseColorFactor, 0));
				const_albedo.vec4.f32.g = float(json_array_get_number(jbaseColorFactor, 1));
				const_albedo.vec4.f32.b = float(json_array_get_number(jbaseColorFactor, 2));
				const_albedo.vec4.f32.a = float(json_array_get_number(jbaseColorFactor, 3));
				LOG_F(INFO, "-> material=%u %s vec4.f32 %.02f %.02f %.02f %.02f", imat, const_albedo.name,
					const_albedo.vec4.f32.r, const_albedo.vec4.f32.g,
					const_albedo.vec4.f32.b, const_albedo.vec4.f32.a);
			}
			if (json_object_has_value(jmr, "metallicFactor")) {
				UniformValue& const_metallic = m.uniforms[m.num_uniforms++];
				const_metallic.name = DefaultUniforms::ConstMetallic.name;
				const_metallic.etype = ElementType::SCALAR;
				const_metallic.ctype = ComponentType::F32;
				const_metallic.scalar.f32 = float(json_object_get_number(jmr, "metallicFactor"));
				LOG_F(INFO, "-> material=%u %s vec4.f32 %.02f %.02f %.02f %.02f", imat, const_metallic.name,
					const_metallic.vec4.f32.r, const_metallic.vec4.f32.g,
					const_metallic.vec4.f32.b, const_metallic.vec4.f32.a);
			}
			if (json_object_has_value(jmr, "roughnessFactor")) {
				UniformValue& const_roughness = m.uniforms[m.num_uniforms++];
				const_roughness.name = DefaultUniforms::ConstRoughness.name;
				const_roughness.etype = ElementType::SCALAR;
				const_roughness.ctype = ComponentType::F32;
				const_roughness.scalar.f32 = float(json_object_get_number(jmr, "roughnessFactor"));
				LOG_F(INFO, "-> material=%u %s vec4.f32 %.02f %.02f %.02f %.02f", imat, const_roughness.name,
					const_roughness.vec4.f32.r, const_roughness.vec4.f32.g,
					const_roughness.vec4.f32.b, const_roughness.vec4.f32.a);
			}
			JSON_Object* jbaseColorTexture = json_object_get_object(jmr, "baseColorTexture");
			if (jbaseColorTexture) {
				int itex = (int) json_object_get_number(jbaseColorTexture, "index");
				JSON_Object* jtex = json_array_get_object(jtextures, itex);
				if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
					SamplerBinding& albedo = m.samplers[m.num_samplers++];
					albedo.name = DefaultUniforms::TexAlbedo.name;
					albedo.texture = textures[uint32_t(json_object_get_number(jtex, "source"))];
					albedo.sampler = samplers[uint32_t(json_object_get_number(jtex, "sampler"))];
					LOG_F(INFO, "-> material=%u %s gltex=%u", imat, albedo.name, albedo.texture->gl_texture);
				}
			}
			JSON_Object* jmetallicRoughnessTexture = json_object_get_object(jmr, "metallicRoughnessTexture");
			if (jmetallicRoughnessTexture) {
				int itex = (int) json_object_get_number(jmetallicRoughnessTexture, "index");
				JSON_Object* jtex = json_array_get_object(jtextures, itex);
				if (jtex && json_object_has_value(jtex, "source") && json_object_has_value(jtex, "sampler")) {
					SamplerBinding& rm = m.samplers[m.num_samplers++];
					rm.name = DefaultUniforms::TexOccRghMet.name;
					rm.texture = textures[uint32_t(json_object_get_number(jtex, "source"))];
					rm.sampler = samplers[uint32_t(json_object_get_number(jtex, "sampler"))];
					LOG_F(INFO, "-> material=%u %s gltex=%u", imat, rm.name, rm.texture->gl_texture);
				}
			}
		}
	}

	// Extract scene graph into a GameObject:
	// TODO: Support GLTF scenes
	JSON_Array* jnodes  = json_object_get_array(root, "nodes");
	model.root_object = new GameObject();
	auto objects = std::vector<GameObject*>(json_array_get_count(jnodes));
	// Parent all nodes to root object to begin with
	for (uint32_t inode = 0; inode < json_array_get_count(jnodes); inode++) {
		GameObject* node = objects[inode] = new GameObject();
		node->parent = model.root_object;
	}
	// Process nodes: reparent, set transforms
	for (uint32_t inode = 0; inode < json_array_get_count(jnodes); inode++) {
		GameObject* node = objects[inode];
		JSON_Object* jnode = json_array_get_object(jnodes, inode);

		JSON_Array* jchildren = json_object_get_array(jnode, "children");
		if (jchildren) {
			for (uint32_t ichild = 0; ichild < json_array_get_count(jchildren); ichild++) {
				objects[uint32_t(json_array_get_number(jchildren, ichild))]->parent = node;
			}
		}

		JSON_Array* jmat = json_object_get_array(jnode, "matrix");
		Transform& nt = node->MutLocal();
		if (jmat) {
			// We'll have to decompose this into translation, rotation and scale
			mat4 matrix;
			float* fmatrix = reinterpret_cast<float*>(&matrix); // FIXME: Surely glm has a helper for this?
			for (int i = 0; i < 16; i++) { fmatrix[i] = float(json_array_get_number(jmat, i)); }
			vec3 skew; vec4 perspective;
			glm::decompose(matrix, nt.scale, nt.rotation, nt.position, skew, perspective);
		} else {
			JSON_Array* jr = json_object_get_array(jnode, "rotation");
			JSON_Array* jt = json_object_get_array(jnode, "translation");
			JSON_Array* js = json_object_get_array(jnode, "scale");
			if (jr) { for (int i = 0; i < 4; i++) { nt.rotation[i] = float(json_array_get_number(jr, i)); } }
			if (jt) { for (int i = 0; i < 3; i++) { nt.position[i] = float(json_array_get_number(jt, i)); } }
			if (js) { for (int i = 0; i < 3; i++) { nt.scale[i]    = float(json_array_get_number(js, i)); } }
		}
	}

	// Count the number of meshes (GLTF primitives) we need to extract:
	JSON_Array* jmeshes = json_object_get_array(root, "meshes");
	uint32_t mesh_count = 0;
	for (uint32_t imesh = 0; imesh < json_array_get_count(jmeshes); imesh++) {
		JSON_Object* jmesh = json_array_get_object(jmeshes, imesh);
		JSON_Array* jprims = json_object_get_array(jmesh, "primitives");
		mesh_count += json_array_get_count(jprims);
	}

	// Extract meshes from the node structure:
	// Note that GLTF materials are attached to primitives, so a GLTF primitive actually corresponds
	// to our MeshInstance game-object. GLTF has no real equivalent to our Mesh object.
	auto meshes = std::vector<Mesh*>(mesh_count);
	uint32_t extracted_mesh_idx = 0;
	for (uint32_t inode = 0; inode < json_array_get_count(jnodes); inode++) {
		JSON_Object* jnode = json_array_get_object(jnodes, inode);
		if (!json_object_has_value(jnode, "mesh")) { continue; }

		uint32_t igltfmesh = uint32_t(json_object_get_number(jnode, "mesh"));
		JSON_Object* jmesh = json_array_get_object(jmeshes, igltfmesh);
		JSON_Array* jprims = json_object_get_array(jmesh, "primitives");

		for (uint32_t iprim = 0; iprim < json_array_get_count(jprims); iprim++) {
			JSON_Object* jprim = json_array_get_object(jprims, iprim);
			JSON_Object* jattr = json_object_get_object(jprim, "attributes");
			if (!jattr || !json_object_has_value(jprim, "material")) { continue; }

			// Create a Mesh and a MeshInstance object for this primitive.
			// TODO: Ideally we would detect when a primitive can use a pre-existing Mesh (if the
			// parameters and accessors are the same).
			meshes[extracted_mesh_idx] = new Mesh();
			Mesh& mesh = *meshes[extracted_mesh_idx++];
			MeshInstance* instance = static_cast<MeshInstance*>(objects.emplace_back(new MeshInstance()));
			instance->parent = objects[inode];
			instance->mesh = &mesh;

			uint32_t imat = uint32_t(json_object_get_number(jprim, "material"));
			instance->material = materials[imat];

			if (json_object_has_value(jprim, "mode") ){
				mesh.ptype = PrimitiveType::from_gl_enum(GLenum(json_object_get_number(jprim, "mode")));
			} else {
				mesh.ptype = PrimitiveType::TRIANGLES;
			}

			String debug_str = "";
			if (json_object_has_value(jprim, "indices")) {
				uint32_t ibv = uint32_t(json_object_get_number(jprim, "indices"));
				mesh.index_buffer = *buffer_views[ibv];
				if (buffer_views[ibv]->buffer->usage.v == BufferUsage::Vertex) {
					LOG_F(WARNING, "Mesh %u prim %u uses vertex buffer (acc=%u) for indices", igltfmesh, iprim, ibv);
				} else {
					buffer_views[ibv]->buffer->usage = BufferUsage::Index;
				}
				debug_str = String::format("INDEX(acc=%u)", ibv);
			}

			for (DefaultAttributes::Item attr : DefaultAttributes::all) {
				if (!json_object_has_value(jattr, attr.gltf_name)) { continue; }
				uint32_t ibv = json_object_get_number(jattr, attr.gltf_name);
				mesh.vertex_attribs[attr.index] = *buffer_views[ibv];
				if (buffer_views[ibv]->buffer->usage.v == BufferUsage::Index) {
					LOG_F(WARNING, "Mesh %u prim %u uses index buffer (acc=%u) for vertex data", igltfmesh, iprim, ibv);
				} else {
					buffer_views[ibv]->buffer->usage = BufferUsage::Vertex;
				}
				debug_str = String::format("%s %s(acc=%u)", debug_str.cstr, attr.gltf_name, ibv);
			}

			BufferView& position = mesh.vertex_attribs[DefaultAttributes::Position.index];
			if (position.buffer && position.etype.v == ElementType::VEC3 && position.ctype.v == ComponentType::F32) {
				vec3 aabb_min = { INFINITY,  INFINITY,  INFINITY};
				vec3 aabb_max = {-INFINITY, -INFINITY, -INFINITY};
				for (size_t i = 0; i < position.elements; i++) {
					uint8_t* p = &position.buffer->cpu_buffer[position.offset + 3 * i * sizeof(float)];
					vec3* vp = reinterpret_cast<vec3*>(p); // NOTE: needs -fno-strict-aliasing
					aabb_min = glm::min(aabb_min, *vp);
					aabb_max = glm::max(aabb_min, *vp);
				}
				mesh.aabb_center = (aabb_min + aabb_max) / vec3(2.0f);
				mesh.aabb_half_extents = aabb_max - mesh.aabb_center;
			}

			LOG_F(INFO, "-> mesh=%u prim=%u [%p] mat=%u %s %s", igltfmesh, iprim, &mesh, imat,
				mesh.ptype.name(), debug_str.cstr);
		}
	}

	// Debug output for node graph, now that we've added all of them
	LOG_F(INFO, "-> root object [%p]", model.root_object);
	for (uint32_t inode = 0; inode < objects.size(); inode++) {
		GameObject* node = objects[inode];
		const Transform& nt = node->GetLocal();
		String extra = "";
		if (node->type == GameObject::MESH_INSTANCE) {
			MeshInstance* instance = static_cast<MeshInstance*>(objects[inode]);
			extra = String::format(" mesh=[%p] material=[%p]", instance->mesh, instance->material);
		}
		LOG_F(INFO,
			"-> node=%u [%p] parent=[%p] pos=(%.02f %.02f %.02f) rot=(%.02f %.02f %.02f %.02f)%s",
			inode, node, node->parent, nt.position.x, nt.position.y, nt.position.z,
			nt.rotation.x, nt.rotation.y, nt.rotation.z, nt.rotation.w, extra.cstr);
	}

	// Upload buffers to the GPU now that we have usage info for them
	for (uint32_t ibuf = 0; ibuf < buffers.size(); ibuf++) {
		Buffer& buffer = *buffers[ibuf];
		GLenum target = buffer.usage.gl_target();
		glBindBuffer(target, buffer.gpu_handle);
		glBufferData(target, buffer.size, buffer.cpu_buffer, GL_STATIC_DRAW);
		glBindBuffer(target, 0);
	}

	// Set up GL vertex array object for each mesh and enable vertex attribute arrays
	for (uint32_t imesh = 0; imesh < meshes.size(); imesh++) {
		Mesh& mesh = *meshes[imesh];
		glGenVertexArrays(1, &mesh.gl_vertex_array);
		glBindVertexArray(mesh.gl_vertex_array);
		if (mesh.index_buffer.buffer) {
			Buffer& buffer = *mesh.index_buffer.buffer;
			glBindBuffer(buffer.usage.gl_target(), buffer.gpu_handle);
		}
		for (uint32_t iattr = 0; iattr < mesh.MaxVertexAttribs; iattr++) {
			if (mesh.vertex_attribs[iattr].buffer) {
				BufferView& bufview = mesh.vertex_attribs[iattr];
				Buffer& buffer = *bufview.buffer;
				glBindBuffer(buffer.usage.gl_target(), buffer.gpu_handle);
				GLuint location = DefaultAttributes::all[iattr].index;
				glEnableVertexAttribArray(location);
				glVertexAttribPointer(location, bufview.components(), bufview.ctype.gl_enum(), false,
					bufview.stride(), nullptr);
			}
		}
		glBindVertexArray(0);
	}

	model.buffers = std::move(buffers);
	model.textures = std::move(textures);
	model.samplers = std::move(samplers);
	model.materials = std::move(materials);
	model.meshes = std::move(meshes);
	model.objects = std::move(objects);

	json_value_free(rootval);

	uint64_t time_end = SDL_GetPerformanceCounter();
	LOG_F(INFO, "-> model %s loaded in %.03f ms", model.display_name.cstr,
		float(time_end - time_get_start) / ticks_per_msec);

	return &model;
}
