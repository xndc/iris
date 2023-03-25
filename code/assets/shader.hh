#pragma once
#include "base/string.hh"
#include "base/math.hh"
#include "graphics/opengl.hh"
#include "graphics/defaults.hh"
#include "assets/mesh.hh"

struct Engine; // from engine/engine.hh

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

struct Shader {
	enum Type {
		VERTEX,
		FRAGMENT,
	} type;
	String source_path;
	String source_code;
	uint64_t mtime;
	GLenum gl_type;
	GLuint gl_shader;

	Shader() = default;
	Shader(Type type, GLenum gl_type): type{type}, gl_type{gl_type} {}
	void invalidate();
};

struct VertShader : Shader {};
struct FragShader : Shader {};

struct Program {
	VertShader* vsh;
	FragShader* fsh;
	GLuint gl_program;
	String name;

	Program() = default;
	Program(VertShader* vsh, FragShader* fsh): vsh{vsh}, fsh{fsh} {}
	void invalidate();

	GLint uniform_locations [CountOf(DefaultUniforms::all)];
	GLint location(const DefaultUniforms::Item& uniform);
	bool set(const UniformValue& u);
};

VertShader* GetVertShader(const char* path);
FragShader* GetFragShader(const char* path);

Program* GetProgram(VertShader* vsh, FragShader* fsh);

void ProcessShaderUpdates(const Engine& engine);
