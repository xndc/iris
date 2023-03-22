#pragma once
#include "base/base.hh"
#include "base/debug.hh"
#include "graphics/opengl.hh"

struct ImageFormat {
	enum Enum {
		RGBA8,
		RGB8,
		RG8,
		RG11B10,
		RGB16,
		RG16,
		D32,
		D32S8,
		D24S8,
		Count,
	} v;
	constexpr ImageFormat(uint8_t v = 0) : v{Enum(v)} {};

	constexpr GLenum gl_internalformat() const {
		switch (v) {
			case RGBA8: return GL_RGBA8;
			case RGB8: return GL_RGB8;
			case RG8: return GL_RG8;
			case RG11B10: return GL_R11F_G11F_B10F;
			case RGB16: return GL_RGB16F;
			case RG16: return GL_RG16F;
			case D32: return GL_DEPTH_COMPONENT32F;
			case D32S8: return GL_DEPTH32F_STENCIL8;
			case D24S8: return GL_DEPTH24_STENCIL8;
			case Count: Unreachable();
		} Unreachable();
	}

	static constexpr ImageFormat from_gl_internalformat(GLenum gl) {
		for (uint8_t i = 0; i < Count; i++) {
			if (gl == ImageFormat(i).gl_internalformat()) {
				return ImageFormat(i);
			}
		}
		Panic("Invalid GL internalformat %u", gl);
	}

	constexpr GLenum gl_base_format() const {
		switch (v) {
			case RGBA8: return GL_RGBA;
			case RGB8: return GL_RGB;
			case RG8: return GL_RG;
			case RG11B10: return GL_RGB;
			case RGB16: return GL_RGB;
			case RG16: return GL_RG;
			case D32: return GL_DEPTH_COMPONENT;
			case D32S8: return GL_DEPTH_STENCIL;
			case D24S8: return GL_DEPTH_STENCIL;
			case Count: Unreachable();
		} Unreachable();
	}

	constexpr GLenum gl_element_type() const {
		switch (v) {
			case RGBA8: return GL_UNSIGNED_BYTE;
			case RGB8: return GL_UNSIGNED_BYTE;
			case RG8: return GL_UNSIGNED_BYTE;
			case RG11B10: return GL_UNSIGNED_INT_10F_11F_11F_REV;
			case RGB16: return GL_HALF_FLOAT;
			case RG16: return GL_HALF_FLOAT;
			case D32: return GL_FLOAT;
			case D32S8: return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
			case D24S8: return GL_UNSIGNED_INT_24_8;
			case Count: Unreachable();
		} Unreachable();
	}

	static constexpr ImageFormat from_gl_types(GLenum base_format, GLenum element_type) {
		for (uint8_t i = 0; i < Count; i++) {
			if (base_format == ImageFormat(i).gl_base_format() && element_type == ImageFormat(i).gl_element_type()) {
				return ImageFormat(i);
			}
		}
		Panic("Invalid GL base format and element type combination %u / %u", base_format, element_type);
	}

	constexpr GLenum gl_framebuffer_base_attachment() const {
		switch (v) {
			case RGBA8: return GL_COLOR_ATTACHMENT0;
			case RGB8: return GL_COLOR_ATTACHMENT0;
			case RG8: return GL_COLOR_ATTACHMENT0;
			case RG11B10: return GL_COLOR_ATTACHMENT0;
			case RGB16: return GL_COLOR_ATTACHMENT0;
			case RG16: return GL_COLOR_ATTACHMENT0;
			case D32: return GL_DEPTH_ATTACHMENT;
			case D32S8: return GL_DEPTH_STENCIL_ATTACHMENT;
			case D24S8: return GL_DEPTH_STENCIL_ATTACHMENT;
			case Count: Unreachable();
		} Unreachable();
	}
};
