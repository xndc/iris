#pragma once
#include "base/string.hh"
#include "base/math.hh"
#include "graphics/opengl.hh"
#include "graphics/defaults.hh"

struct Engine; // from engine/engine.hh

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

	template<typename T> void uniform(const DefaultUniforms::Item& uniform, T value);
	template<> void uniform<float>(const DefaultUniforms::Item& uniform, float value);
	template<> void uniform<vec2>(const DefaultUniforms::Item& uniform, vec2 value);
	template<> void uniform<vec3>(const DefaultUniforms::Item& uniform, vec3 value);
	template<> void uniform<vec4>(const DefaultUniforms::Item& uniform, vec4 value);
};

VertShader* GetVertShader(const char* path);
FragShader* GetFragShader(const char* path);

Program* GetProgram(VertShader* vsh, FragShader* fsh);

void ProcessShaderUpdates(const Engine& engine);
