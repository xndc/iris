#include "assets/texture.hh"
#include "assets/asset_loader.hh"

#include <unordered_map>

#include <stb_image.h>
#include <stb_image_resize.h>
#include <SDL.h>

#include "base/debug.hh"

static bool TextureLoader_Initialised = false;

namespace DefaultTextures {
	Texture White_1x1 = {};
	Texture Black_1x1 = {};
	Texture Red_1x1 = {};
}

namespace DefaultSamplers {
	Sampler NearestRepeat = {};
	Sampler LinearRepeat = {};
	Sampler MipmappedNearestRepeat = {};
	Sampler MipmappedLinearRepeat = {};
}

std::unordered_map<uint64_t, Texture> TextureLoader_Cache = {};
std::unordered_map<uint64_t, Sampler> SamplerLoader_Cache = {};

static void UploadStagedLevels(Texture& texture) {
	// TODO: Support more image formats than just the 8-bit UNORM ones
	GLenum internalformat = GL_RGBA8, format = GL_RGBA, type = GL_UNSIGNED_BYTE;
	switch (texture.channels) {
		case 1: internalformat = GL_R8;    format = GL_RED;  break;
		case 2: internalformat = GL_RG8;   format = GL_RG;   break;
		case 3: internalformat = GL_RGB8;  format = GL_RGB;  break;
		case 4: internalformat = GL_RGBA8; format = GL_RGBA; break;
	}

	if (texture.gl_texture == 0) {
		glGenTextures(1, &texture.gl_texture);
		glBindTexture(GL_TEXTURE_2D, texture.gl_texture);
		// The default min-filter is NEAREST_MIPMAP_LINEAR, which requires the texture to be mipmap
		// complete. LINEAR is a more sensible default.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		// REPEAT is the default, but we might as well be explicit about it.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexStorage2D(GL_TEXTURE_2D, texture.num_levels, internalformat, texture.width, texture.height);
	} else {
		glBindTexture(GL_TEXTURE_2D, texture.gl_texture);
	}

	for (uint32_t i = 0; i < texture.num_levels; i++) {
		Texture::Level& l = texture.levels[i];
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, l.width, l.height, format, type, l.staging_buffer);
		l.staging_buffer = nullptr;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
}

static void UploadSampler(Sampler& sampler) {
	if (sampler.gl_sampler == 0) {
		glGenSamplers(1, &sampler.gl_sampler);
	}
	glSamplerParameteri(sampler.gl_sampler, GL_TEXTURE_MIN_FILTER, sampler.params.min_filter);
	glSamplerParameteri(sampler.gl_sampler, GL_TEXTURE_MAG_FILTER, sampler.params.mag_filter);
	glSamplerParameteri(sampler.gl_sampler, GL_TEXTURE_WRAP_S, sampler.params.wrap_s);
	glSamplerParameteri(sampler.gl_sampler, GL_TEXTURE_WRAP_T, sampler.params.wrap_t);
}

void InitTextureLoader() {
	if (TextureLoader_Initialised) { return; }

	GLubyte staging_white_1x1[] = {255, 255, 255, 255};
	DefaultTextures::White_1x1.width  = 1;
	DefaultTextures::White_1x1.height = 1;
	DefaultTextures::White_1x1.channels = 4;
	DefaultTextures::White_1x1.num_levels = 1;
	DefaultTextures::White_1x1.levels[0] = {.width = 1, .height = 1, .staging_buffer = staging_white_1x1};
	UploadStagedLevels(DefaultTextures::White_1x1);

	GLubyte staging_black_1x1[] = {0, 0, 0, 0};
	DefaultTextures::Black_1x1.width  = 1;
	DefaultTextures::Black_1x1.height = 1;
	DefaultTextures::Black_1x1.channels = 4;
	DefaultTextures::Black_1x1.num_levels = 1;
	DefaultTextures::Black_1x1.levels[0] = {.width = 1, .height = 1, .staging_buffer = staging_black_1x1};
	UploadStagedLevels(DefaultTextures::Black_1x1);

	GLubyte staging_red_1x1[] = {255, 0, 0, 0};
	DefaultTextures::Red_1x1.width  = 1;
	DefaultTextures::Red_1x1.height = 1;
	DefaultTextures::Red_1x1.channels = 4;
	DefaultTextures::Red_1x1.num_levels = 1;
	DefaultTextures::Red_1x1.levels[0] = {.width = 1, .height = 1, .staging_buffer = staging_red_1x1};
	UploadStagedLevels(DefaultTextures::Red_1x1);

	DefaultSamplers::NearestRepeat.params.min_filter = GL_NEAREST;
	DefaultSamplers::NearestRepeat.params.mag_filter = GL_NEAREST;
	UploadSampler(DefaultSamplers::NearestRepeat);

	DefaultSamplers::LinearRepeat.params.min_filter = GL_LINEAR;
	DefaultSamplers::LinearRepeat.params.mag_filter = GL_LINEAR;
	UploadSampler(DefaultSamplers::LinearRepeat);

	DefaultSamplers::MipmappedNearestRepeat.params.min_filter = GL_NEAREST_MIPMAP_NEAREST;
	DefaultSamplers::MipmappedNearestRepeat.params.mag_filter = GL_NEAREST;
	UploadSampler(DefaultSamplers::MipmappedNearestRepeat);

	DefaultSamplers::MipmappedLinearRepeat.params.min_filter = GL_LINEAR_MIPMAP_LINEAR;
	DefaultSamplers::MipmappedLinearRepeat.params.mag_filter = GL_LINEAR;
	UploadSampler(DefaultSamplers::MipmappedLinearRepeat);

	const uint32_t max_expected_textures = 256, max_expected_samplers = 32;
	TextureLoader_Cache.reserve(max_expected_textures);
	SamplerLoader_Cache.reserve(max_expected_samplers);

	TextureLoader_Initialised = true;
}

static uint8_t MipchainLevelCount(uint32_t w, uint32_t h) {
	// https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_non_power_of_two.txt
	return 1 + static_cast<uint8_t>(floorf(log2f(float(Max(Max(w, h), 2U)))));
}

Texture* GetTexture(uint64_t source_path_hash, const char* source_path, bool generate_mips) {
	Texture& texture = TextureLoader_Cache[source_path_hash];

	bool uninitialised = (texture.source_path == nullptr);
	bool needs_reupload = (!uninitialised && (generate_mips && !texture.generate_mips));
	if (!(uninitialised || needs_reupload)) {
		return &texture;
	}

	float ticks_per_msec = float(SDL_GetPerformanceFrequency()) * 0.001f;
	uint64_t time_get_start = SDL_GetPerformanceCounter();

	if (texture.gl_texture != 0 && needs_reupload) {
		glDeleteTextures(1, &texture.gl_texture);
		memset(&texture.levels, 0, sizeof(texture.levels));
	}

	texture.source_path = String::copy(source_path);
	texture.generate_mips = generate_mips;

	int w, h, c;
	uint8_t* image = stbi_load(source_path, &w, &h, &c, 0);
	if (!image) {
		LOG_F(ERROR, "Failed to load %s: %s", source_path, stbi_failure_reason());
		texture.width = 1;
		texture.height = 1;
		texture.channels = 4;
		texture.num_levels = 1;
		texture.gl_texture = DefaultTextures::Red_1x1.gl_texture;
		return &texture;
	}
	texture.width  = texture.levels[0].width  = static_cast<uint32_t>(w);
	texture.height = texture.levels[0].height = static_cast<uint32_t>(h);
	texture.channels = static_cast<uint32_t>(c);

	uint64_t time_disk_load_end = SDL_GetPerformanceCounter();

	texture.num_levels = generate_mips ? MipchainLevelCount(w, h) : 1;
	uint32_t texture_size = 0;
	uint32_t mip_w = texture.width, mip_h = texture.height;
	for (uint32_t i = 0; i < texture.num_levels; i++) {
		texture_size += mip_w * mip_h * c;
		mip_w = Max(1U, mip_w / 2U);
		mip_h = Max(1U, mip_h / 2U);
	}

	// Allocate staging mipchain and copy level 0 into it
	uint8_t* mipchain = static_cast<uint8_t*>(calloc(texture_size, 1));
	CHECK_NOTNULL_F(mipchain);
	texture.levels[0].staging_buffer = mipchain;
	memcpy(mipchain, image, w * h * c);

	if (generate_mips) {
		uint32_t mip_w = texture.width, mip_h = texture.height;
		uint32_t mip_offset = mip_w * mip_h * c;
		for (uint32_t i = 1; i < texture.num_levels; i++) {
			mip_w = Max(1U, mip_w / 2U);
			mip_h = Max(1U, mip_h / 2U);
			texture.levels[i].width  = mip_w;
			texture.levels[i].height = mip_h;
			uint8_t* staging_buffer = &mipchain[mip_offset];
			mip_offset += mip_w * mip_h * c;
			texture.levels[i].staging_buffer = staging_buffer;
			// Downscale from previous level to current level
			Texture::Level& last = texture.levels[i-1];
			if (!stbir_resize_uint8(last.staging_buffer, last.width, last.height, last.width * c,
				staging_buffer, mip_w, mip_h, mip_w * c, c))
			{
				// If downscale fails, fill level with red so this is visible
				for (size_t i = 0; i < mip_w * mip_h * c; i++) { staging_buffer[i] = (i % c) ? 0 : 255; }
				LOG_F(ERROR, "Downscale failed for texture %s level %u (%ux%u)", source_path, i, mip_w, mip_h);
			}
		}
	}

	uint64_t time_mipgen_end = SDL_GetPerformanceCounter();

	UploadStagedLevels(texture);

	uint64_t time_upload_end = SDL_GetPerformanceCounter();
	LOG_F(INFO, "Texture %s: load %.03fms mipgen %.03f ms upload %.03fms gltex=%u", source_path,
		float(time_disk_load_end - time_get_start) / ticks_per_msec,
		float(time_mipgen_end - time_disk_load_end) / ticks_per_msec,
		float(time_upload_end - time_mipgen_end) / ticks_per_msec, texture.gl_texture);

	stbi_image_free(image);
	free(mipchain);

	return &texture;
}

Sampler* GetSampler(const SamplerParams& params) {
	uint64_t hash = Hash64(&params, sizeof(params));
	Sampler& sampler = SamplerLoader_Cache[hash];
	if (sampler.gl_sampler) { return &sampler; }

	sampler.params = params;
	UploadSampler(sampler);

	return &sampler;
}
