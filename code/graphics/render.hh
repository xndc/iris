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
	extern RenderTarget Diffuse;
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

Framebuffer* GetFramebuffer(std::initializer_list<RenderTarget> attachments);
void BindFramebuffer(Framebuffer* framebuffer);

void Render(const Engine& engine, GameObject* scene, Camera* camera, Program* program, Framebuffer* framebuffer);