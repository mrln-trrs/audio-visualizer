#include "render/renderer.h"

#include <iostream>
#include <vector>
#include <memory>
#include <array>
#include <algorithm>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "core/band_layout.h"
#include "platform/window_effects.h"
#include "render/gl_window.h"
#include "render/fullscreen_quad.h"
#include "render/data_textures.h"
#include "render/band_waveform_texture.h"
#include "render/bar_spectrum.h"
#include "render/spectrum_dynamics.h"
#include "render/frame_limiter.h"
#include "render/post_process.h"
#include "render/panel_material.h"
#include "render/modes/visual_mode.h"
#include "render/modes/bars_mode.h"
#include "render/modes/radial_mode.h"
#include "render/modes/waveform_mode.h"
#include "render/modes/waterfall_mode.h"
#include "render/modes/band_meters_mode.h"
#include "render/modes/stacked_oscilloscope_mode.h"
#include "ui/theme.h"
#include "ui/telemetry.h"
#include "ui/hud.h"
#include "ui/motion.h"

namespace render {
namespace {

// Duraciones de las transiciones (docs/12, sección 7).
constexpr double kHudFadeSeconds = 0.20;
constexpr double kModeCrossfadeSeconds = 0.15;

// Atajos de teclado: Tab o H alternan el HUD; 1 a 6 cambian de modo.
class KeyboardShortcuts {
public:
    void Poll(GLFWwindow* window, ui::HudState& hud, core::VisualizerConfig& cfg) {
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        const bool tab = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        const bool h = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        if ((tab && !tab_prev_) || (h && !h_prev_)) hud.visible = !hud.visible;
        tab_prev_ = tab;
        h_prev_ = h;

        static const std::array<std::pair<int, int>, 6> kModeKeys = { {
            { GLFW_KEY_1, core::MODE_BARS }, { GLFW_KEY_2, core::MODE_RADIAL },
            { GLFW_KEY_3, core::MODE_WAVEFORM }, { GLFW_KEY_4, core::MODE_WATERFALL },
            { GLFW_KEY_5, core::MODE_STACKED_OSCILLOSCOPE }, { GLFW_KEY_6, core::MODE_BAND_METERS } } };
        for (const auto& [key, mode] : kModeKeys) {
            if (glfwGetKey(window, key) == GLFW_PRESS) cfg.visual_mode = mode;
        }
    }

private:
    bool tab_prev_ = false;
    bool h_prev_ = false;
};

// Partición en bandas del lado del render, reconstruida con la misma función que usa el hilo de
// análisis para que ambas coincidan bin a bin.
class BandLayoutMirror {
public:
    bool Refresh(const core::VisualizerConfig& cfg, int sample_rate) {
        core::BandConfig bands = cfg.bands;
        core::ValidateBandConfig(bands, cfg.attack_ms, cfg.release_ms);
        const bool changed = (bands != bands_) || sample_rate != layout_.sample_rate;
        if (changed && sample_rate > 0) {
            bands_ = bands;
            layout_ = core::BuildBandLayout(bands_, sample_rate, core::FFT_SIZE);
            ++version_;
            attack_.clear();
            release_.clear();
            for (const auto& b : layout_.bands) {
                attack_.push_back(b.attack_ms);
                release_.push_back(b.release_ms);
            }
            return true;
        }
        return false;
    }
    const core::BandLayout& layout() const { return layout_; }
    uint64_t version() const { return version_; }
    const std::vector<float>& attack_ms() const { return attack_; }
    const std::vector<float>& release_ms() const { return release_; }

private:
    core::BandConfig bands_;
    core::BandLayout layout_;
    uint64_t version_ = 0;
    std::vector<float> attack_;
    std::vector<float> release_;
};

void BandLevels(const core::AnalysisFrame& frame, const core::BandLayout& layout, const core::VisualizerConfig& cfg, std::vector<float>& out) {
    const int count = std::min(frame.band_count(), layout.count());
    out.resize(count);
    const float range_db = std::max(1.0f, cfg.dynamic_range_db);
    for (int b = 0; b < count; ++b) {
        const float gain_db = 20.0f * std::log10(std::max(1e-6f, layout.bands[b].gain));
        out[b] = std::clamp((frame.band_peak_db[b] + gain_db + range_db) / range_db, 0.0f, 1.0f);
    }
}

platform::SystemBackdrop BackdropFor(const std::string& material) {
    if (material == "mica") return platform::SystemBackdrop::Mica;
    if (material == "acrylic_system") return platform::SystemBackdrop::Acrylic;
    return platform::SystemBackdrop::None;
}

} // namespace

void RenderThread(core::VisualizerData& vis, core::SharedConfigData& shared_config, core::AudioData& audio) {
    std::cout << "Render: hilo iniciado con OpenGL 3.3 Core." << std::endl;

    core::VisualizerConfig cfg;
    {
        std::lock_guard<std::mutex> lock(shared_config.mtx);
        cfg = shared_config.config;
    }

    // Preferencias del sistema (docs/12, secciones 10 y 11). Se leen al arrancar.
    const platform::SystemEffectsPreference system_pref = platform::ReadSystemEffectsPreference();
    const platform::SystemBackdrop backdrop = BackdropFor(cfg.material);

    if (!glfwInit()) {
        std::cerr << "Render: no se pudo inicializar GLFW." << std::endl;
        return;
    }
    WindowOptions options;
    options.title = "Audio Visualizer 3.0";
    options.vsync = cfg.vsync;
    options.transparent_framebuffer = (backdrop != platform::SystemBackdrop::None);
    GLFWwindow* window = CreateMainWindow(options);
    if (!window) {
        glfwTerminate();
        return;
    }
    const platform::WindowEffectsResult effects = platform::ApplyWindowEffects(window, backdrop);
    if (!effects.message.empty()) std::cout << "Render: " << effects.message << std::endl;
    const bool system_backdrop_active = effects.backdrop && glfwGetWindowAttrib(window, GLFW_TRANSPARENT_FRAMEBUFFER) == GLFW_TRUE;

    // Dear ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    {
        float sx = 1.0f, sy = 1.0f;
        glfwGetWindowContentScale(window, &sx, &sy);
        ImGui::GetIO().FontGlobalScale = std::max(1.0f, sx);
    }
    ui::ApplyObsidianTheme();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Recursos compartidos y modos.
    FullscreenQuad quad;
    quad.Init();
    DataTextures textures;
    textures.Init();
    BarSpectrum bar_spectrum;
    SpectrumDynamics dynamics;
    SpectrumDynamics band_dynamics;
    BandLayoutMirror bands;
    std::vector<float> band_levels;
    BandWaveformTexture band_waves;
    band_waves.Init();

    // Post-procesado: escena a textura, fundido entre modos y material del panel (docs/12).
    RenderTarget scene, scene_previous;
    scene.Init(1024, 600);
    scene_previous.Init(1024, 600);
    Presenter presenter;
    presenter.Init();
    PanelMaterial panel_material;
    panel_material.Init();

    std::array<std::unique_ptr<IVisualMode>, core::MODE_COUNT> modes;
    modes[core::MODE_BARS] = std::make_unique<BarsMode>();
    modes[core::MODE_RADIAL] = std::make_unique<RadialMode>();
    modes[core::MODE_WAVEFORM] = std::make_unique<WaveformMode>();
    modes[core::MODE_WATERFALL] = std::make_unique<WaterfallMode>();
    modes[core::MODE_BAND_METERS] = std::make_unique<BandMetersMode>();
    modes[core::MODE_STACKED_OSCILLOSCOPE] = std::make_unique<StackedOscilloscopeMode>();
    for (auto& mode : modes) {
        if (!mode->Init()) std::cerr << "Render: el modo " << mode->Name() << " no pudo inicializarse." << std::endl;
    }

    // Estado del bucle.
    uint64_t seen_sequence = 0;
    uint64_t title_sequence = 0;
    int title_frames = 0;
    double last_time = glfwGetTime();
    double title_time = last_time;
    int current_mode = std::clamp(cfg.visual_mode, 0, static_cast<int>(core::MODE_COUNT) - 1);
    int previous_mode = current_mode;
    double mode_change_time = -1e9;

    ui::HudState hud;
    ui::Telemetry telemetry;
    ui::Transition hud_fade;
    ui::AppearanceStatus appearance;
    appearance.system_transparency = system_pref.transparency;
    appearance.system_animations = system_pref.animations;
    appearance.system_backdrop_active = system_backdrop_active;
    appearance.system_backdrop_supported = platform::SupportsSystemBackdrop();
    appearance.effects_message = effects.message;
    KeyboardShortcuts keys;
    FrameLimiter limiter;
    telemetry.monitor_hz = RefreshRateForWindow(window);
    limiter.Start(cfg.vsync, cfg.max_fps, telemetry.monitor_hz, last_time);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::clamp(now - last_time, 0.0, 0.1));
        last_time = now;

        keys.Poll(window, hud, cfg);

        // Efectos permitidos este cuadro: configuración y, si se respeta, preferencia del sistema.
        const bool effects_allowed = cfg.material == "acrylic_app" && (!cfg.respect_system_effects || system_pref.transparency);
        const bool animations_allowed = cfg.animations && (!cfg.respect_system_effects || system_pref.animations);
        appearance.effects_active = effects_allowed;
        appearance.animations_active = animations_allowed;
        hud_fade.Configure(kHudFadeSeconds, animations_allowed);
        hud_fade.SetTarget(hud.visible, now);
        const float hud_alpha = hud_fade.value(now);

        // Cambio de modo con fundido cruzado.
        const int mode_index = std::clamp(cfg.visual_mode, 0, static_cast<int>(core::MODE_COUNT) - 1);
        if (mode_index != current_mode) {
            previous_mode = current_mode;
            current_mode = mode_index;
            mode_change_time = animations_allowed ? now : -1e9;
        }
        const float crossfade_t = static_cast<float>(std::clamp((now - mode_change_time) / kModeCrossfadeSeconds, 0.0, 1.0));
        const bool crossfading = crossfade_t < 1.0f && previous_mode != current_mode;

        // Pedir al análisis las ondas por banda solo mientras un modo que las usa está activo.
        vis.band_waveforms_requested.store(modes[current_mode]->NeedsBandWaveforms() || (crossfading && modes[previous_mode]->NeedsBandWaveforms()), std::memory_order_relaxed);

        // Última trama de análisis, sin bloqueo ni copia.
        const core::AnalysisFrame& frame = vis.analysis.Read();
        const bool has_new_frame = frame.valid() && frame.sequence != seen_sequence;
        if (has_new_frame) seen_sequence = frame.sequence;

        int fbw = 0, fbh = 0;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        fbw = std::max(1, fbw);
        fbh = std::max(1, fbh);
        const int num_bars = fbw;

        if (frame.valid()) bands.Refresh(cfg, frame.sample_rate);

        const core::BandLayout* bars_layout = cfg.bands.bars_inherit_dynamics ? &bands.layout() : nullptr;
        if (has_new_frame || static_cast<int>(bar_spectrum.values().size()) != num_bars) {
            bar_spectrum.Update(frame, num_bars, cfg, bars_layout, bands.version());
        }
        const bool per_bar = !bar_spectrum.attack_ms().empty() && static_cast<int>(bar_spectrum.attack_ms().size()) == num_bars;
        dynamics.Update(bar_spectrum.values(), num_bars, cfg,
                        per_bar ? bar_spectrum.attack_ms().data() : nullptr,
                        per_bar ? bar_spectrum.release_ms().data() : nullptr, dt);

        if (frame.valid()) BandLevels(frame, bands.layout(), cfg, band_levels);
        const int band_count = static_cast<int>(band_levels.size());
        const bool per_band = static_cast<int>(bands.attack_ms().size()) == band_count && band_count > 0;
        band_dynamics.Update(band_levels, band_count, cfg,
                             per_band ? bands.attack_ms().data() : nullptr,
                             per_band ? bands.release_ms().data() : nullptr, dt);

        textures.UploadSpectrum(dynamics.heights());
        if (has_new_frame) {
            textures.UploadWaveform(frame.mix_waveform);
            textures.PushWaterfallRow(bar_spectrum.values(), cfg.amplitude_factor);
            if (frame.band_waveform_valid) band_waves.Append(frame);
        }

        // Escena a textura. Con material del sistema el fondo queda transparente (alfa 0) para que
        // Windows componga Mica o Acrílico detrás; si no, opaco.
        const float bg_alpha = system_backdrop_active ? 0.0f : 1.0f;
        const RenderContext ctx{ fbw, fbh, now, num_bars, cfg, textures, dynamics, quad, bands.layout(), band_dynamics, band_waves };
        scene.Resize(fbw, fbh);
        scene.Bind();
        glDisable(GL_BLEND);
        glClearColor(0.06f, 0.06f, 0.08f, bg_alpha);
        glClear(GL_COLOR_BUFFER_BIT);
        modes[current_mode]->Render(ctx);
        if (crossfading) {
            scene_previous.Resize(fbw, fbh);
            scene_previous.Bind();
            glClearColor(0.06f, 0.06f, 0.08f, bg_alpha);
            glClear(GL_COLOR_BUFFER_BIT);
            modes[previous_mode]->Render(ctx);
        }

        // Presentación en pantalla.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        if (crossfading) {
            presenter.Draw(scene_previous.texture(), 1.0f, quad);
            presenter.Draw(scene.texture(), ui::EaseDecelerate(crossfade_t), quad);
        }
        else {
            presenter.Draw(scene.texture(), 1.0f, quad);
        }

        // Material del panel: solo si el panel está visible o desvaneciéndose.
        const bool panel_showing = hud.visible || hud_alpha > 0.001f;
        if (panel_showing && effects_allowed) {
            MaterialParams params;
            params.opacity = std::clamp(cfg.material_opacity, 0.6f, 0.95f);
            panel_material.set_params(params);
            panel_material.Prepare(scene.texture(), fbw, fbh, quad);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, fbw, fbh);
        }
        else {
            panel_material.Skip();
        }

        // Interfaz.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        modes[current_mode]->DrawOverlay(ctx);
        telemetry.num_bars = num_bars;
        telemetry.limiter_active = limiter.active();
        ui::HudContext hud_ctx{ cfg, shared_config, vis, audio, telemetry, frame, bands.layout(), appearance,
                                panel_material.ready() ? &panel_material : nullptr, hud_alpha, now };
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
            const double ups = static_cast<double>(seen_sequence - title_sequence) / elapsed;
            telemetry.Record(static_cast<float>(fps), static_cast<float>(ups));
            telemetry.monitor_hz = RefreshRateForWindow(window);
            limiter.UpdateEverySecond(fps, telemetry.monitor_hz, cfg.max_fps, glfwGetTime());
            telemetry.limiter_active = limiter.active();
            glfwSetWindowTitle(window, ui::BuildWindowTitle(modes[current_mode]->Name(), telemetry, audio.sample_rate.load()).c_str());
            title_frames = 0;
            title_time = now;
            title_sequence = seen_sequence;
        }
    }

    // Limpieza en orden inverso.
    limiter.Stop();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    for (auto& mode : modes) mode->Shutdown();
    panel_material.Shutdown();
    presenter.Shutdown();
    scene_previous.Shutdown();
    scene.Shutdown();
    band_waves.Shutdown();
    textures.Shutdown();
    quad.Shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
}

} // namespace render
