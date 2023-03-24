#include "graphics/render.hh"
#include "engine/engine.hh"
#include "scene/light.hh"
#include <unordered_map>

namespace DefaultRenderTargets {
	RenderTarget Diffuse    = RenderTarget(ImageFormat::RGB8, &DefaultUniforms::RTDiffuse);
	RenderTarget Normal     = RenderTarget(ImageFormat::RGB16, &DefaultUniforms::RTNormal);
	RenderTarget Material   = RenderTarget(ImageFormat::RGB8, &DefaultUniforms::RTMaterial);
	RenderTarget Velocity   = RenderTarget(ImageFormat::RG16, &DefaultUniforms::RTVelocity);
	RenderTarget ColorHDR   = RenderTarget(ImageFormat::RG11B10, &DefaultUniforms::RTColorHDR);
	RenderTarget PersistTAA = RenderTarget(ImageFormat::RG11B10, &DefaultUniforms::RTColorHDR);
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
		RebuildRenderTarget(DefaultRenderTargets::Diffuse,    engine.display_w, engine.display_h);
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
		uint32_t next_color_attachment = 0;
		for (RenderTarget* attachment : attachments) {
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

void Render(const Engine& engine, RenderList& rlist, Camera* camera, Program* program, Framebuffer* framebuffer) {
	BindFramebuffer(framebuffer);
	glUseProgram(program->gl_program);

	program->uniform(DefaultUniforms::FramebufferSize, vec2(engine.display_w, engine.display_h));

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
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(GL_TEXTURE_2D, mat.samplers[i].texture->gl_texture);
				glBindSampler(i, mat.samplers[i].sampler->gl_sampler);
				program->uniform(mat.samplers[i].uniform, i);
			}

			for (uint32_t i = 0; i < mat.num_uniforms; i++) {
				UniformValue& u = mat.uniforms[i];
				// FIXME: The model loader only outputs vec4 and f32, so that's all we need to implement for now
				if (u.etype.v == ElementType::VEC4 && u.ctype.v == ComponentType::F32) {
					program->uniform(u.uniform, u.vec4.f32);
				} else if (u.etype.v == ElementType::SCALAR && u.ctype.v == ComponentType::F32) {
					program->uniform(u.uniform, u.scalar.f32);
				}
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

void* StartRenderPass(const char* name) {
	GLPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, name);
	return nullptr;
}

void EndRenderPass(void* render_pass_handle) {
	(void)(render_pass_handle); // unused
	GLPopDebugGroup();
}
