#pragma once
#include "base/base.hh"
#include "graphics/opengl.hh"
#include "graphics/defaults.hh"
#include "graphics/formats.hh"
#include "graphics/renderlist.hh"
#include "assets/material.hh"
#include "assets/model.hh"
#include "assets/shader.hh"
#include "scene/camera.hh"

struct Engine;
struct DirectionalLight;

struct RenderTarget {
	const ImageFormat format;
	const DefaultUniforms::Item* uniform;
	GLuint gl_texture;
	RenderTarget(ImageFormat format, const DefaultUniforms::Item* uniform):
		format{format}, uniform{uniform}, gl_texture{0} {}
};

namespace DefaultRenderTargets {
	extern RenderTarget Albedo;
	extern RenderTarget Normal;
	extern RenderTarget Material;
	extern RenderTarget Velocity;
	extern RenderTarget ColorHDR;
	extern RenderTarget PersistTAA;
	extern RenderTarget Depth;
	extern RenderTarget ShadowMap;
};

struct Framebuffer {
	static constexpr uint32_t MaxAttachments = 8;
	RenderTarget* attachments [MaxAttachments] = {};
	GLuint gl_framebuffer = 0;
	uint32_t gl_drawbuffer_count = 0;
	GLuint gl_drawbuffers [MaxAttachments] = {};
};

void UpdateRenderTargets(const Engine& engine);
void UpdateShadowRenderTargets(const DirectionalLight& light);

Framebuffer* GetFramebuffer(std::initializer_list<RenderTarget*> attachments);
void BindFramebuffer(Framebuffer* framebuffer);

namespace RenderFlags {
	typedef uint32_t Flag;
	static constexpr Flag UseOriginalAlbedo = (1 << 0);
	static constexpr Flag UseOriginalStippleParams = (1 << 1);
};

void Render(Engine& engine, RenderList& rlist, Camera* camera, Program* program, Framebuffer* input,
	Framebuffer* output, std::initializer_list<UniformValue> uniforms, Material* override_material = nullptr,
	RenderFlags::Flag flags = 0);

namespace RenderEffectFlags {
	typedef uint32_t Flag;
	static constexpr Flag BlendAdditive = (1 << 0);
};

void RenderEffect(Engine& engine, FragShader* fsh, Framebuffer* input, Framebuffer* output,
	std::initializer_list<UniformValue> uniforms, RenderEffectFlags::Flag flags = 0);

void* StartRenderPass(const char* name);
void EndRenderPass(void* render_pass_handle);

template <typename F> static void RenderPass(const char* name, F function) {
	void* render_pass_handle = StartRenderPass(name);
	function();
	EndRenderPass(render_pass_handle);
}
