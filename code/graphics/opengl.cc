#include "graphics/opengl.hh"

#include "base/debug.hh"

#if DEBUG && PLATFORM_DESKTOP
#define ENABLE_GL_DEBUG_MODE 1
void GLAPIENTRY GLDebugMessageCallback (GLenum src, GLenum type, GLuint id, GLenum sev, GLsizei len,
	const char* msg, const void* uparam)
{
	switch (type) {
		case GL_DEBUG_TYPE_ERROR: { LOG_F(INFO, "GL error: %s", msg); break; }
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: { LOG_F(INFO, "GL deprecation warning: %s", msg); break; }
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: { LOG_F(INFO, "GL UB warning: %s", msg); break; }
		case GL_DEBUG_TYPE_PORTABILITY: { LOG_F(INFO, "GL portability warning: %s", msg); break; }
		case GL_DEBUG_TYPE_PERFORMANCE: { LOG_F(INFO, "GL performance warning: %s", msg); break; }
		case GL_DEBUG_TYPE_PUSH_GROUP: break;
		case GL_DEBUG_TYPE_POP_GROUP: break;
		default: { LOG_F(INFO, "GL message: %s", msg); }
	}
}
#endif

SDL_GLContext GLCreateContext(SDL_Window* window) {
	#if PLATFORM_DESKTOP || PLATFORM_MOBILE
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		// FIXME: Remove once we start using FBOs
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
	#endif
	#if ENABLE_GL_DEBUG_MODE
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
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

	#if ENABLE_GL_DEBUG_MODE
		if (glDebugMessageCallback) {
			glDebugMessageCallback(GLDebugMessageCallback, nullptr);
			glEnable(GL_DEBUG_OUTPUT);
			// Try to disable GLPushDebugGroup/GLPopDebugGroup messages.
			// FIXME: We target GL 4.0 so this doesn't work (on Windows/AMD anyway).
			glDebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DONT_CARE, GL_DEBUG_TYPE_PUSH_GROUP,
				0, nullptr, GL_FALSE);
			glDebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DONT_CARE, GL_DEBUG_TYPE_POP_GROUP,
				0, nullptr, GL_FALSE);
			const char message[] = "OpenGL debug messages enabled";
			glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_OTHER, 0,
				GL_DEBUG_SEVERITY_NOTIFICATION, sizeof(message), message);
		}
	#endif
}

void GLObjectLabel(GLenum identifier, GLuint name, const char* label) {
	#if !PLATFORM_WEB
		if (glObjectLabel) { glObjectLabel(identifier, name, -1, label); }
	#endif
}

void GLPushDebugGroup(GLenum source, GLuint id, const char* message) {
	#if !PLATFORM_WEB
		if (glPushDebugGroup) { glPushDebugGroup(source, id, -1, message); }
	#endif
}

void GLPopDebugGroup() {
	#if !PLATFORM_WEB
		if (glPopDebugGroup) { glPopDebugGroup(); }
	#endif
}
