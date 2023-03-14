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

#if !PLATFORM_DESKTOP
	#define glClearDepth(depth) glClearDepthf(depth)
#endif
