#pragma once
#include "graphics/opengl.hh"
#include "assets/texture.hh"
#include "assets/mesh.hh"

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

struct UniformValue {
	union {
		union {
			int8_t   i8;
			uint8_t  u8;
			int16_t  i16;
			uint16_t u16;
			int32_t  i32;
			uint32_t u32;
			float    f32;
		} scalar;
		union {
			glm::i8vec2  i8;
			glm::u8vec2  u8;
			glm::i16vec2 i16;
			glm::u16vec2 u16;
			glm::ivec2   i32;
			glm::uvec2   u32;
			glm::vec2    f32;
		} vec2;
		union {
			glm::i8vec3  i8;
			glm::u8vec3  u8;
			glm::i16vec3 i16;
			glm::u16vec3 u16;
			glm::ivec3   i32;
			glm::uvec3   u32;
			glm::vec3    f32;
		} vec3;
		union {
			glm::i8vec4  i8;
			glm::u8vec4  u8;
			glm::i16vec4 i16;
			glm::u16vec4 u16;
			glm::ivec4   i32;
			glm::uvec4   u32;
			glm::vec4    f32;
		} vec4;
		union {
			glm::i8mat2x2  i8;
			glm::u8mat2x2  u8;
			glm::i16mat2x2 i16;
			glm::u16mat2x2 u16;
			glm::imat2x2   i32;
			glm::umat2x2   u32;
			glm::mat2      f32;
		} mat2x2;
		union {
			glm::i8mat3x3  i8;
			glm::u8mat3x3  u8;
			glm::i16mat3x3 i16;
			glm::u16mat3x3 u16;
			glm::imat3x3   i32;
			glm::umat3x3   u32;
			glm::mat3      f32;
		} mat3x3;
		union {
			glm::i8mat4x4  i8;
			glm::u8mat4x4  u8;
			glm::i16mat4x4 i16;
			glm::u16mat4x4 u16;
			glm::imat4x4   i32;
			glm::umat4x4   u32;
			glm::mat4      f32;
		} mat4x4;
	};
	DefaultUniforms::Item uniform = DefaultUniforms::all[0];
	ElementType etype;
	ComponentType ctype;

	UniformValue() = default;

	#define CONSTRUCTOR(etype_name, ctype_name, short_etype, short_ctype, type) \
		UniformValue(DefaultUniforms::Item uniform, type short_etype##_##short_ctype): \
			uniform{uniform}, etype{etype_name}, ctype{ctype_name} \
			{ short_etype.short_ctype = short_etype##_##short_ctype; }

	CONSTRUCTOR(ElementType::SCALAR, ComponentType::F32, scalar, f32, float)
	CONSTRUCTOR(ElementType::SCALAR, ComponentType::I32, scalar, i32, int32_t)
	CONSTRUCTOR(ElementType::VEC2,   ComponentType::F32, vec2,   f32, glm::vec2)
	CONSTRUCTOR(ElementType::VEC3,   ComponentType::F32, vec3,   f32, glm::vec3)
	CONSTRUCTOR(ElementType::VEC4,   ComponentType::F32, vec4,   f32, glm::vec4)
	CONSTRUCTOR(ElementType::VEC2,   ComponentType::I32, vec2,   i32, glm::ivec2)
	CONSTRUCTOR(ElementType::VEC3,   ComponentType::I32, vec3,   i32, glm::ivec3)
	CONSTRUCTOR(ElementType::VEC4,   ComponentType::I32, vec4,   i32, glm::ivec4)
	CONSTRUCTOR(ElementType::MAT2X2, ComponentType::F32, mat2x2, f32, glm::mat2)
	CONSTRUCTOR(ElementType::MAT3X3, ComponentType::F32, mat3x3, f32, glm::mat3)
	CONSTRUCTOR(ElementType::MAT4X4, ComponentType::F32, mat4x4, f32, glm::mat4)
};

struct SamplerBinding {
	DefaultUniforms::Item uniform = DefaultUniforms::all[0];
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
