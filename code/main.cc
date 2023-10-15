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
#include "engine/deferred.hh"
#include "graphics/opengl.hh"
#include "scene/gameobject.hh"
#include "scene/light.hh"
#include "assets/asset_loader.hh"
#include "assets/texture.hh"
#include "assets/mesh.hh"
#include "assets/model.hh"
#include "assets/shader.hh"
#include "graphics/render.hh"
#include "editor/editor_camera.hh"

#if PLATFORM_WEB
#include <emscripten.h>
#endif

static void loop(void);

static SDL_Window* window;
static SDL_GLContext gl_context;
static Engine engine;
static ImFont* font_inter_16;
static ImFont* font_inter_14;
static GameObject* scene;

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
	InitDebugSystem(argc, argv);
	SDL_Init(SDL_INIT_EVERYTHING);

	engine = Engine();
	engine.initial_t = SDL_GetPerformanceCounter();
	engine.this_frame.ignore_for_timing = true;

	window = SDL_CreateWindow("Iris",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		engine.display_w, engine.display_h,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
	CHECK_NOTNULL_F(window, "Failed to create SDL window");

	gl_context = GLCreateContext(window);
	GLMakeContextCurrent(window, gl_context);

	CreateMeshes();
	InitAssetLoader();
	ProcessShaderUpdates(engine);

	// This engine uses reverse-Z (0.0f is far) for higher precision. To actually get this precision
	// increase, we have to set the Z clip-space bounds to [0,1] instead of the default [-1,1]. This
	// isn't supported on WebGL, GLES and macOS; we have to live with bad precision there.
	if (glClipControl) {
		glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
	} else if (glDepthRangedNV) {
		glDepthRangedNV(-1.0f, 1.0f);
	}

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

	scene = new GameObject();

	// znear=0.5f results in reasonably high depth precision even without clip-control support
	engine.cam_main = scene->Add(new EditorCamera());
	engine.cam_main->position = vec3(0.0f, 5.0f, 0.0f);

	Model* sponza = GetModelFromGLTF("data/models/Sponza/Sponza.gltf");
	scene->AddCopy(sponza->root_object);

	DirectionalLight* dl = scene->Add(new DirectionalLight());
	dl->position = vec3(0.1, 1.0, 0.1);
	dl->color = vec3(2.0, 2.0, 2.0);

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

	// Poll and swap times are dependent on the platform and may take abnormally long because of
	// things outside our control (e.g. window resize or webpage focus loss).
	if ((engine.last_frame.t_poll - engine.last_frame.t > 100.0f) ||
		(engine.this_frame.t - engine.last_frame.t_defer > 100.0f))
	{
		engine.last_frame.ignore_for_timing = true;
	}

	if (!engine.last_frame.ignore_for_timing) {
		engine.metrics_poll  .push(frame_start_t, engine.last_frame.t_poll   - engine.last_frame.t);
		engine.metrics_update.push(frame_start_t, engine.last_frame.t_update - engine.last_frame.t_poll);
		engine.metrics_render.push(frame_start_t, engine.last_frame.t_render - engine.last_frame.t_update);
		engine.metrics_defer .push(frame_start_t, engine.last_frame.t_defer  - engine.last_frame.t_render);
		engine.metrics_swap  .push(frame_start_t, engine.this_frame.t - engine.last_frame.t_defer);
		// defer_plt is swap+deferred, render_plt is swap+deferred+render, etc.
		engine.metrics_defer_plt .push(frame_start_t, engine.this_frame.t - engine.last_frame.t_render);
		engine.metrics_render_plt.push(frame_start_t, engine.this_frame.t - engine.last_frame.t_update);
		engine.metrics_update_plt.push(frame_start_t, engine.this_frame.t - engine.last_frame.t_poll);
		engine.metrics_poll_plt  .push(frame_start_t, engine.this_frame.t - engine.last_frame.t);
	}

	SDL_Event event = {0};
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
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
		}
		ImGui_ImplSDL2_ProcessEvent(&event);
	}

	SDL_GL_GetDrawableSize(window, (int*)&engine.display_w, (int*)&engine.display_h);
	UpdateRenderTargets(engine);

	engine.this_frame.t_poll = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;
	if (engine.this_frame.t_poll - engine.this_frame.t > 100.0f) {
		engine.this_frame.ignore_for_timing = true;
	}

	// Start ImGUI frame early to allow the various update functions to use it
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	ProcessShaderUpdates(engine);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::BeginMainMenuBar();
	ImVec2 menu_size = ImGui::GetWindowSize();
	ImGui::PopStyleVar();

	if (ImGui::BeginMenu("Windows")) {
		ImGui::MenuItem("Performance Stats", NULL, &engine.ui_show_perf_graph);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Buffers")) {
		auto bufferVisMenuItem = [](Engine& engine, const char* name, DebugVisBuffer buffer) {
			if (ImGui::MenuItem(name, NULL, engine.debugvis_buffer == buffer)) {
				engine.debugvis_buffer = (engine.debugvis_buffer == buffer) ? DebugVisBuffer::NONE : buffer;
			}
		};
		bufferVisMenuItem(engine, "GBuffer Diffuse",  DebugVisBuffer::GBUF_COLOR);
		bufferVisMenuItem(engine, "GBuffer Material", DebugVisBuffer::GBUF_MATERIAL);
		bufferVisMenuItem(engine, "GBuffer Normal",   DebugVisBuffer::GBUF_NORMAL);
		bufferVisMenuItem(engine, "GBuffer Velocity", DebugVisBuffer::GBUF_VELOCITY);
		bufferVisMenuItem(engine, "Depth (Linear)", DebugVisBuffer::DEPTH_LINEAR);
		bufferVisMenuItem(engine, "Depth (Raw)", DebugVisBuffer::DEPTH_RAW);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Tonemapper")) {
		auto tonemapperMenuItem = [](Engine& engine, const char* name, Tonemapper::Type tonemapper) {
			if (ImGui::MenuItem(name, NULL, engine.tonemapper.type == tonemapper)) {
				engine.tonemapper.type = tonemapper;
			}
		};
		tonemapperMenuItem(engine, "Linear",   Tonemapper::LINEAR);
		tonemapperMenuItem(engine, "Reinhard", Tonemapper::REINHARD);
		tonemapperMenuItem(engine, "Hable",    Tonemapper::HABLE);
		tonemapperMenuItem(engine, "ACES",     Tonemapper::ACES);
		ImGui::SliderFloat("Exposure", &engine.tonemapper.exposure, 0.0f, 30.0f);
		ImGui::EndMenu();
	}

	const char* helptext = "Use WASDQE/Shift/Space to move, hold RMB to rotate camera";
	float helptext_width = ImGui::CalcTextSize(helptext).x;
	ImGui::SameLine(menu_size.x - helptext_width - 18);
	ImGui::TextUnformatted(helptext);

	ImGui::EndMainMenuBar();

	if (engine.ui_show_perf_graph) {
		ImGui::PushFont(font_inter_14);
		ImGui::SetNextWindowPos(ImVec2(10, menu_size.y + 10));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("Stats", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
		ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
		if (engine.metrics_poll.used != 0 &&
			ImPlot::BeginPlot("Stats Plot", ImVec2(320, 140), ImPlotFlags_NoMouseText | ImPlotFlags_NoTitle |
				ImPlotFlags_NoFrame | ImPlotFlags_NoInputs))
		{
			ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);
			double time_min = INFINITY, time_max = -INFINITY, val_max = -INFINITY;
			auto metric_minmax = [](double& time_min, double& time_max, double& val_max, const MetricBuffer& metric) {
				double tmin = double(metric.min_time()), tmax = double(metric.max_time()), vmax = double(metric.max());
				if (tmin < time_min) { time_min = tmin; }
				if (tmax > time_max) { time_max = tmax; }
				if (vmax > val_max)  { val_max  = vmax; }
			};
			metric_minmax(time_min, time_max, val_max, engine.metrics_poll);
			metric_minmax(time_min, time_max, val_max, engine.metrics_update);
			metric_minmax(time_min, time_max, val_max, engine.metrics_render);
			metric_minmax(time_min, time_max, val_max, engine.metrics_defer);
			metric_minmax(time_min, time_max, val_max, engine.metrics_swap);
			double min_plot_time = Min(time_min, time_max - float(engine.metrics_poll.frames) * 4.0);
			ImPlot::SetupAxisLimits(ImAxis_X1, min_plot_time, time_max, ImPlotCond_Always);

			// Default plot Y-axis range suitable for 30FPS frames, with gradual transitions when needed
			static double max_plot_val = 33.3333;
			max_plot_val = (19.0 * max_plot_val + Max(val_max, 33.3333)) / 20.0;
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, max_plot_val, ImPlotCond_Always);

			// Plotting from cumulative arrays. Order is important to get the correct overlap.
			// Have to set colours manually because they're derived from the label by default.
			auto plot = [](const char* name, const MetricBuffer& region, const MetricBuffer& cumulative, ImVec4 color) {
				static char label[64];
				stbsp_snprintf(label, sizeof(label), "%s %.03fms max %.03fms", name, region.avg(), region.max());
				ImPlot::SetNextFillStyle(color, 1.0f);
				const float* xs = cumulative.times;
				const float* ys = cumulative.values;
				int offset = cumulative.next;
				ImPlot::PlotShaded<float>(label, xs, ys, cumulative.used, -INFINITY, 0, offset);
			};
			plot("poll",   engine.metrics_poll,   engine.metrics_poll_plt,   {0.32f, 0.80f, 0.96f, 1.0f});
			plot("update", engine.metrics_update, engine.metrics_update_plt, {0.87f, 0.36f, 0.91f, 1.0f});
			plot("render", engine.metrics_render, engine.metrics_render_plt, {0.65f, 0.96f, 0.38f, 1.0f});
			plot("defer",  engine.metrics_defer,  engine.metrics_defer_plt,  {0.95f, 0.40f, 0.20f, 1.0f});
			plot("swap",   engine.metrics_swap,   engine.metrics_swap,       {0.96f, 0.69f, 0.41f, 1.0f});
			ImPlot::EndPlot();
		}
		ImPlot::PopStyleVar();
		ImGui::End();
		ImGui::SetNextWindowPos(ImVec2(10, menu_size.y + 170));
		ImGui::SetNextWindowBgAlpha(0.5f);
		if (ImGui::Begin("Draw Stats", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
		{
			ImGui::Text("Draws: %u", engine.last_frame.total_drawcalls);
			ImGui::SameLine(80);
			ImGui::Text("Polys: %u", engine.last_frame.total_polys_rendered);
		}
		ImGui::End();
		ImGui::PopFont();
	}

	scene->RecursiveUpdate(engine);
	scene->RecursiveUpdateTransforms();
	scene->RecursiveLateUpdate(engine);

	ImGui::Render(); // doesn't emit drawcalls, so it belongs in the update section; should be last

	engine.this_frame.t_update = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	static RenderList render_list;
	render_list.UpdateFromScene(engine, scene, engine.cam_main);

	glViewport(0, 0, engine.display_w, engine.display_h);

	Framebuffer* gbuffer = GetFramebuffer({
		&RenderTargets::Albedo,
		&RenderTargets::Normal,
		&RenderTargets::Material,
		&RenderTargets::Velocity,
		&RenderTargets::Depth,
	});

	RenderPass("GBuffer Clear", [&]() {
		BindFramebuffer(gbuffer);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClearDepth(0.0f); // reverse Z
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	});

	Framebuffer* fb_color_hdr = GetFramebuffer({
		&RenderTargets::ColorHDR,
	});

	RenderPass("HDR Clear", [&]() {
		BindFramebuffer(fb_color_hdr);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	});

	RenderPass("GBuffer", [&]() {
		VertShader* vsh = GetVertShader("data/shaders/core_transform.vert");
		FragShader* fsh = GetFragShader("data/shaders/gbuffer.frag");
		Program* program = GetProgram(vsh, fsh);
		Render(engine, render_list, engine.cam_main, program, nullptr, gbuffer, {});
	});

	Framebuffer* debugvis = NULL;
	if (engine.debugvis_buffer != DebugVisBuffer::NONE) {
		// Initial UpdateRenderTargets pass is before we enable/disable debugvis, need to recheck
		UpdateRenderTargets(engine);
		debugvis = GetFramebuffer({&RenderTargets::DebugVis});
	}

	// Read GBuffer data into debugvis buffer if enabled
	switch (engine.debugvis_buffer) {
		case DebugVisBuffer::GBUF_COLOR:
		case DebugVisBuffer::GBUF_MATERIAL:
		case DebugVisBuffer::GBUF_NORMAL:
		case DebugVisBuffer::GBUF_VELOCITY:
		case DebugVisBuffer::DEPTH_LINEAR:
		case DebugVisBuffer::DEPTH_RAW: {
			FragShader* fsh = GetFragShader("data/shaders/debugvis.frag");
			RenderEffect(engine, fsh, gbuffer, debugvis, {});
		} break;
		default:;
	}

	for (RenderableDirectionalLight& light : render_list.directional_lights) {
		UpdateShadowRenderTargets(*light.object);

		Framebuffer* shadowmap = GetFramebuffer({&RenderTargets::ShadowMap});
		Framebuffer* gbuffer_plus_shadowmap = GetFramebuffer({
			&RenderTargets::Albedo,
			&RenderTargets::Normal,
			&RenderTargets::Material,
			&RenderTargets::ShadowMap,
			&RenderTargets::Depth,
		});

		static Material* shadow_material = nullptr;
		if (ExpectFalse(!shadow_material)) {
			shadow_material = new Material();
			shadow_material->face_culling_mode = GL_FRONT; // render only backfaces
			shadow_material->depth_test_func = GL_GREATER;
			shadow_material->blend_mode = BlendMode::Stippled;
		}

		String pass_name_shadowmap = String::format("%s Shadow Map", light.object->Name().cstr);
		RenderPass(pass_name_shadowmap.cstr, [&]() {
			BindFramebuffer(shadowmap);
			glViewport(0, 0, light.object->shadowmap_size, light.object->shadowmap_size);
			glClearDepth(0.0f); // reverse Z
			glClear(GL_DEPTH_BUFFER_BIT);
			VertShader* vsh = GetVertShader("data/shaders/core_transform_min.vert");
			FragShader* fsh = GetFragShader("data/shaders/shadowmap.frag");
			Program* program = GetProgram(vsh, fsh);
			Render(engine, render_list, light.object, program, nullptr, shadowmap, {}, shadow_material,
				RenderFlags::UseOriginalAlbedo | RenderFlags::UseOriginalStippleParams);
			glViewport(0, 0, engine.display_w, engine.display_h);
		});

		String pass_name_accumulation = String::format("%s Accumulation", light.object->Name().cstr);
		RenderPass(pass_name_accumulation.cstr, [&]() {
			FragShader* fsh = GetFragShader("data/shaders/light_directional.frag");
			RenderEffect(engine, fsh, gbuffer_plus_shadowmap, fb_color_hdr, {
				UniformValue(Uniforms::LightPosition, light.position),
				UniformValue(Uniforms::LightColor, light.color),
				UniformValue(Uniforms::ShadowWorldToClip, light.object->this_frame.vp),
				UniformValue(Uniforms::ShadowBiasMin, light.object->shadow_bias_min),
				UniformValue(Uniforms::ShadowBiasMax, light.object->shadow_bias_max),
				UniformValue(Uniforms::ShadowPCFTapsX, int32_t(light.object->shadow_pcf_taps_x)),
				UniformValue(Uniforms::ShadowPCFTapsY, int32_t(light.object->shadow_pcf_taps_y)),
			}, RenderEffectFlags::BlendAdditive);
		});
	}

	RenderPass("Tonemap & PostFX", [&]() {
		FragShader* fsh = GetFragShader("data/shaders/tonemap_postfx.frag");
		RenderEffect(engine, fsh, fb_color_hdr, nullptr, {
			UniformValue(Uniforms::TonemapExposure, engine.tonemapper.exposure),
		});
	});

	// Blit debugvis framebuffer to main framebuffer if enabled
	if (engine.debugvis_buffer != DebugVisBuffer::NONE) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, debugvis->gl_framebuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, engine.display_w, engine.display_h, 0, 0, engine.display_w, engine.display_h,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	RenderPass("Editor UI", [&]() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	});

	engine.this_frame.t_render = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	// Run one deferred action.
	// TODO: Run multiple actions if there's time. The logic for that might be nontrivial.
	RunDeferredAction(engine);

	engine.this_frame.t_defer = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	SDL_GL_SwapWindow(window);
}
