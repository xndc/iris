#include <SDL.h>
#include <sqlite3.h>

#include "base/base.hh"
#include "base/debug.hh"
#include "base/math.hh"
#include "base/string.hh"
#include "graphics/opengl.hh"
#include "scene/gameobject.hh"

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

	GameObject* scene = new GameObject{};
	scene->Add(new PointLight{vec3(0.2, 0.4, 0.6)});
	scene->Add(new PointLight{vec3(0.6, 0.1, 0.1)});
	scene->Add(new DirectionalLight{vec3(0.1, 0.1, 0.1)});
	scene->Add(new DirectionalLight{vec3(0.2, 0.2, 0.2)});
	scene->Add(new MeshInstance{nullptr});

	for (GameObject& obj : scene->Children()) {
		if (obj.type == GameObject::DIRECTIONAL_LIGHT) {
			obj.Delete();
			break;
		}
	}

	String log = String::format("* %p %s", scene, scene->GetTypeName());
	for (GameObject& obj : scene->Children()) {
		log = String::join(std::move(log), String::format("\n  * %p %s", &obj, obj.GetTypeName()));
	}
	LOG_F(INFO, "All objects:\n%s", log.cstr);

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
