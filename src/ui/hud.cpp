#include "ui/hud.h"

#include <vector>
#include <cmath>
#include <imgui.h>

#include "ui/theme.h"
#include "audio/device_enumerator.h"

namespace ui {
namespace {

const ImVec4 kAccent(0.35f, 0.75f, 1.0f, 1.0f);
const ImVec4 kOk(0.2f, 0.9f, 0.4f, 1.0f);

void PublishConfig(HudContext& ctx) {
    std::lock_guard<std::mutex> lock(ctx.shared_config.mtx);
    ctx.shared_config.config = ctx.cfg;
    ctx.shared_config.version.fetch_add(1);
}

void TabModes(HudState& state, HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Seleccion de Modo Visual:");

    if (ImGui::RadioButton("1. Barras de Espectro (Ecualizador)", cfg.visual_mode == core::MODE_BARS)) cfg.visual_mode = core::MODE_BARS;
    ImGui::SameLine(); HelpMarker("Frecuencias en columnas verticales con marcadores de pico.");
    if (ImGui::RadioButton("2. Radial / Circular (Anillo Reactivo)", cfg.visual_mode == core::MODE_RADIAL)) cfg.visual_mode = core::MODE_RADIAL;
    ImGui::SameLine(); HelpMarker("Mapeo polar en anillo con nucleo pulsante al ritmo de los graves.");
    if (ImGui::RadioButton("3. Osciloscopio / Forma de Onda", cfg.visual_mode == core::MODE_WAVEFORM)) cfg.visual_mode = core::MODE_WAVEFORM;
    ImGui::SameLine(); HelpMarker("Muestras crudas de audio en tiempo real con haz tipo CRT.");
    if (ImGui::RadioButton("4. Espectrograma Cascada 2D (Waterfall)", cfg.visual_mode == core::MODE_WATERFALL)) cfg.visual_mode = core::MODE_WATERFALL;
    ImGui::SameLine(); HelpMarker("Historial de frecuencias desplazandose hacia abajo con paleta termica.");

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Mecanica de Picos (Peak-Hold en Barras):");
    ImGui::Checkbox("Habilitar Marcadores de Pico", &cfg.peak_hold_enabled);
    ImGui::SameLine(); HelpMarker("Sostiene una marca en el valor mas alto antes de descender por gravedad.");

    if (!cfg.peak_hold_enabled) ImGui::BeginDisabled();
    ImGui::SliderFloat("Retardo Pico", &cfg.peak_hold_time_ms, 50.0f, 1000.0f, "%.0f ms");
    ImGui::SameLine(); HelpMarker("Milisegundos que el marcador se mantiene en la cima antes de caer.");
    ImGui::SliderFloat("Velocidad de Caida", &cfg.peak_decay_speed, 0.5f, 5.0f, "%.1fx");
    ImGui::SameLine(); HelpMarker("Rapidez de descenso de los picos cuando se agota el retardo.");
    if (!cfg.peak_hold_enabled) ImGui::EndDisabled();
    (void)state;
}

void TabDsp(HudState& state, HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    ImGui::Spacing();

    std::string current_name;
    std::vector<core::AudioDeviceInfo> devices;
    {
        std::lock_guard<std::mutex> lock(ctx.vis.dev_mtx);
        current_name = ctx.vis.current_device_name;
        devices = ctx.vis.devices;
    }

    ImGui::BeginChild("AudioInfoCard", ImVec2(0, 68), true);
    ImGui::TextColored(kOk, "DISPOSITIVO DE AUDIO ACTIVO");
    ImGui::TextUnformatted(current_name.empty() ? "Dispositivo por defecto de Windows" : current_name.c_str());
    ImGui::TextDisabled("Frecuencia: %d Hz | Modo: Loopback WASAPI", ctx.audio.sample_rate.load());
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Cambiar Dispositivo de Reproduccion:");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110);
    if (ImGui::BeginCombo("##DeviceCombo", current_name.empty() ? "Predeterminado" : current_name.c_str())) {
        for (const auto& dev : devices) {
            const bool selected = (dev.name == current_name);
            const std::string label = dev.name + (dev.is_default ? " [Predeterminado]" : "");
            if (ImGui::Selectable(label.c_str(), selected)) {
                std::lock_guard<std::mutex> lock(ctx.vis.dev_mtx);
                ctx.vis.requested_device_id = dev.id;
                ctx.vis.device_change_pending.store(true);
                cfg.selected_device_name = dev.name;
                state.Toast("Conmutando a: " + dev.name, ctx.now);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refrescar", ImVec2(100, 0))) {
        std::vector<core::AudioDeviceInfo> refreshed = audio::EnumerateAudioDevices();
        std::lock_guard<std::mutex> lock(ctx.vis.dev_mtx);
        ctx.vis.devices = refreshed;
        state.Toast("Lista de dispositivos actualizada", ctx.now);
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Parametros de Respuesta Temporal:");
    ImGui::SliderFloat("Ganancia", &cfg.amplitude_factor, 0.1f, 3.0f, "%.2fx");
    ImGui::SameLine(); HelpMarker("Multiplicador vertical del espectro.");
    ImGui::SliderFloat("Ataque", &cfg.attack_ms, 1.0f, 80.0f, "%.0f ms");
    ImGui::SameLine(); HelpMarker("Tiempo de respuesta al subir. Valores bajos reaccionan al instante.");
    ImGui::SliderFloat("Caida (Decay)", &cfg.release_ms, 20.0f, 500.0f, "%.0f ms");
    ImGui::SameLine(); HelpMarker("Tiempo de descenso. Valores altos suavizan la caida.");
    ImGui::SliderFloat("Rango Dinamico", &cfg.dynamic_range_db, 20.0f, 100.0f, "%.0f dB");
    ImGui::SameLine(); HelpMarker("0 dBFS arriba, -rango abajo.");

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Distribucion de Frecuencias:");
    int scale_idx = (cfg.frequency_scale == "log") ? 1 : 0;
    if (ImGui::RadioButton("Lineal (Hz constantes)", &scale_idx, 0)) cfg.frequency_scale = "linear";
    ImGui::SameLine();
    if (ImGui::RadioButton("Logaritmica (Octavas)", &scale_idx, 1)) cfg.frequency_scale = "log";

    if (cfg.frequency_scale == "log") {
        ImGui::DragFloatRange2("Rango Hz", &cfg.min_frequency, &cfg.max_frequency, 5.0f, 10.0f, 22000.0f, "Min: %.0f Hz", "Max: %.0f Hz");
        ImGui::SameLine(); HelpMarker("Limites de frecuencia para la distribucion por octavas.");
    }
    else {
        ImGui::SliderFloat("Hz por Barra", &cfg.bin_grouping_factor, 2.0f, 50.0f, "%.1f Hz");
        ImGui::SameLine(); HelpMarker("Ancho de banda de cada barra en hercios.");
    }
}

void TabColors(HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Colores Personalizados:");
    if (cfg.base_color_rgb.size() >= 3) {
        ImGui::ColorEdit3("Color Base", cfg.base_color_rgb.data(), ImGuiColorEditFlags_DisplayRGB);
        ImGui::SameLine(); HelpMarker("Base de las barras y anillo central en modo radial.");
    }
    if (cfg.peak_color_rgb.size() >= 3) {
        ImGui::ColorEdit3("Color Picos / Halo", cfg.peak_color_rgb.data(), ImGuiColorEditFlags_DisplayRGB);
        ImGui::SameLine(); HelpMarker("Marcadores de pico y resplandores exteriores.");
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Presets Tematicos:");

    struct Preset { const char* name; float base[3]; float peak[3]; };
    static const Preset presets[] = {
        { "Cyberpunk Neon",  { 0.05f, 0.75f, 0.95f }, { 1.00f, 0.15f, 0.65f } },
        { "Matrix Emerald",  { 0.10f, 0.85f, 0.35f }, { 0.85f, 1.00f, 0.20f } },
        { "Solar Amber",     { 0.85f, 0.25f, 0.10f }, { 1.00f, 0.85f, 0.20f } },
        { "Deep Amethyst",   { 0.60f, 0.15f, 0.90f }, { 0.10f, 0.80f, 1.00f } },
        { "Arctic Ice",      { 0.75f, 0.90f, 1.00f }, { 0.00f, 0.65f, 0.95f } },
        { "Classic Crimson", { 0.65f, 0.15f, 0.15f }, { 1.00f, 0.85f, 0.20f } },
    };
    for (int i = 0; i < 6; ++i) {
        if (i % 3 != 0) ImGui::SameLine();
        if (ImGui::Button(presets[i].name, ImVec2(130, 28))) {
            cfg.base_color_rgb = { presets[i].base[0], presets[i].base[1], presets[i].base[2] };
            cfg.peak_color_rgb = { presets[i].peak[0], presets[i].peak[1], presets[i].peak[2] };
        }
    }
}

void TabTelemetry(HudState& state, HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    const Telemetry& t = ctx.telemetry;
    ImGui::Spacing();

    ImGui::TextColored(kAccent, "Rendimiento en Tiempo Real:");
    ImGui::PlotLines("##fps_plot", t.fps_history, Telemetry::HISTORY, t.history_offset, "Cuadros por Segundo (ultimos 60 s)", 0.0f, 240.0f, ImVec2(0, 65));

    ImGui::Columns(2, "TelemetriaCols", false);
    ImGui::Text("FPS Actuales:"); ImGui::NextColumn();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "%.1f FPS", t.last_fps); ImGui::NextColumn();
    ImGui::Text("Refresco Monitor:"); ImGui::NextColumn();
    ImGui::Text("%d Hz (%s)", t.monitor_hz, t.limiter_active ? "Limitador activo" : "VSync del driver"); ImGui::NextColumn();
    ImGui::Text("Espectros / seg:"); ImGui::NextColumn();
    ImGui::Text("%.1f esp/s", t.last_ups); ImGui::NextColumn();
    ImGui::Text("Resolucion Barras:"); ImGui::NextColumn();
    ImGui::Text("%d px", t.num_bars); ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Trama de Analisis (ultima publicada):");
    const core::AnalysisFrame& f = ctx.frame;
    if (f.valid()) {
        const float rms_db = 20.0f * std::log10(f.rms + 1e-9f);
        const float peak_db = 20.0f * std::log10(f.peak + 1e-9f);
        ImGui::Columns(2, "AnalisisCols", false);
        ImGui::Text("Secuencia:"); ImGui::NextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(f.sequence)); ImGui::NextColumn();
        ImGui::Text("FFT / salto / bins:"); ImGui::NextColumn(); ImGui::Text("%d / %d / %d", f.fft_size, f.hop_size, static_cast<int>(f.magnitude.size())); ImGui::NextColumn();
        ImGui::Text("Resolucion por bin:"); ImGui::NextColumn(); ImGui::Text("%.2f Hz", f.bin_resolution_hz()); ImGui::NextColumn();
        ImGui::Text("RMS:"); ImGui::NextColumn(); ImGui::Text("%.1f dBFS", rms_db); ImGui::NextColumn();
        ImGui::Text("Pico:"); ImGui::NextColumn(); ImGui::Text("%.1f dBFS", peak_db); ImGui::NextColumn();
        ImGui::Text("Flujo espectral:"); ImGui::NextColumn(); ImGui::Text("%.3f", f.spectral_flux); ImGui::NextColumn();
        ImGui::Text("Centroide:"); ImGui::NextColumn(); ImGui::Text("%.0f Hz", f.spectral_centroid_hz); ImGui::NextColumn();
        ImGui::Columns(1);
    }
    else {
        ImGui::TextDisabled("Sin tramas todavia (esperando audio).");
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Sincronizacion de Cuadros:");
    ImGui::Checkbox("Sincronia Vertical (VSync)", &cfg.vsync);
    ImGui::SameLine(); HelpMarker("Sincroniza el dibujo con el refresco de pantalla.");
    ImGui::SliderInt("Limite Max FPS", &cfg.max_fps, 0, 360, cfg.max_fps == 0 ? "Auto (Monitor Hz)" : "%d FPS");
    ImGui::SameLine(); HelpMarker("0 = tasa de refresco del monitor.");

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Persistencia de Ajustes:");
    if (ImGui::Button("Guardar en config.json", ImVec2(180, 32))) {
        PublishConfig(ctx);
        if (core::SaveConfig(ctx.shared_config, "config.json")) {
            state.Toast("Configuracion guardada", ctx.now);
        }
        else {
            state.Toast("No se pudo escribir config.json", ctx.now);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Restaurar por Defecto", ImVec2(160, 32))) {
        cfg = core::VisualizerConfig();
        state.Toast("Valores restaurados por defecto", ctx.now);
    }

    if (ctx.now - state.toast_time < 3.0 && !state.toast.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(kOk, "OK: %s", state.toast.c_str());
    }
}

} // namespace

void DrawHud(HudState& state, HudContext& ctx) {
    if (!state.visible) return;

    ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Audio Visualizer 2.0  -  Panel de Control", &state.visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextColored(kAccent, "ESTADO:");
        ImGui::SameLine();
        ImGui::Text("%s", core::VisualizerModeName(ctx.cfg.visual_mode));
        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::TextDisabled("Atajo: Tab / H");
        ImGui::Spacing();

        if (ImGui::BeginTabBar("VisualizerTabBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem(" Modos y Efectos ")) { TabModes(state, ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" DSP y Dinamica ")) { TabDsp(state, ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" Color y Temas ")) { TabColors(ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" Telemetria y Sistema ")) { TabTelemetry(state, ctx); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    // Publicar los cambios en vivo al hilo de análisis.
    PublishConfig(ctx);
}

} // namespace ui
