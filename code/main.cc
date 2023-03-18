#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

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
	engine.initial_t = SDL_GetPerformanceCounter();

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

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();

	const ImWchar font_ranges[] = {
		0x0020, 0x024F, // Basic Latin + Latin-1 + Latin Extended-A + Latin Extended-B
		0x0370, 0x06FF, // Greek, Cyrillic, Armenian, Hebrew, Arabic
		0x1E00, 0x20CF, // Latin Additional, Greek Extended, Punctuation, Superscripts and Subscripts, Currency
		0
	};

	io.Fonts->AddFontFromFileTTF("data/fonts/Inter_Medium.otf", 16, NULL, font_ranges);
	io.Fonts->Build();

	ImGuiStyle& s = ImGui::GetStyle();
	s.FramePadding.y = 2;

	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();

	#if defined(EMSCRIPTEN)
		emscripten_set_main_loop(loop, 0, true);
	#else
		for(;;) { loop(); }
	#endif

	return 0;
}

static void loop(void) {
	float msec_per_tick = 1.0f / (float(SDL_GetPerformanceFrequency()) * 0.001f);
	float frame_start_t = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;
	engine.last_frame = engine.this_frame;
	engine.this_frame = FrameState(engine.last_frame, frame_start_t);

	engine.metrics_poll.push(engine.last_frame.t_poll - engine.last_frame.t);
	engine.metrics_update.push(engine.last_frame.t_update - engine.last_frame.t_poll);
	engine.metrics_render.push(engine.last_frame.t_render - engine.last_frame.t_update);
	engine.metrics_swap.push(engine.this_frame.t - engine.last_frame.t_render);

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

	engine.this_frame.t_poll = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	uint32_t asset_loader_ops_left = 1;
	while (asset_loader_ops_left) {
		asset_loader_ops_left = ProcessAssetLoadOperation();
	}

	if (engine.this_frame.n == 60) {
		LOG_F(INFO, "Frame %llu", engine.this_frame.n);
		engine.tonemapper.type = Tonemapper::ACES;
	}

	ProcessShaderUpdates(engine);

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowBgAlpha(0.4f);
	ImGui::Begin("Stats", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
	ImGui::Text("poll");   ImGui::SameLine(60); ImGui::Text("%.03f ms", engine.metrics_poll.avg());
	ImGui::Text("update"); ImGui::SameLine(60); ImGui::Text("%.03f ms", engine.metrics_update.avg());
	ImGui::Text("submit"); ImGui::SameLine(60); ImGui::Text("%.03f ms", engine.metrics_render.avg());
	ImGui::Text("swap");   ImGui::SameLine(60); ImGui::Text("%.03f ms", engine.metrics_swap.avg());
	ImGui::End();

	ImGui::Render(); // doesn't emit drawcalls, so it belongs in the update section

	Model* duck = GetModelFromGLTF("data/models/Duck/Duck.gltf");

	VertShader* vsh = GetVertShader("data/shaders/core_fullscreen.vert");
	FragShader* fsh = GetFragShader("data/shaders/debug_pos_clip.frag");
	Program* prog = GetProgram(vsh, fsh);

	engine.this_frame.t_update = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	glViewport(0, 0, engine.display_w, engine.display_h);
	glClearColor(0.3f, 0.4f, 0.55f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	engine.this_frame.t_render = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	SDL_GL_SwapWindow(window);
}
