#include "assets/shader.hh"
#include "assets/asset_loader.hh"

#include <stdarg.h>
#include <unordered_map>
#include <functional>

#include <stb_sprintf.h>

#include "base/debug.hh"
#include "base/hash.hh"
#include "base/filesystem.hh"
#include "engine/engine.hh"

static bool ShaderLoader_Initialised = false;

StaticAssert(sizeof(VertShader) == sizeof(Shader));
StaticAssert(sizeof(FragShader) == sizeof(Shader));
static std::unordered_map<uint64_t, Shader> ShaderCache = {};
static std::unordered_map<uint64_t, Program> ProgramCache = {};

void InitShaderLoader() {
	if (ShaderLoader_Initialised) { return; }
	ShaderCache.reserve(32);
	ShaderLoader_Initialised = true;
}

static uint64_t ShaderDefineHash = 0;
static String ShaderDefineBlock = nullptr;
static Engine ShaderDefineLastEngineState = {};

// Generate or update ShaderDefineBlock given the engine's current configuration and state. Returns
// true if the defines were updated, i.e. if shaders need to be recompiled.
static bool UpdateShaderDefines(const Engine& engine) {
	enum { DECIDE, COUNT, WRITE } action;
	uint32_t size_required = 0;
	uint32_t bytes_written = 0;
	Engine& last = ShaderDefineLastEngineState;

	bool needs_update = false;
	if (!ShaderDefineBlock) { needs_update = true; }

	auto write = [&](const char* fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		if (action == COUNT) {
			size_required += stbsp_vsnprintf(nullptr, 0, fmt, ap);
		} else if (action == WRITE) {
			char* p = &(ShaderDefineBlock.mut()[bytes_written]);
			bytes_written += stbsp_vsnprintf(p, size_required - bytes_written, fmt, ap);
		}
		va_end(ap);
	};

	auto define = [&](std::function<bool()> value_was_updated, std::function<void()> write_value) {
		switch (action) {
			case DECIDE: needs_update |= value_was_updated(); break;
			default: write_value();
		}
	};

	auto defines = [&]() {
		define([&](){ return engine.tonemapper.type != last.tonemapper.type; }, [&](){
			switch (engine.tonemapper.type) {
				case Tonemapper::LINEAR:   write("#define TONEMAP_LINEAR\n");   break;
				case Tonemapper::REINHARD: write("#define TONEMAP_REINHARD\n"); break;
				case Tonemapper::HABLE:    write("#define TONEMAP_HABLE\n");    break;
				case Tonemapper::ACES:     write("#define TONEMAP_ACES\n");     break;
			}
		});

		define([&](){ return engine.debugvis_buffer != last.debugvis_buffer; }, [&](){
			bool debug_vis_enabled = true;
			switch (engine.debugvis_buffer) {
				case DebugVisBuffer::GBUF_COLOR:     write("#define DEBUG_VIS_GBUF_COLOR\n");     break;
				case DebugVisBuffer::GBUF_NORMAL:    write("#define DEBUG_VIS_GBUF_NORMAL\n");    break;
				case DebugVisBuffer::GBUF_MATERIAL:  write("#define DEBUG_VIS_GBUF_MATERIAL\n");  break;
				case DebugVisBuffer::GBUF_VELOCITY:  write("#define DEBUG_VIS_GBUF_VELOCITY\n");  break;
				case DebugVisBuffer::WORLD_POSITION: write("#define DEBUG_VIS_WORLD_POSITION\n"); break;
				case DebugVisBuffer::DEPTH_RAW:      write("#define DEBUG_VIS_DEPTH_RAW\n");      break;
				case DebugVisBuffer::DEPTH_LINEAR:   write("#define DEBUG_VIS_DEPTH_LINEAR\n");   break;
				case DebugVisBuffer::SHADOWMAP:      write("#define DEBUG_VIS_SHADOWMAP\n");      break;
				default: debug_vis_enabled = false;
			}
			if (debug_vis_enabled) { write("#define DEBUG_VIS"); }
		});
	};

	// Decide if the defines block needs to be updated:
	action = DECIDE; defines();
	if (!needs_update) { return false; }

	// Run through all the defines that would be produced to see how large the block needs to be.
	action = COUNT; defines();
	size_required += 1; // null terminator; needed because of how we use sprintf above
	ShaderDefineBlock = String(size_required - 1);

	// Run through all the defines again and write them out to the block.
	action = WRITE; defines();
	const_cast<uint32_t&>(ShaderDefineBlock._size) = bytes_written;

	LOG_F(INFO, "Generated new shader define block:\n%s", ShaderDefineBlock.cstr);

	ShaderDefineLastEngineState = engine;
	return true;
}

static void LoadShaderFromDisk(Shader& shader) {
	shader.mtime = GetFileModificationTime(shader.source_path);
	shader.source_code = ReadFile(shader.source_path);

	const char* expected_version = "#version 300 es";
	if (strstr(shader.source_code.cstr, expected_version) != shader.source_code.cstr) {
		LOG_F(ERROR, "Failed to load shader %s", shader.source_path.cstr);
		LOG_F(ERROR, "Expected shader to start with %s", expected_version);
		return;
	}

	GLuint gl_shader = glCreateShader(shader.gl_type);
	shader.gl_shader = gl_shader;

	const char* version = PLATFORM_DESKTOP ? "#version 330 core\n" : "#version 300 es\n";
	const char* code = &shader.source_code.cstr[strlen(expected_version)];
	const GLchar* sources[] = { version, ShaderDefineBlock.cstr, code };
	GLint lengths[] = { (GLint)strlen(version), (GLint)(ShaderDefineBlock.size()), (GLint)strlen(code) };
	glShaderSource(gl_shader, CountOf(sources), sources, lengths);

	glCompileShader(gl_shader);

	GLint ok, logsize;
	glGetShaderiv(gl_shader, GL_COMPILE_STATUS, &ok);
	glGetShaderiv(gl_shader, GL_INFO_LOG_LENGTH, &logsize);

	auto log = String(logsize);
	if (logsize > 0) {
		glGetShaderInfoLog(gl_shader, logsize, NULL, log.mut());
	}

	if (!ok) {
		LOG_F(ERROR, "Error compiling shader %u from %s:\n%s", gl_shader, shader.source_path.cstr, log.cstr);
		return;
	}

	if (logsize > 0) {
		LOG_F(WARNING, "Compiled shader %u from %s with warnings:\n%s", gl_shader, shader.source_path.cstr, log.cstr);
	} else {
		LOG_F(INFO, "Compiled shader %u from %s", gl_shader, shader.source_path.cstr);
	}

	GLObjectLabel(GL_SHADER, gl_shader, shader.source_path.cstr);
}

VertShader* GetVertShader(const char* path) {
	Shader& gen_shader = ShaderCache[Hash64(path)];
	auto& shader = static_cast<VertShader&>(gen_shader);

	if (shader.gl_shader != 0) {
		DCHECK_EQ_F(shader.type, Shader::VERTEX);
		return &shader;
	}

	shader.type = Shader::VERTEX;
	shader.gl_type = GL_VERTEX_SHADER;
	shader.source_path = String::copy(path);
	LoadShaderFromDisk(shader);

	return &shader;
}

FragShader* GetFragShader(const char* path) {
	Shader& gen_shader = ShaderCache[Hash64(path)];
	auto& shader = static_cast<FragShader&>(gen_shader);

	if (shader.gl_shader != 0) {
		DCHECK_EQ_F(shader.type, Shader::FRAGMENT);
		return &shader;
	}

	shader.type = Shader::FRAGMENT;
	shader.gl_type = GL_FRAGMENT_SHADER;
	shader.source_path = String::copy(path);
	LoadShaderFromDisk(shader);

	return &shader;
}

Program* GetProgram(VertShader* vsh, FragShader* fsh) {
	uint64_t key[2] = {uint64_t(vsh), uint64_t(fsh)};
	uint64_t hash = Hash64(reinterpret_cast<char*>(key));
	Program& program = ProgramCache[hash];

	if (program.gl_program != 0) {
		return &program;
	}

	program = Program(vsh, fsh);
	DCHECK_NOTNULL_F(program.vsh);
	DCHECK_NOTNULL_F(program.fsh);

	auto name = String::format("[%s %s]", program.vsh->source_path.cstr, program.fsh->source_path.cstr);
	program.name = name;

	if (program.vsh->gl_shader == 0 || program.fsh->gl_shader == 0) {
		LOG_F(INFO, "Can't link program as shaders have not been compiled");
		program.gl_program = 0;
	}

	GLuint gl_program = glCreateProgram();
	program.gl_program = gl_program;

	glAttachShader(gl_program, program.vsh->gl_shader);
	glAttachShader(gl_program, program.fsh->gl_shader);
	glLinkProgram(gl_program);

	GLint ok, logsize;
	glGetProgramiv(gl_program, GL_LINK_STATUS, &ok);
	glGetProgramiv(gl_program, GL_INFO_LOG_LENGTH, &logsize);

	auto log = String(logsize);
	if (logsize > 0) {
		glGetProgramInfoLog(gl_program, logsize, NULL, log.mut());
	}

	if (!ok) {
		LOG_F(ERROR, "Failed to link program %u %s:\n%s", gl_program, name.cstr, log.cstr);
		return &program;
	}

	if (logsize > 0) {
		LOG_F(WARNING, "Linked program %u %s with warnings:\n%s", gl_program, name.cstr, log.cstr);
	} else {
		LOG_F(INFO, "Linked program %u %s", gl_program, name.cstr);
	}

	GLObjectLabel(GL_PROGRAM, gl_program, name.cstr);

	for (uint32_t i = 0; i < CountOf(DefaultUniforms::all); i++) {
		program.uniform_locations[i] = glGetUniformLocation(gl_program, DefaultUniforms::all[i].name);
	}

	for (const DefaultAttributes::Item& attrib : DefaultAttributes::all) {
		glBindAttribLocation(gl_program, attrib.index, attrib.name);
	}

	return &program;
}

void ProcessShaderUpdates(const Engine& engine) {
	if (UpdateShaderDefines(engine)) {
		for (auto& [key, shader] : ShaderCache) {
			shader.invalidate();
		}
		for (auto& [key, program] : ProgramCache) {
			program.invalidate();
		}
	}
	// Detect on-disk shader changes. Pointless for web/mobile builds since the "disk" is read-only.
	if (PLATFORM_DESKTOP) {
		static size_t bucket_idx = 0;
		size_t bucket_count = ShaderCache.bucket_count();
		Shader* invalidated_shader = nullptr;

		for (size_t i = bucket_idx; i < bucket_idx + bucket_count; i++) {
			size_t bucket = i % bucket_count;
			for (auto it = ShaderCache.begin(bucket); it != ShaderCache.end(bucket); ++it) {
				Shader& shader = it->second;
				if (shader.mtime != 0 && shader.mtime != GetFileModificationTime(shader.source_path)) {
					shader.invalidate();
					invalidated_shader = &shader;
					break;
				}
			}
			if (invalidated_shader) { break; }
		}
		bucket_idx = (bucket_idx + 1) % bucket_count;

		if (invalidated_shader) {
			for (auto& [key, program] : ProgramCache) {
				if (program.vsh == invalidated_shader || program.fsh == invalidated_shader) {
					program.invalidate();
				}
			}
		}
	}
}

void Shader::invalidate() {
	if (gl_shader && !PLATFORM_WEB) { glDeleteShader(gl_shader); }
	gl_shader = 0;
}

void Program::invalidate() {
	if (gl_program && !PLATFORM_WEB) { glDeleteProgram(gl_program); }
	gl_program = 0;
}

GLint Program::location(const DefaultUniforms::Item& uniform) {
	for (uint32_t i = 0; i < CountOf(DefaultUniforms::all); i++) {
		if (DefaultUniforms::all[i].hash == uniform.hash) {
			return uniform_locations[i];
		}
	}
	return glGetUniformLocation(gl_program, uniform.name);
}

template<> void Program::uniform<uint32_t>(const DefaultUniforms::Item& uniform, uint32_t value) {
	glUniform1i(location(uniform), value);
}
template<> void Program::uniform<float>(const DefaultUniforms::Item& uniform, float value) {
	glUniform1f(location(uniform), value);
}
template<> void Program::uniform<vec2>(const DefaultUniforms::Item& uniform, vec2 value) {
	glUniform2fv(location(uniform), 1, reinterpret_cast<float*>(&value));
}
template<> void Program::uniform<vec3>(const DefaultUniforms::Item& uniform, vec3 value) {
	glUniform3fv(location(uniform), 1, reinterpret_cast<float*>(&value));
}
template<> void Program::uniform<vec4>(const DefaultUniforms::Item& uniform, vec4 value) {
	glUniform4fv(location(uniform), 1, reinterpret_cast<float*>(&value));
}
