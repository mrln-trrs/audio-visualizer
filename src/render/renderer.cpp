#include "render/renderer.h"

#include <iostream>
#include <vector>
#include <memory>
#include <array>
#include <algorithm>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "render/gl_window.h"
#include "render/fullscreen_quad.h"
#include "render/data_textures.h"
#include "render/spectrum_dynamics.h"
#include "render/frame_limiter.h"
#include "render/modes/visual_mode.h"
#include "render/modes/bars_mode.h"
#include "render/modes/radial_mode.h"
#include "render/modes/waveform_mode.h"
#include "render/modes/waterfall_mode.h"
#include "ui/theme.h"
#include "ui/telemetry.h"
#include "ui/hud.h"

namespace render {
namespace {

// Atajos de teclado: Tab o H alternan el HUD; 1 a 4 cambian de modo.
class KeyboardShortcuts {
public:
    void Poll(GLFWwindow* window, ui::HudState& hud, core::VisualizerConfig& cfg) {
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        const bool tab = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        const bool h = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        if ((tab && !tab_prev_) || (h && !h_prev_)) hud.visible = !hud.visible;
        tab_prev_ = tab;
        h_prev_ = h;

        static const std::array<std::pair<int, int>, 4> kModeKeys = { {
            { GLFW_KEY_1, core::MODE_BARS }, { GLFW_KEY_2, core::MODE_RADIAL },
            { GLFW_KEY_3, core::MODE_WAVEFORM }, { GLFW_KEY_4, core::MODE_WATERFALL } } };
        for (const auto& [key, mode] : kModeKeys) {
            if (glfwGetKey(window, key) == GLFW_PRESS) cfg.visual_mode = mode;
        }
    }

private:
    bool tab_prev_ = false;
    bool h_prev_ = false;
};

} // namespace

void RenderThread(core::VisualizerData& vis, core::SharedConfigData& shared_config, core::AudioData& audio) {
    std::cout << "Render: hilo iniciado con OpenGL 3.3 Core." << std::endl;

    core::VisualizerConfig cfg;
    {
        std::lock_guard<std::mutex> lock(shared_config.mtx);
        cfg = shared_config.config;
    }

    if (!glfwInit()) {
        std::cerr << "Render: no se pudo inicializar GLFW." << std::endl;
        return;
    }
    GLFWwindow* window = CreateMainWindow(1024, 600, "Audio Visualizer 2.0", cfg.vsync, &vis);
    if (!window) {
        glfwTerminate();
        return;
    }

    // Dear ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ui::ApplyObsidianTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Recursos compartidos y modos.
    FullscreenQuad quad;
    quad.Init();
    DataTextures textures;
    textures.Init();
    SpectrumDynamics dynamics;

    std::array<std::unique_ptr<IVisualMode>, core::MODE_COUNT> modes;
    modes[core::MODE_BARS] = std::make_unique<BarsMode>();
    modes[core::MODE_RADIAL] = std::make_unique<RadialMode>();
    modes[core::MODE_WAVEFORM] = std::make_unique<WaveformMode>();
    modes[core::MODE_WATERFALL] = std::make_unique<WaterfallMode>();
    for (auto& mode : modes) {
        if (!mode->Init()) std::cerr << "Render: el modo " << mode->Name() << " no pudo inicializarse." << std::endl;
    }

    // Estado del bucle.
    std::vector<float> spectrum;
    std::vector<float> waveform;
    uint64_t seen_generation = 0;
    uint64_t title_generation = 0;
    int title_frames = 0;
    double last_time = glfwGetTime();
    double title_time = last_time;

    ui::HudState hud;
    ui::Telemetry telemetry;
    KeyboardShortcuts keys;
    FrameLimiter limiter;
    telemetry.monitor_hz = RefreshRateForWindow(window);
    limiter.Start(cfg.vsync, cfg.max_fps, telemetry.monitor_hz, last_time);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        // dt real del cuadro, acotado para que un tirón no dispare la animación.
        const float dt = static_cast<float>(std::clamp(now - last_time, 0.0, 0.1));
        last_time = now;

        keys.Poll(window, hud, cfg);

        // Recoger el espectro nuevo si lo hay. El bloqueo dura una copia de ~1000 floats.
        const uint64_t generation = vis.generation.load(std::memory_order_acquire);
        const bool has_new_spectrum = (generation != seen_generation);
        if (has_new_spectrum) {
            std::lock_guard<std::mutex> lock(vis.mtx);
            spectrum = vis.spectrum;
            waveform = vis.waveform;
            seen_generation = generation;
        }

        const int num_bars = std::max(1, vis.atomic_num_bars.load());
        dynamics.Update(spectrum, num_bars, cfg, dt);

        // Texturas: las alturas animadas se suben cada cuadro para que el modo radial se mueva a
        // la tasa del monitor; la onda y la fila del espectrograma solo cuando hay datos nuevos.
        textures.UploadSpectrum(dynamics.heights());
        if (has_new_spectrum) {
            textures.UploadWaveform(waveform);
            textures.PushWaterfallRow(spectrum, cfg.amplitude_factor);
        }

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const int mode_index = std::clamp(cfg.visual_mode, 0, static_cast<int>(core::MODE_COUNT) - 1);
        const RenderContext ctx{ fbw, fbh, now, num_bars, cfg, textures, dynamics, quad };
        modes[mode_index]->Render(ctx);

        // Interfaz.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        telemetry.num_bars = num_bars;
        telemetry.limiter_active = limiter.active();
        ui::HudContext hud_ctx{ cfg, shared_config, vis, audio, telemetry, now };
        ui::DrawHud(hud, hud_ctx);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        limiter.WaitForNextFrame();

        // Métricas una vez por segundo.
        ++title_frames;
        if (now - title_time >= 1.0) {
            const double elapsed = now - title_time;
            const double fps = title_frames / elapsed;
            const double ups = static_cast<double>(seen_generation - title_generation) / elapsed;
            telemetry.Record(static_cast<float>(fps), static_cast<float>(ups));
            telemetry.monitor_hz = RefreshRateForWindow(window);
            limiter.UpdateEverySecond(fps, telemetry.monitor_hz, cfg.max_fps, glfwGetTime());
            telemetry.limiter_active = limiter.active();
            glfwSetWindowTitle(window, ui::BuildWindowTitle(modes[mode_index]->Name(), telemetry, audio.sample_rate.load()).c_str());
            title_frames = 0;
            title_time = now;
            title_generation = seen_generation;
        }
    }

    // Limpieza en orden inverso.
    limiter.Stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    for (auto& mode : modes) mode->Shutdown();
    textures.Shutdown();
    quad.Shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace render
