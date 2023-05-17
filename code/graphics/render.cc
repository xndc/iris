#include "graphics/render.hh"
#include "engine/engine.hh"
#include "scene/light.hh"
#include <unordered_map>

// NOTE: Must use formats that are colour-renderable on WebGL2 / GLES 3.0
namespace RenderTargets {
	RenderTarget Albedo     = RenderTarget(ImageFormat::RGB8,    &Uniforms::RTAlbedo);
	// World-space normal vectors encoded with octahedral encoding, see gbuffer.frag
	RenderTarget Normal     = RenderTarget(ImageFormat::RG8,     &Uniforms::RTNormal);
	RenderTarget Material   = RenderTarget(ImageFormat::RGB8,    &Uniforms::RTMaterial);
	RenderTarget Velocity   = RenderTarget(ImageFormat::RG8,     &Uniforms::RTVelocity);
	RenderTarget ColorHDR   = RenderTarget(ImageFormat::RGB10A2, &Uniforms::RTColorHDR);
	RenderTarget PersistTAA = RenderTarget(ImageFormat::RGB10A2, &Uniforms::RTPersistTAA);
	RenderTarget Depth      = RenderTarget(ImageFormat::D32,     &Uniforms::RTDepth);
	RenderTarget ShadowMap  = RenderTarget(ImageFormat::D32,     &Uniforms::ShadowMap);
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
		RebuildRenderTarget(RenderTargets::Albedo,     engine.display_w, engine.display_h);
		RebuildRenderTarget(RenderTargets::Normal,     engine.display_w, engine.display_h);
		RebuildRenderTarget(RenderTargets::Material,   engine.display_w, engine.display_h);
		RebuildRenderTarget(RenderTargets::Velocity,   engine.display_w, engine.display_h);
		RebuildRenderTarget(RenderTargets::ColorHDR,   engine.display_w, engine.display_h);
		RebuildRenderTarget(RenderTargets::PersistTAA, engine.display_w, engine.display_h);
		RebuildRenderTarget(RenderTargets::Depth,      engine.display_w, engine.display_h);
		ClearFramebufferCache();
	}
}

void UpdateShadowRenderTargets(const DirectionalLight& light) {
	static uint32_t last_shadowmap_size = 0;
	if (light.shadowmap_size != last_shadowmap_size) {
		last_shadowmap_size = light.shadowmap_size;
		RebuildRenderTarget(RenderTargets::ShadowMap, light.shadowmap_size, light.shadowmap_size);
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

	program->set({Uniforms::FramebufferSize, vec2(engine.display_w, engine.display_h)});
	program->set({Uniforms::Time, engine.this_frame.t});

	if (input) {
		for (RenderTarget* rt : input->attachments) {
			if (!rt) { continue; }
			glActiveTexture(GL_TEXTURE0 + next_texture_unit);
			glBindTexture(GL_TEXTURE_2D, rt->gl_texture);
			glBindSampler(next_texture_unit, Samplers::NearestRepeat.gl_sampler);
			program->set({*(rt->uniform), int32_t(next_texture_unit)});
			next_texture_unit++;
		}
	}

	return next_texture_unit;
}

void Render(Engine& engine, RenderList& rlist, Camera* camera, Program* program, Framebuffer* input,
	Framebuffer* output, std::initializer_list<UniformValue> uniforms, Material* override_material,
	RenderFlags::Flag flags)
{
	BindFramebuffer(output);
	glUseProgram(program->gl_program);

	uint32_t first_texture_unit = SetCoreUniforms(engine, program, input);
	for (UniformValue u : uniforms) { program->set(u); }

	program->set({Uniforms::CameraPosition, camera->world_position});
	program->set({Uniforms::ClipToWorld, camera->this_frame.inv_vp});

	// Find per-view render list for this camera
	auto viewlist_iter = std::find_if(rlist.views.cbegin(), rlist.views.cend(),
		[&](const RenderListPerView& v) { return v.camera == camera; });
	CHECK_NE_F(viewlist_iter, rlist.views.end());
	const RenderListPerView& viewlist = *viewlist_iter;

	Material* last_material = nullptr;
	uint32_t next_texture_unit = 0;
	uint32_t num_drawcalls = 0, num_polys_rendered = 0;

	for (const auto& [key, rmesh] : viewlist.meshes) {
		Mesh& mesh = *rmesh.mesh;

		// Set material parameters, either from the MeshInstance's material or from the override
		// material. Render flags might require that some parameters remain unset here.
		if (override_material ? (num_drawcalls == 0) : (rmesh.material != last_material)) {
			Material& mat = override_material ? *override_material : *rmesh.material;
			next_texture_unit = first_texture_unit;

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
				glDisable(GL_BLEND);
				if (mat.blend_mode == BlendMode::Stippled) {
					program->set({Uniforms::StippleHardCutoff, mat.stipple_hard_cutoff});
					program->set({Uniforms::StippleSoftCutoff, mat.stipple_soft_cutoff});
				} else {
					program->set({Uniforms::StippleHardCutoff, 1.0f});
					program->set({Uniforms::StippleSoftCutoff, 1.0f});
				}
			}

			for (uint32_t i = 0; i < mat.num_samplers; i++) {
				// Don't waste a texture unit on the albedo sampler if we're going to overwrite it
				bool is_albedo = mat.samplers[i].uniform.hash == Uniforms::TexAlbedo.hash;
				if (is_albedo && (flags & RenderFlags::UseOriginalAlbedo)) { continue; }
				glActiveTexture(GL_TEXTURE0 + next_texture_unit);
				glBindTexture(GL_TEXTURE_2D, mat.samplers[i].texture->gl_texture);
				glBindSampler(next_texture_unit, mat.samplers[i].sampler->gl_sampler);
				program->set({mat.samplers[i].uniform, int32_t(next_texture_unit)});
				next_texture_unit++;
			}

			for (uint32_t i = 0; i < mat.num_uniforms; i++) {
				program->set(mat.uniforms[i]);
			}
		}

		// Override the override material with the instance material if requested via flags. Needed
		// for shadow passes. Yes, I know, but I can't think of better options.
		if (override_material && rmesh.material != last_material) {
			Material& mat = *rmesh.material;

			if (mat.blend_mode == BlendMode::Stippled && (flags & RenderFlags::UseOriginalStippleParams)) {
				program->set({Uniforms::StippleHardCutoff, mat.stipple_hard_cutoff});
				program->set({Uniforms::StippleSoftCutoff, mat.stipple_soft_cutoff});
			}

			if (flags & RenderFlags::UseOriginalAlbedo) {
				for (uint32_t i = 0; i < mat.num_samplers; i++) {
					bool is_albedo = mat.samplers[i].uniform.hash == Uniforms::TexAlbedo.hash;
					if (is_albedo) {
						glActiveTexture(GL_TEXTURE0 + next_texture_unit);
						glBindTexture(GL_TEXTURE_2D, mat.samplers[i].texture->gl_texture);
						glBindSampler(next_texture_unit, mat.samplers[i].sampler->gl_sampler);
						program->set({mat.samplers[i].uniform, int32_t(next_texture_unit)});
						next_texture_unit++;
					}
				}
			}

			if (flags & RenderFlags::UseOriginalAlbedo) {
				for (uint32_t i = 0; i < mat.num_uniforms; i++) {
					bool is_albedo = mat.uniforms[i].uniform.hash == Uniforms::ConstAlbedo.hash;
					if (is_albedo) {
						program->set(mat.uniforms[i]);
					}
				}
			}
		}

		last_material = rmesh.material;

		glBindVertexArray(mesh.gl_vertex_array);

		// TODO: Use instancing. Changes required:
		// 1. Stop wiping out RenderListPerView every frame
		// 2. Keep track of a GL uniform buffer object in RenderableMesh
		// 3. Upload the contents of RenderListPerView::mesh_instances to the buffer when needed
		// 4. Create a vertex shader that can pull data from it based on instance ID and a uniform

		for (uint32_t i = rmesh.first_instance; i < rmesh.first_instance + rmesh.instance_count; i++) {
			const RenderableMeshInstanceData& rmid = viewlist.mesh_instances[i];

			glUniformMatrix4fv(program->location(Uniforms::LocalToWorld),
				1, false, reinterpret_cast<const float*>(&rmid.local_to_world));
			glUniformMatrix4fv(program->location(Uniforms::LocalToClip),
				1, false, reinterpret_cast<const float*>(&rmid.local_to_clip));
			glUniformMatrix4fv(program->location(Uniforms::LastLocalToClip),
				1, false, reinterpret_cast<const float*>(&rmid.last_local_to_clip));

			glDrawElements(mesh.ptype.gl_enum(), mesh.index_buffer.total_components(),
				mesh.index_buffer.ctype.gl_enum(), nullptr);

			num_drawcalls += 1;
			num_polys_rendered += mesh.index_buffer.total_components() / mesh.ptype.vertices();
		}
	}

	for (uint32_t i = 0; i < Material::MaxSamplers; i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindSampler(i, 0);
	}

	glBindVertexArray(0);

	engine.this_frame.total_drawcalls += num_drawcalls;
	engine.this_frame.total_polys_rendered += num_polys_rendered;
}

void RenderEffect(Engine& engine, FragShader* fsh, Framebuffer* input, Framebuffer* output,
	std::initializer_list<UniformValue> uniforms, RenderEffectFlags::Flag flags)
{
	BindFramebuffer(output);

	VertShader* vsh = GetVertShader("data/shaders/core_fullscreen.vert");
	Program* program = GetProgram(vsh, fsh);
	glUseProgram(program->gl_program);

	SetCoreUniforms(engine, program, input);
	for (UniformValue u : uniforms) { program->set(u); }

	// TODO: Will we ever want to use RenderEffect for something other than the main camera?
	program->set({Uniforms::CameraPosition, engine.cam_main->world_position});
	program->set({Uniforms::ClipToWorld, engine.cam_main->this_frame.inv_vp});

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	if (flags & RenderEffectFlags::BlendAdditive) {
		glEnable(GL_BLEND);
		glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
	} else {
		glDisable(GL_BLEND);
	}

	glBindVertexArray(Meshes::QuadXZ.gl_vertex_array);
	glDrawElements(Meshes::QuadXZ.ptype.gl_enum(), Meshes::QuadXZ.index_buffer.total_components(),
		Meshes::QuadXZ.index_buffer.ctype.gl_enum(), nullptr);
}

void* StartRenderPass(const char* name) {
	GLPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, name);
	return nullptr;
}

void EndRenderPass(void* render_pass_handle) {
	(void)(render_pass_handle); // unused
	GLPopDebugGroup();
}
