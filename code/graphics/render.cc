#include "graphics/render.hh"
#include "engine/engine.hh"
#include "scene/light.hh"
#include <unordered_map>

// NOTE: Must use formats that are colour-renderable on WebGL2 / GLES 3.0
namespace DefaultRenderTargets {
	RenderTarget Albedo     = RenderTarget(ImageFormat::RGB8, &DefaultUniforms::RTAlbedo);
	RenderTarget Normal     = RenderTarget(ImageFormat::RGB8, &DefaultUniforms::RTNormal);
	RenderTarget Material   = RenderTarget(ImageFormat::RGB8, &DefaultUniforms::RTMaterial);
	RenderTarget Velocity   = RenderTarget(ImageFormat::RG8, &DefaultUniforms::RTVelocity);
	RenderTarget ColorHDR   = RenderTarget(ImageFormat::RGB10A2, &DefaultUniforms::RTColorHDR);
	RenderTarget PersistTAA = RenderTarget(ImageFormat::RGB10A2, &DefaultUniforms::RTColorHDR);
	RenderTarget Depth      = RenderTarget(ImageFormat::D32, &DefaultUniforms::RTDepth);
	RenderTarget ShadowMap  = RenderTarget(ImageFormat::D32, &DefaultUniforms::ShadowMap);
};

struct FramebufferKey {
	RenderTarget* attachments [Framebuffer::MaxAttachments];
	constexpr bool operator==(const FramebufferKey& rhs) const {
		for (uint32_t i = 0; i < CountOf(attachments); i++) {
			if (attachments[i] != rhs.attachments[i]) return false;
		}
		return true;
	}
	constexpr bool operator!=(const FramebufferKey& rhs) const { return !(*this == rhs); }
};
std::unordered_map<FramebufferKey, Framebuffer, Hash64T> FramebufferCache;

static void ClearFramebufferCache() {
	for (auto& [key, framebuffer] : FramebufferCache) {
		glDeleteFramebuffers(1, &framebuffer.gl_framebuffer);
		framebuffer.gl_framebuffer = 0;
	}
}

static void RebuildRenderTarget(RenderTarget& target, uint32_t w, uint32_t h) {
	if (target.gl_texture != 0) {
		glDeleteTextures(1, &target.gl_texture);
	}
	glGenTextures(1, &target.gl_texture);
	glBindTexture(GL_TEXTURE_2D, target.gl_texture);
	glTexStorage2D(GL_TEXTURE_2D, 1, target.format.gl_internalformat(), w, h);
	glBindTexture(GL_TEXTURE_2D, 0);
	if (target.uniform) { GLObjectLabel(GL_TEXTURE, target.gl_texture, target.uniform->name); }
}

void UpdateRenderTargets(const Engine& engine) {
	static uint32_t last_w = 0, last_h = 0;
	if (engine.display_w != last_w || engine.display_h != last_h) {
		last_w = engine.display_w;
		last_h = engine.display_h;
		RebuildRenderTarget(DefaultRenderTargets::Albedo,     engine.display_w, engine.display_h);
		RebuildRenderTarget(DefaultRenderTargets::Normal,     engine.display_w, engine.display_h);
		RebuildRenderTarget(DefaultRenderTargets::Material,   engine.display_w, engine.display_h);
		RebuildRenderTarget(DefaultRenderTargets::Velocity,   engine.display_w, engine.display_h);
		RebuildRenderTarget(DefaultRenderTargets::ColorHDR,   engine.display_w, engine.display_h);
		RebuildRenderTarget(DefaultRenderTargets::PersistTAA, engine.display_w, engine.display_h);
		RebuildRenderTarget(DefaultRenderTargets::Depth,      engine.display_w, engine.display_h);
		ClearFramebufferCache();
	}
}

void UpdateShadowRenderTargets(const DirectionalLight& light) {
	static uint32_t last_shadowmap_size = 0;
	if (light.shadowmap_size != last_shadowmap_size) {
		last_shadowmap_size = light.shadowmap_size;
		RebuildRenderTarget(DefaultRenderTargets::ShadowMap, light.shadowmap_size, light.shadowmap_size);
		ClearFramebufferCache();
	}
}

Framebuffer* GetFramebuffer(std::initializer_list<RenderTarget*> attachments) {
	FramebufferKey key = {};
	uint32_t num_attachments = 0;
	for (RenderTarget* attachment : attachments) {
		CHECK_LT_F(num_attachments, Framebuffer::MaxAttachments);
		key.attachments[num_attachments++] = attachment;
	}
	Framebuffer& framebuffer = FramebufferCache[key];
	if (framebuffer.gl_framebuffer == 0) {
		glGenFramebuffers(1, &framebuffer.gl_framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer.gl_framebuffer);
		uint32_t next_color_attachment = 0, next_attachment = 0;
		for (RenderTarget* attachment : attachments) {
			framebuffer.attachments[next_attachment++] = attachment;
			GLenum ap = attachment->format.gl_framebuffer_base_attachment();
			if (ap == GL_COLOR_ATTACHMENT0) {
				ap += next_color_attachment;
				framebuffer.gl_drawbuffers[next_color_attachment] = ap;
				next_color_attachment++;
			}
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, ap, GL_TEXTURE_2D, attachment->gl_texture, 0);
		}
		framebuffer.gl_drawbuffer_count = next_color_attachment;
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
	return &framebuffer;
}

void BindFramebuffer(Framebuffer* framebuffer) {
	if (framebuffer) {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer->gl_framebuffer);
		glDrawBuffers(framebuffer->gl_drawbuffer_count, framebuffer->gl_drawbuffers);
	} else {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	}
}

static uint32_t SetCoreUniforms(const Engine& engine, Program* program, Framebuffer* input) {
	uint32_t next_texture_unit = 0;

	program->uniform(DefaultUniforms::FramebufferSize, vec2(engine.display_w, engine.display_h));

	if (input) {
		for (RenderTarget* rt : input->attachments) {
			if (!rt) { continue; }
			glActiveTexture(GL_TEXTURE0 + next_texture_unit);
			glBindTexture(GL_TEXTURE_2D, rt->gl_texture);
			glBindSampler(next_texture_unit, DefaultSamplers::NearestRepeat.gl_sampler);
			program->uniform(*(rt->uniform), next_texture_unit);
			next_texture_unit++;
		}
	}

	return next_texture_unit;
}

static void SetUniformValue(Program& program, UniformValue& u) {
	using namespace glm;
	constexpr ComponentType::Enum U8  = ComponentType::U8;
	constexpr ComponentType::Enum I8  = ComponentType::I8;
	constexpr ComponentType::Enum U16 = ComponentType::U16;
	constexpr ComponentType::Enum I16 = ComponentType::I16;
	constexpr ComponentType::Enum U32 = ComponentType::U32;
	constexpr ComponentType::Enum I32 = ComponentType::I32;
	constexpr ComponentType::Enum F32 = ComponentType::F32;

	GLint loc = program.location(u.uniform);
	switch (u.etype.v) {
		case ElementType::SCALAR: switch (u.ctype.v) {
			case U8:  glUniform1ui(loc, u.scalar.u8);  break;
			case I8:  glUniform1i (loc, u.scalar.i8);  break;
			case U16: glUniform1ui(loc, u.scalar.u16); break;
			case I16: glUniform1i (loc, u.scalar.i16); break;
			case U32: glUniform1ui(loc, u.scalar.u32); break;
			case I32: glUniform1i (loc, u.scalar.i32); break;
			case F32: glUniform1f (loc, u.scalar.f32); break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::VEC2: switch (u.ctype.v) {
			case U8:  { uvec2 v = u.vec2.u8;  glUniform2uiv(loc, 1, (uint *)(&v)); } break;
			case I8:  { ivec2 v = u.vec2.i8;  glUniform2iv (loc, 1, (int  *)(&v)); } break;
			case U16: { uvec2 v = u.vec2.u16; glUniform2uiv(loc, 1, (uint *)(&v)); } break;
			case I16: { ivec2 v = u.vec2.i16; glUniform2iv (loc, 1, (int  *)(&v)); } break;
			case U32: { uvec2 v = u.vec2.u32; glUniform2uiv(loc, 1, (uint *)(&v)); } break;
			case I32: { ivec2 v = u.vec2.i32; glUniform2iv (loc, 1, (int  *)(&v)); } break;
			case F32: {  vec2 v = u.vec2.f32; glUniform2fv (loc, 1, (float*)(&v)); } break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::VEC3: switch (u.ctype.v) {
			case U8:  { uvec3 v = u.vec3.u8;  glUniform3uiv(loc, 1, (uint *)(&v)); } break;
			case I8:  { ivec3 v = u.vec3.i8;  glUniform3iv (loc, 1, (int  *)(&v)); } break;
			case U16: { uvec3 v = u.vec3.u16; glUniform3uiv(loc, 1, (uint *)(&v)); } break;
			case I16: { ivec3 v = u.vec3.i16; glUniform3iv (loc, 1, (int  *)(&v)); } break;
			case U32: { uvec3 v = u.vec3.u32; glUniform3uiv(loc, 1, (uint *)(&v)); } break;
			case I32: { ivec3 v = u.vec3.i32; glUniform3iv (loc, 1, (int  *)(&v)); } break;
			case F32: {  vec3 v = u.vec3.f32; glUniform3fv (loc, 1, (float*)(&v)); } break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::VEC4: switch (u.ctype.v) {
			case U8:  { uvec4 v = u.vec4.u8;  glUniform4uiv(loc, 1, (uint *)(&v)); } break;
			case I8:  { ivec4 v = u.vec4.i8;  glUniform4iv (loc, 1, (int  *)(&v)); } break;
			case U16: { uvec4 v = u.vec4.u16; glUniform4uiv(loc, 1, (uint *)(&v)); } break;
			case I16: { ivec4 v = u.vec4.i16; glUniform4iv (loc, 1, (int  *)(&v)); } break;
			case U32: { uvec4 v = u.vec4.u32; glUniform4uiv(loc, 1, (uint *)(&v)); } break;
			case I32: { ivec4 v = u.vec4.i32; glUniform4iv (loc, 1, (int  *)(&v)); } break;
			case F32: {  vec4 v = u.vec4.f32; glUniform4fv (loc, 1, (float*)(&v)); } break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::MAT2X2: switch (u.ctype.v) {
			case U8:  { mat2 v = u.mat2x2.u8;  glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case I8:  { mat2 v = u.mat2x2.i8;  glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case U16: { mat2 v = u.mat2x2.u16; glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case I16: { mat2 v = u.mat2x2.i16; glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case U32: { mat2 v = u.mat2x2.u32; glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case I32: { mat2 v = u.mat2x2.i32; glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case F32: { mat2 v = u.mat2x2.f32; glUniformMatrix2fv(loc, 1, 0, (float*)(&v)); } break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::MAT3X3: switch (u.ctype.v) {
			case U8:  { mat3 v = u.mat3x3.u8;  glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case I8:  { mat3 v = u.mat3x3.i8;  glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case U16: { mat3 v = u.mat3x3.u16; glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case I16: { mat3 v = u.mat3x3.i16; glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case U32: { mat3 v = u.mat3x3.u32; glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case I32: { mat3 v = u.mat3x3.i32; glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case F32: { mat3 v = u.mat3x3.f32; glUniformMatrix3fv(loc, 1, 0, (float*)(&v)); } break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::MAT4X4: switch (u.ctype.v) {
			case U8:  { mat4 v = u.mat4x4.u8;  glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case I8:  { mat4 v = u.mat4x4.i8;  glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case U16: { mat4 v = u.mat4x4.u16; glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case I16: { mat4 v = u.mat4x4.i16; glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case U32: { mat4 v = u.mat4x4.u32; glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case I32: { mat4 v = u.mat4x4.i32; glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case F32: { mat4 v = u.mat4x4.f32; glUniformMatrix4fv(loc, 1, 0, (float*)(&v)); } break;
			case ComponentType::Count: Unreachable();
		} break;
		case ElementType::Count: Unreachable();
	}
}

void Render(const Engine& engine, RenderList& rlist, Camera* camera, Program* program,
	Framebuffer* input, Framebuffer* output, std::initializer_list<UniformValue> uniforms)
{
	BindFramebuffer(output);
	glUseProgram(program->gl_program);

	uint32_t next_texture_unit = SetCoreUniforms(engine, program, input);
	for (UniformValue u : uniforms) {
		SetUniformValue(*program, u);
	}

	// Find per-view render list for this camera
	auto viewlist_iter = std::find_if(rlist.views.cbegin(), rlist.views.cend(),
		[&](const RenderListPerView& v) { return v.camera == camera; });
	CHECK_NE_F(viewlist_iter, rlist.views.end());
	const RenderListPerView& viewlist = *viewlist_iter;

	Material* last_material = nullptr;

	for (const auto& [key, rmesh] : viewlist.meshes) {
		Mesh& mesh = *rmesh.mesh;
		Material& mat = *rmesh.material;

		if (&mat != last_material) {
			if (mat.face_culling_mode != GL_NONE) {
				glEnable(GL_CULL_FACE);
				glCullFace(mat.face_culling_mode);
			} else {
				glDisable(GL_CULL_FACE);
			}

			if (mat.depth_test) {
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(mat.depth_test_func);
				glDepthMask(mat.depth_write);
			} else {
				glDisable(GL_DEPTH_TEST);
			}

			if (mat.blend_mode == BlendMode::Transparent) {
				glEnable(GL_BLEND);
				glBlendFuncSeparate(mat.blend_srcf_color, mat.blend_dstf_color,
					mat.blend_srcf_alpha, mat.blend_dstf_alpha);
				glBlendEquationSeparate(mat.blend_op_color, mat.blend_op_alpha);
			} else {
				// TODO: Add uniforms for stipple cutoffs
				glDisable(GL_BLEND);
			}

			for (uint32_t i = 0; i < mat.num_samplers; i++) {
				glActiveTexture(GL_TEXTURE0 + next_texture_unit);
				glBindTexture(GL_TEXTURE_2D, mat.samplers[next_texture_unit].texture->gl_texture);
				glBindSampler(next_texture_unit, mat.samplers[next_texture_unit].sampler->gl_sampler);
				program->uniform(mat.samplers[next_texture_unit].uniform, next_texture_unit);
				next_texture_unit++;
			}

			for (uint32_t i = 0; i < mat.num_uniforms; i++) {
				SetUniformValue(*program, mat.uniforms[i]);
			}

			last_material = &mat;
		}

		glBindVertexArray(mesh.gl_vertex_array);

		// TODO: Use instancing. Changes required:
		// 1. Stop wiping out RenderListPerView every frame
		// 2. Keep track of a GL uniform buffer object in RenderableMesh
		// 3. Upload the contents of RenderListPerView::mesh_instances to the buffer when needed
		// 4. Create a vertex shader that can pull data from it based on instance ID and a uniform

		for (uint32_t i = rmesh.first_instance; i < rmesh.first_instance + rmesh.instance_count; i++) {
			const RenderableMeshInstanceData& rmid = viewlist.mesh_instances[i];

			glUniformMatrix4fv(program->location(DefaultUniforms::LocalToWorld),
				1, false, reinterpret_cast<const float*>(&rmid.local_to_world));
			glUniformMatrix4fv(program->location(DefaultUniforms::LocalToClip),
				1, false, reinterpret_cast<const float*>(&rmid.local_to_clip));
			glUniformMatrix4fv(program->location(DefaultUniforms::LastLocalToClip),
				1, false, reinterpret_cast<const float*>(&rmid.last_local_to_clip));

			glDrawElements(mesh.ptype.gl_enum(), mesh.index_buffer.total_components(),
				mesh.index_buffer.ctype.gl_enum(), nullptr);
		}
	}

	for (uint32_t i = 0; i < Material::MaxSamplers; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindSampler(i, 0);
	}

	glBindVertexArray(0);
}

void RenderEffect(const Engine& engine, FragShader* fsh, Framebuffer* input, Framebuffer* output,
	std::initializer_list<UniformValue> uniforms)
{
	BindFramebuffer(output);

	VertShader* vsh = GetVertShader("data/shaders/core_fullscreen.vert");
	Program* program = GetProgram(vsh, fsh);
	glUseProgram(program->gl_program);

	SetCoreUniforms(engine, program, input);
	for (UniformValue u : uniforms) {
		SetUniformValue(*program, u);
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	glBindVertexArray(DefaultMeshes::QuadXZ.gl_vertex_array);
	glDrawElements(DefaultMeshes::QuadXZ.ptype.gl_enum(), DefaultMeshes::QuadXZ.index_buffer.total_components(),
		DefaultMeshes::QuadXZ.index_buffer.ctype.gl_enum(), nullptr);
}

void* StartRenderPass(const char* name) {
	GLPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, name);
	return nullptr;
}

void EndRenderPass(void* render_pass_handle) {
	(void)(render_pass_handle); // unused
	GLPopDebugGroup();
}
