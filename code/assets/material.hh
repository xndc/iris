#pragma once
#include "graphics/opengl.hh"
#include "assets/texture.hh"
#include "assets/mesh.hh"
#include "assets/shader.hh"

enum class MaterialType : uint8_t {
	// The material writes albedo, normal and occlusion/roughness/metallic values to a G-Buffer.
	// Used for opaque surfaces following the PBR metallic-roughness model.
	GeometryDeferredORM,
};

enum class BlendMode : uint8_t {
	// The material will be 100% opaque at every point on any surface that uses it.
	Opaque,
	// The material will be rendered with stippling, i.e. fragments with alpha values between two
	// thresholds will be discarded based on a dithering mask.
	Stippled,
	// The material will be rendered with GPU blending.
	// TODO: This feature is unfinished. We need blending for things like light accumulation, so we
	// need a way to enable it and set all the factors, but for actual transparency we'd need a
	// separate render pass with forward lighting and sorted back-to-front rendering.
	Transparent,
};

struct SamplerBinding {
	Uniforms::Item uniform = Uniforms::all[0];
	Texture* texture = nullptr;
	Sampler* sampler = nullptr;
};

struct Material {
	MaterialType type = MaterialType::GeometryDeferredORM;
	BlendMode blend_mode = BlendMode::Opaque;

	// Blend source factor. This is a term in the blending equation corresponding to this material's
	// colour output for each fragment. For standard A-over-B blending with back-to-front
	// transparency, use GL_SRC_ALPHA for the colour component and GL_ONE for alpha.
	GLenum blend_srcf_color = GL_SRC_ALPHA;
	GLenum blend_srcf_alpha = GL_ONE;
	// Blend destination factor. This is a term in the blending equation corresponding to the colour
	// value already in the framebuffer before blending happens for a particular fragment. For
	// standard A-over-B blending with back-to-front transparency, use GL_ONE_MINUS_SRC_ALPHA for
	// the colour component and GL_ZERO for alpha.
	GLenum blend_dstf_color = GL_ONE_MINUS_SRC_ALPHA;
	GLenum blend_dstf_alpha = GL_ZERO;
	// Blending operator, used to combine the source and destination factors.
	GLenum blend_op_color = GL_FUNC_ADD;
	GLenum blend_op_alpha = GL_FUNC_ADD;

	// Parameter for stippling. Below this alpha value, the pixel is not rendered at all.
	// This is called alphaCutoff in GLTF, and the default is 0.5 there.
	float stipple_hard_cutoff = 0.5f;
	// Parameter for stippling. Above this alpha value, the pixel is always rendered.
	// This doesn't exist in GLTF. For GLTF models, this should be the same as the hard cutoff.
	float stipple_soft_cutoff = 0.5f;

	// Whether to cull back faces, front faces or neither (GL_NONE) for triangles. The default
	// winding order is counter-clockwise, i.e. GL_BACK will cull clockwise winded faces.
	GLenum face_culling_mode = GL_BACK;

	// Function to use for depth testing. Fragments will pass the depth test and be rendered if
	// [current-fragment-depth] [depth-test-func] [framebuffer-fragment-depth]. Since we use a
	// reversed Z-buffer, where Z=1 is near and Z=0 is far, GL_GREATER is a suitable default.
	GLenum depth_test_func = GL_GREATER;

	// Should fragments rendered with this material be depth-tested at all?
	bool depth_test : 1 = true;

	// Should rendering with this material write to the Z-buffer, assuming depth testing is enabled?
	// We probably want this to be disabled for light volumes and transparent objects.
	bool depth_write : 1 = true;

	constexpr static size_t MaxUniforms = 16;
	UniformValue uniforms [MaxUniforms] = {};
	uint32_t num_uniforms = 0;

	constexpr static size_t MaxSamplers = 16;
	SamplerBinding samplers [MaxSamplers] = {};
	uint32_t num_samplers = 0;
};
