#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <stb_sprintf.h>

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
#include "assets/mesh.hh"
#include "assets/model.hh"
#include "assets/shader.hh"

#if PLATFORM_WEB
#include <emscripten.h>
#endif

static void loop(void);

static SDL_Window* window;
static SDL_GLContext gl_context;
static Engine engine;
static ImFont* font_inter_16;
static ImFont* font_inter_14;

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
	InitDebugSystem(argc, argv);
	SDL_Init(SDL_INIT_EVERYTHING);

	engine = Engine();
	engine.initial_t = SDL_GetPerformanceCounter();
	engine.this_frame.ignore_for_timing = true;

	window = SDL_CreateWindow("Lycoris",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		engine.display_w, engine.display_h,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
	CHECK_NOTNULL_F(window, "Failed to create SDL window");

	gl_context = GLCreateContext(window);
	GLMakeContextCurrent(window, gl_context);

	CreateDefaultMeshes();
	InitAssetLoader();

	if (SDL_GL_SetSwapInterval(-1) == -1) {
		SDL_GL_SetSwapInterval(1);
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::StyleColorsDark();
	ImGuiIO& io = ImGui::GetIO();

	const ImWchar font_ranges[] = {
		0x0020, 0x024F, // Basic Latin + Latin-1 + Latin Extended-A + Latin Extended-B
		0x0370, 0x06FF, // Greek, Cyrillic, Armenian, Hebrew, Arabic
		0x1E00, 0x20CF, // Latin Additional, Greek Extended, Punctuation, Superscripts and Subscripts, Currency
		0
	};

	font_inter_16 = io.Fonts->AddFontFromFileTTF("data/fonts/Inter_Medium.otf", 16, NULL, font_ranges);
	font_inter_14 = io.Fonts->AddFontFromFileTTF("data/fonts/Inter_Medium.otf", 14, NULL, font_ranges);
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

	if (!engine.last_frame.ignore_for_timing) {
		engine.metrics_poll  .push(frame_start_t, engine.last_frame.t_poll   - engine.last_frame.t);
		engine.metrics_update.push(frame_start_t, engine.last_frame.t_update - engine.last_frame.t_poll);
		engine.metrics_render.push(frame_start_t, engine.last_frame.t_render - engine.last_frame.t_update);
		engine.metrics_swap  .push(frame_start_t, engine.this_frame.t - engine.last_frame.t_render);
		engine.metrics_update_plt.push(frame_start_t, engine.last_frame.t_update - engine.last_frame.t);
		engine.metrics_render_plt.push(frame_start_t, engine.last_frame.t_render - engine.last_frame.t);
		engine.metrics_swap_plt  .push(frame_start_t, engine.this_frame.t        - engine.last_frame.t);
	}

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
	if (engine.this_frame.t_poll - engine.this_frame.t > 100.0f) {
		engine.this_frame.ignore_for_timing = true;
	}

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
	ImGui::PushFont(font_inter_14);
	ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
	if (engine.metrics_poll.used != 0 &&
		ImPlot::BeginPlot("Stats Plot", ImVec2(320, 140), ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle |
			ImPlotFlags_NoFrame | ImPlotFlags_NoChild | ImPlotFlags_NoInputs))
	{
		ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);
		double time_min = Min<double>(
			Min(engine.metrics_poll.min_time(), engine.metrics_update.min_time()),
			Min(engine.metrics_swap.min_time(), engine.metrics_render.min_time()));
		double time_max = Max<double>(
			Max(engine.metrics_poll.max_time(), engine.metrics_update.max_time()),
			Max(engine.metrics_swap.max_time(), engine.metrics_render.max_time()));
		ImPlot::SetupAxisLimits(ImAxis_X1, time_min, time_max, ImPlotCond_Always);

		// Default plot Y-axis range suitable for 30FPS frames, with gradual transitions when needed
		double val_max = Max<double>(
			Max(engine.metrics_poll.max(), engine.metrics_update.max()),
			Max(engine.metrics_swap.max(), engine.metrics_render.max()));
		static double max_plot_val = 33.3333;
		max_plot_val = (19.0 * max_plot_val + Max(val_max, 33.3333)) / 20.0;
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, max_plot_val, ImPlotCond_Always);

		static char label_poll[64], label_update[64], label_render[64], label_swap[64];
		stbsp_snprintf(label_poll, sizeof(label_poll), "poll %.03fms max %.03fms",
			engine.metrics_poll.avg(), engine.metrics_poll.max());
		stbsp_snprintf(label_update, sizeof(label_update), "update %.03fms max %.03fms",
			engine.metrics_update.avg(), engine.metrics_update.max());
		stbsp_snprintf(label_render, sizeof(label_render), "render %.03fms max %.03fms",
			engine.metrics_render.avg(), engine.metrics_render.max());
		stbsp_snprintf(label_swap, sizeof(label_swap), "swap %.03fms max %.03fms",
			engine.metrics_swap.avg(), engine.metrics_swap.max());

		// Plotting from cumulative arrays, in reverse order to get the correct overlap
		// Have to set colours manually because they're derived from the label by default
		ImPlot::SetNextFillStyle(ImVec4(0.32f, 0.8f, 0.96f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_swap, engine.metrics_swap_plt.times, engine.metrics_swap_plt.values,
			engine.metrics_swap_plt.used, -INFINITY, 0, engine.metrics_swap_plt.next);
		ImPlot::SetNextFillStyle(ImVec4(0.87f, 0.36f, 0.96f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_render, engine.metrics_render_plt.times, engine.metrics_render_plt.values,
			engine.metrics_render_plt.used, -INFINITY, 0, engine.metrics_swap_plt.next);
		ImPlot::SetNextFillStyle(ImVec4(0.65f, 0.96f, 0.38f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_update, engine.metrics_update_plt.times, engine.metrics_update_plt.values,
			engine.metrics_update_plt.used, -INFINITY, 0, engine.metrics_swap_plt.next);
		ImPlot::SetNextFillStyle(ImVec4(0.96f, 0.69f, 0.41f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_poll, engine.metrics_poll.times, engine.metrics_poll.values,
			engine.metrics_poll.used, -INFINITY, 0, engine.metrics_swap_plt.next);
		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
	ImGui::PopFont();
	ImGui::End();

	ImGui::Render(); // doesn't emit drawcalls, so it belongs in the update section

	Model* duck = GetModelFromGLTF("data/models/Duck/Duck.gltf");

	VertShader* vsh = GetVertShader("data/shaders/core_fullscreen.vert");
	FragShader* fsh = GetFragShader("data/shaders/debug_pos_clip.frag");
	Program* prog = GetProgram(vsh, fsh);

	engine.this_frame.t_update = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	glViewport(0, 0, engine.display_w, engine.display_h);
	glClearColor(0.3f, 0.4f, 0.55f, 1.0f);
	glClearDepth(0.0f); // reverse Z
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(prog->gl_program);
	glUniform2f(glGetUniformLocation(prog->gl_program, "FramebufferSize"),
		float(engine.display_w), float(engine.display_h));
	glBindVertexArray(DefaultMeshes::QuadXZ.gl_vertex_array);
	glDrawElements(DefaultMeshes::QuadXZ.ptype.gl_enum(), DefaultMeshes::QuadXZ.index_buffer.total_components(),
		DefaultMeshes::QuadXZ.index_buffer.ctype.gl_enum(), nullptr);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	engine.this_frame.t_render = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	SDL_GL_SwapWindow(window);
}
