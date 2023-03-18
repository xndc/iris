#pragma once
#include "base/string.hh"
#include "base/hash.hh"
#include "graphics/opengl.hh"

// Represents a 2D texture that may be fully, partially or not at all loaded into GPU memory.
// To retrieve a texture object usable for rendering, use GetTexture().
// TODO: We only support 8-bit UNORM textures at the moment. We probably want to support more
// formats, including compressed ones.
struct Texture {
	String source_path;
	bool generate_mips;

	bool loaded = false;
	uint32_t width = 0;
	uint32_t height = 0;
	uint8_t channels = 0;

	static constexpr uint32_t MaxLevels = 16; // max level count for a 64k texture
	struct Level {
		uint32_t width = 0;
		uint32_t height = 0;
		// CPU-side staging buffer containing RGBA8 image data for this level.
		// Will be deallocated using free() once the level is uploaded.
		uint8_t* staging_buffer = nullptr;
	};
	uint8_t num_levels = 0;
	Level levels[MaxLevels];

	GLuint gl_texture = 0;

	Texture(): source_path{nullptr}, generate_mips{false} {}

	Texture(const String& source_path, bool generate_mips = false):
		source_path{String::copy(source_path)}, generate_mips{generate_mips} {}

	uint32_t size() const {
		uint32_t accum = 0;
		for (uint32_t i = 0; i < num_levels; i++) {
			accum += levels[i].width * levels[i].height * channels;
		}
		return accum;
	}
};

namespace DefaultTextures {
	extern Texture White_1x1;
	extern Texture Black_1x1;
	extern Texture Red_1x1;
}

// Allocates or returns a previously allocated Texture object for the given path and parameters.
// Once requested, the texture will be uploaded to the GPU when possible.
Texture* GetTexture(uint64_t source_path_hash, const char* source_path, bool generate_mips = false);

static Texture* GetTexture(const char* source_path, bool generate_mips = false) {
	return GetTexture(Hash64(source_path), source_path, generate_mips);
}

// Represents a set of texture sampling parameters.
struct SamplerParams {
	GLenum min_filter = GL_LINEAR;
	GLenum mag_filter = GL_LINEAR;
	GLenum wrap_s = GL_REPEAT;
	GLenum wrap_t = GL_REPEAT;
};

// Represents a generic sampler object that can be used with any texture.
struct Sampler {
	SamplerParams params = {};
	GLuint gl_sampler = 0;
};

namespace DefaultSamplers {
	extern Sampler NearestRepeat;
	extern Sampler LinearRepeat;
	extern Sampler MipmappedNearestRepeat;
	extern Sampler MipmappedLinearRepeat;
}

// Allocates or returns a previously allocated Sampler for the given parameters.
Sampler* GetSampler(const SamplerParams& params);
