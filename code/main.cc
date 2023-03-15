#include <SDL.h>
#include <sqlite3.h>

#include "base/base.hh"
#include "base/debug.hh"
#include "base/math.hh"
#include "graphics/opengl.hh"
#include "scene/scene.hh"

#if PLATFORM_WEB
#include <emscripten.h>
#endif

static void loop(void);

static SDL_Window* window;
static SDL_GLContext gl_context;

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
	InitDebugSystem(argc, argv);
	SDL_Init(SDL_INIT_EVERYTHING);

	window = SDL_CreateWindow("Lycoris",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		1280, 720,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
	CHECK_NOTNULL_F(window, "Failed to create SDL window");

	gl_context = GLCreateContext(window);
	GLMakeContextCurrent(window, gl_context);

	if (SDL_GL_SetSwapInterval(-1) == -1) {
		SDL_GL_SetSwapInterval(1);
	}

	sqlite3* db;
	CHECK_EQ_F(sqlite3_open_v2(NULL, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, NULL), SQLITE_OK,
		"Failed to create test SQLite database: %s", sqlite3_errmsg(db));

	Scene* scene = new Scene{};
	scene->New<PointLight>() = PointLight{.color = vec3(0.2, 0.4, 0.6)};
	scene->New<PointLight>() = PointLight{.color = vec3(0.6, 0.1, 0.1)};
	scene->New<DirectionalLight>() = DirectionalLight{.color = vec3(0.1, 0.1, 0.1)};
	LOG_F(INFO, "PointLights: %p %u", scene->arrays[PointLight::type], scene->counts[PointLight::type]);
	for (PointLight& p : scene->Iter<PointLight>()) {
		LOG_F(INFO, "* %p R=%.02f G=%.02f B=%.02f", &p, p.color.r, p.color.g, p.color.b);
	}
	LOG_F(INFO, "All Objects:");
	for (GameObject& obj : scene->IterAll()) {
		LOG_F(INFO, "* %p", &obj);
	}

	#if defined(EMSCRIPTEN)
		emscripten_set_main_loop(loop, 0, true);
	#else
		for(;;) { loop(); }
	#endif

	return 0;
}

static void loop(void) {
	SDL_Event event = {0};
	while (SDL_PollEvent(&event)) { switch (event.type) {
		case SDL_QUIT: {
			exit(0);
		} break;
		case SDL_WINDOWEVENT: switch (event.window.event) {
			case SDL_WINDOWEVENT_CLOSE: {
				if (event.window.windowID == SDL_GetWindowID(window)) {
					exit(0);
				}
			} break;
		} break;
	}}

	int w, h;
	SDL_GL_GetDrawableSize(window, &w, &h);

	glViewport(0, 0, w, h);
	glClearColor(0.3f, 0.4f, 0.55f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	SDL_GL_SwapWindow(window);
}
