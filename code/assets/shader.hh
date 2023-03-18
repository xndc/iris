#pragma once
#include "base/string.hh"
#include "graphics/opengl.hh"

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
};

struct VertShader : Shader {
	VertShader(): Shader{VERTEX, GL_VERTEX_SHADER} {}
};

struct FragShader : Shader {
	FragShader(): Shader{FRAGMENT, GL_FRAGMENT_SHADER} {}
};

struct Program {
	VertShader* vsh;
	FragShader* fsh;
	GLuint gl_program;
	Program() = default;
	Program(VertShader* vsh, FragShader* fsh): vsh{vsh}, fsh{fsh} {}
};

VertShader* GetVertShader(const char* path);
FragShader* GetFragShader(const char* path);

Program* GetProgram(VertShader* vsh, FragShader* fsh);

void ProcessShaderUpdates(const Engine& engine);
