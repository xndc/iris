#include <SDL.h>
#include <sqlite3.h>

#include "base/base.hh"
#include "base/debug.hh"
#include "base/math.hh"
#include "base/string.hh"
#include "base/filesystem.hh"
#include "engine/engine.hh"
#include "graphics/opengl.hh"
#include "scene/gameobject.hh"
#include "assets/asset_loader.hh"
#include "assets/texture.hh"
#include "assets/model.hh"
#include "assets/shader.hh"

#if PLATFORM_WEB
#include <emscripten.h>
#endif

static void loop(void);

static SDL_Window* window;
static SDL_GLContext gl_context;
static Engine engine;

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
	InitDebugSystem(argc, argv);
	SDL_Init(SDL_INIT_EVERYTHING);

	engine = Engine();

	window = SDL_CreateWindow("Lycoris",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		engine.display_w, engine.display_h,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
	CHECK_NOTNULL_F(window, "Failed to create SDL window");

	gl_context = GLCreateContext(window);
	GLMakeContextCurrent(window, gl_context);

	InitAssetLoader();

	if (SDL_GL_SetSwapInterval(-1) == -1) {
		SDL_GL_SetSwapInterval(1);
	}

	#if defined(EMSCRIPTEN)
		emscripten_set_main_loop(loop, 0, true);
	#else
		for(;;) { loop(); }
	#endif

	return 0;
}

static void loop(void) {
	engine.last_frame = engine.this_frame;
	engine.this_frame = FrameState(engine.last_frame, 0.0f);

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

	SDL_GL_GetDrawableSize(window, (int*)&engine.display_w, (int*)&engine.display_h);

	uint32_t asset_loader_ops_left = 1;
	while (asset_loader_ops_left) {
		asset_loader_ops_left = ProcessAssetLoadOperation();
	}

	if (engine.this_frame.n == 60) {
		LOG_F(INFO, "Frame %llu", engine.this_frame.n);
		engine.tonemapper.type = Tonemapper::ACES;
	}

	ProcessShaderUpdates(engine);

	glViewport(0, 0, engine.display_w, engine.display_h);
	glClearColor(0.3f, 0.4f, 0.55f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	Model* duck = GetModelFromGLTF("data/models/Duck/Duck.gltf");
	// Model* sponza = GetModelFromGLTF("data/models/Sponza/Sponza.gltf");

	VertShader* vsh = GetVertShader("data/shaders/core_fullscreen.vert");
	FragShader* fsh = GetFragShader("data/shaders/debug_pos_clip.frag");
	Program* prog = GetProgram(vsh, fsh);

	SDL_GL_SwapWindow(window);
}
