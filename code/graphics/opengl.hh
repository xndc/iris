#pragma once

#include "base/base.hh"

#if PLATFORM_DESKTOP
	#include <glad/gl.h>
#else
	#include <GLES3/gl32.h>
	#include <GLES3/gl2ext.h>
#endif

#include <SDL.h>

SDL_GLContext GLCreateContext(SDL_Window* window);
void GLMakeContextCurrent(SDL_Window* window, SDL_GLContext context);
void GLObjectLabel(GLenum identifier, GLuint name, const char* label);
void GLPushDebugGroup(GLenum source, GLuint id, const char* message);
void GLPopDebugGroup();

// Shims the engine needs to run correctly on GLES
#if !PLATFORM_DESKTOP
	#define GLAD_API_PTR
	#define GLAPIENTRY

	#define glClearDepth(depth) glClearDepthf(depth)

	// https://registry.khronos.org/OpenGL/extensions/ARB/ARB_clip_control.txt
	#define GL_LOWER_LEFT          0x8CA1
	#define GL_UPPER_LEFT          0x8CA2
	#define GL_NEGATIVE_ONE_TO_ONE 0x935E
	#define GL_ZERO_TO_ONE         0x935F
	#define GL_CLIP_ORIGIN         0x935C
	#define GL_CLIP_DEPTH_MODE     0x935D
	typedef void (GLAD_API_PTR *PFNGLCLIPCONTROLPROC)(GLenum origin, GLenum depth);
	static PFNGLCLIPCONTROLPROC glClipControl = nullptr;

	typedef void (GLAD_API_PTR *PFNGLDEPTHRANGEDNVPROC)(GLdouble zNear, GLdouble zFar);
	static PFNGLDEPTHRANGEDNVPROC glDepthRangedNV = nullptr;
#endif
