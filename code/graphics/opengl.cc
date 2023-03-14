#include "graphics/opengl.hh"

#include "base/debug.hh"

SDL_GLContext GLCreateContext(SDL_Window* window) {
	#if PLATFORM_DESKTOP || PLATFORM_MOBILE
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		// FIXME: Remove once we start using FBOs
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
	#endif
	SDL_GLContext gl_context = SDL_GL_CreateContext(window);
	CHECK_NOTNULL_F(gl_context, "Failed to create OpenGL context: %s", SDL_GetError());
	return gl_context;
}

void GLMakeContextCurrent(SDL_Window* window, SDL_GLContext context) {
	CHECK_EQ_F(SDL_GL_MakeCurrent(window, context), 0,
		"Failed to make OpenGL context current: %s", SDL_GetError());

	#if PLATFORM_DESKTOP
		int gl_version = gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress);
		CHECK_NE_F(gl_version, 0, "Failed to load OpenGL functions");
	#endif
}
