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

	window = SDL_CreateWindow("Lycoris",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		engine.display_w, engine.display_h,
		SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
	CHECK_NOTNULL_F(window, "Failed to create SDL window");

	gl_context = GLCreateContext(window);
	GLMakeContextCurrent(window, gl_context);

	CreateDefaultMeshes();
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
	dl->position = vec3(1, 1, 1);
	dl->color = vec3(10, 10, 10);

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
		(engine.this_frame.t - engine.last_frame.t_render > 100.0f))
	{
		engine.last_frame.ignore_for_timing = true;
	}

	if (!engine.last_frame.ignore_for_timing) {
		engine.metrics_poll  .push(frame_start_t, engine.last_frame.t_poll   - engine.last_frame.t);
		engine.metrics_update.push(frame_start_t, engine.last_frame.t_update - engine.last_frame.t_poll);
		engine.metrics_render.push(frame_start_t, engine.last_frame.t_render - engine.last_frame.t_update);
		engine.metrics_swap  .push(frame_start_t, engine.this_frame.t - engine.last_frame.t_render);
		// render_plt is swap+render, update_plt is swap+render+update, poll_plt is the entire frame
		engine.metrics_render_plt.push(frame_start_t, engine.this_frame.t - engine.last_frame.t_update);
		engine.metrics_update_plt.push(frame_start_t, engine.this_frame.t - engine.last_frame.t_poll);
		engine.metrics_poll_plt  .push(frame_start_t, engine.this_frame.t - engine.last_frame.t);
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
	UpdateRenderTargets(engine);

	engine.this_frame.t_poll = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;
	if (engine.this_frame.t_poll - engine.this_frame.t > 100.0f) {
		engine.this_frame.ignore_for_timing = true;
	}

	uint32_t asset_loader_ops_left = 1;
	while (asset_loader_ops_left) {
		asset_loader_ops_left = ProcessAssetLoadOperation();
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
		double min_plot_time = Min(time_min, time_max - float(engine.metrics_poll.frames) * 4.0);
		ImPlot::SetupAxisLimits(ImAxis_X1, min_plot_time, time_max, ImPlotCond_Always);

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

		// Plotting from cumulative arrays. Order is important to get the correct overlap.
		// Have to set colours manually because they're derived from the label by default.
		ImPlot::SetNextFillStyle(ImVec4(0.32f, 0.8f, 0.96f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_poll, engine.metrics_poll_plt.times, engine.metrics_poll_plt.values,
			engine.metrics_poll_plt.used, -INFINITY, 0, engine.metrics_poll_plt.next);
		ImPlot::SetNextFillStyle(ImVec4(0.87f, 0.36f, 0.96f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_update, engine.metrics_update_plt.times, engine.metrics_update_plt.values,
			engine.metrics_update_plt.used, -INFINITY, 0, engine.metrics_update_plt.next);
		ImPlot::SetNextFillStyle(ImVec4(0.65f, 0.96f, 0.38f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_render, engine.metrics_render_plt.times, engine.metrics_render_plt.values,
			engine.metrics_render_plt.used, -INFINITY, 0, engine.metrics_render_plt.next);
		ImPlot::SetNextFillStyle(ImVec4(0.96f, 0.69f, 0.41f, 1.0f), 1.0f);
		ImPlot::PlotShaded<float>(label_swap, engine.metrics_swap.times, engine.metrics_swap.values,
			engine.metrics_swap.used, -INFINITY, 0, engine.metrics_swap.next);
		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
	ImGui::PopFont();
	ImGui::End();

	ImGui::Render(); // doesn't emit drawcalls, so it belongs in the update section

	scene->RecursiveUpdate(engine);
	scene->RecursiveUpdateTransforms();
	scene->RecursiveLateUpdate(engine);

	engine.this_frame.t_update = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	static RenderList render_list;
	render_list.UpdateFromScene(engine, scene, engine.cam_main);

	glViewport(0, 0, engine.display_w, engine.display_h);

	Framebuffer* gbuffer = GetFramebuffer({
		&DefaultRenderTargets::Albedo,
		&DefaultRenderTargets::Normal,
		&DefaultRenderTargets::Material,
		&DefaultRenderTargets::Velocity,
		&DefaultRenderTargets::Depth,
	});

	RenderPass("GBuffer Clear", [&]() {
		BindFramebuffer(gbuffer);
		glClearColor(0.3f, 0.4f, 0.55f, 1.0f);
		glClearDepth(0.0f); // reverse Z
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	});

	RenderPass("GBuffer", [&]() {
		VertShader* vsh = GetVertShader("data/shaders/core_transform.vert");
		FragShader* fsh = GetFragShader("data/shaders/gbuffer.frag");
		Program* program = GetProgram(vsh, fsh);
		Render(engine, render_list, engine.cam_main, program, nullptr, gbuffer, {});
	});

	RenderPass("Directional Light Accumulation", [&]() {
		for (RenderableDirectionalLight& light : render_list.directional_lights) {
			FragShader* fsh = GetFragShader("data/shaders/light_directional.frag");
			RenderEffect(engine, fsh, gbuffer, nullptr, {
				UniformValue(DefaultUniforms::LightPosition, light.position),
				UniformValue(DefaultUniforms::LightColor, light.color),
			});
		}
	});

	RenderPass("Editor UI", [&]() {
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	});

	engine.this_frame.t_render = (SDL_GetPerformanceCounter() - engine.initial_t) * msec_per_tick;

	SDL_GL_SwapWindow(window);

#if 1 // FIXME: Debug code, strip out once scene graph can be visualised graphically
	if (engine.this_frame.n == 10) {
		LOG_F(INFO, "Scene graph:");
		char spaces[] = "                                                  ";
		uint32_t indent = 0;
		scene->Recurse([&](GameObject& obj) {
			LOG_F(INFO, "%s* %s", &spaces[sizeof(spaces) - indent - 1], obj.DebugName().cstr);
			indent++;
		}, [&](GameObject& obj) { indent--; });

		LOG_F(INFO, "Render lists:");
		for (RenderListPerView& v : render_list.views) {
			LOG_F(INFO, "* Camera <%p> pos=[%.02f %.02f %.02f]", v.camera,
				v.camera->world_position.x, v.camera->world_position.y, v.camera->world_position.z);
			for (auto& [k, m] : v.meshes) {
				LOG_F(INFO, "  * Mesh <%p> instances=[%u:%u] (%u)", m.mesh, m.first_instance,
					m.first_instance + m.instance_count - 1, m.instance_count);
				for (uint32_t i = m.first_instance; i < m.first_instance + m.instance_count; i++) {
					RenderableMeshInstanceData& rmid = v.mesh_instances[i];
					vec3 position, scale, skew; quat rotation; vec4 perspective;
					glm::decompose(rmid.local_to_world, scale, rotation, position, skew, perspective);
					LOG_F(INFO, "    * Instance %u pos=[%.02f %.02f %.02f]", i, position.x, position.y, position.z);
				}
			}
		}
		for (RenderableDirectionalLight& r : render_list.directional_lights) {
			LOG_F(INFO, "* DirectionalLight pos=[%.02f %.02f %.02f] color=[%.02f %.02f %.02f]",
				r.position.x, r.position.y, r.position.z,
				r.color.x, r.color.y, r.color.z);
		}
		for (RenderablePointLight& r : render_list.point_lights) {
			LOG_F(INFO, "* PointLight pos=[%.02f %.02f %.02f] color=[%.02f %.02f %.02f]",
				r.position.x, r.position.y, r.position.z,
				r.color.x, r.color.y, r.color.z);
		}
		for (RenderableAmbientCube& r : render_list.ambient_cubes) {
			LOG_F(INFO, "* AmbientCube pos=[%.02f %.02f %.02f]", r.position.x, r.position.y, r.position.z);
		}
	}
#endif
}
