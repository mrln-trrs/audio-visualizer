#include "ui/hud.h"

#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <imgui.h>

#include "ui/theme.h"
#include "audio/device_enumerator.h"

namespace ui {
namespace {

const ImVec4 kAccent(0.35f, 0.75f, 1.0f, 1.0f);
const ImVec4 kOk(0.2f, 0.9f, 0.4f, 1.0f);
const ImVec4 kWarn(1.0f, 0.67f, 0.3f, 1.0f);

void PublishConfig(HudContext& ctx) {
    std::lock_guard<std::mutex> lock(ctx.shared_config.mtx);
    ctx.shared_config.config = ctx.cfg;
    ctx.shared_config.version.fetch_add(1);
}

// ---------------------------------------------------------------- Modos y efectos
void TabModes(HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Seleccion de Modo Visual:");

    struct Entry { const char* label; int mode; const char* help; };
    static const Entry entries[] = {
        { "1. Barras de Espectro (Ecualizador)", core::MODE_BARS, "Frecuencias en columnas verticales con marcadores de pico." },
        { "2. Radial / Circular (Anillo Reactivo)", core::MODE_RADIAL, "Mapeo polar en anillo con nucleo pulsante al ritmo de los graves." },
        { "3. Osciloscopio / Forma de Onda", core::MODE_WAVEFORM, "Muestras crudas de audio en tiempo real con haz tipo CRT." },
        { "4. Espectrograma Cascada 2D (Waterfall)", core::MODE_WATERFALL, "Historial de frecuencias desplazandose hacia abajo con paleta termica." },
        { "5. Osciloscopio Apilado por Bandas", core::MODE_STACKED_OSCILLOSCOPE, "Una traza por banda reconstruida por IFFT enmascarada y, debajo, la mezcla. La suma de las trazas es exactamente la mezcla. Maximo 32 bandas." },
        { "6. Medidores por Banda", core::MODE_BAND_METERS, "Una columna por banda configurada, con nombre, rango, nivel en dB y pico." },
    };
    for (const auto& e : entries) {
        if (ImGui::RadioButton(e.label, cfg.visual_mode == e.mode)) cfg.visual_mode = e.mode;
        ImGui::SameLine(); HelpMarker(e.help);
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Mecanica de Picos (Peak-Hold):");
    ImGui::Checkbox("Habilitar Marcadores de Pico", &cfg.peak_hold_enabled);
    ImGui::SameLine(); HelpMarker("Sostiene una marca en el valor mas alto antes de descender por gravedad.");

    if (!cfg.peak_hold_enabled) ImGui::BeginDisabled();
    ImGui::SliderFloat("Retardo Pico", &cfg.peak_hold_time_ms, 50.0f, 1000.0f, "%.0f ms");
    ImGui::SameLine(); HelpMarker("Milisegundos que el marcador se mantiene en la cima antes de caer.");
    ImGui::SliderFloat("Velocidad de Caida", &cfg.peak_decay_speed, 0.5f, 5.0f, "%.1fx");
    ImGui::SameLine(); HelpMarker("Rapidez de descenso de los picos cuando se agota el retardo.");
    if (!cfg.peak_hold_enabled) ImGui::EndDisabled();
}

// ---------------------------------------------------------------- DSP y dispositivos
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
    ImGui::TextColored(kAccent, "Respuesta Temporal Global:");
    ImGui::SliderFloat("Ganancia", &cfg.amplitude_factor, 0.1f, 3.0f, "%.2fx");
    ImGui::SameLine(); HelpMarker("Multiplicador vertical del espectro.");
    ImGui::SliderFloat("Ataque", &cfg.attack_ms, 1.0f, 80.0f, "%.0f ms");
    ImGui::SameLine(); HelpMarker("Tiempo de respuesta al subir, para las barras que no heredan de una banda.");
    ImGui::SliderFloat("Caida (Decay)", &cfg.release_ms, 20.0f, 500.0f, "%.0f ms");
    ImGui::SameLine(); HelpMarker("Tiempo de descenso, para las barras que no heredan de una banda.");
    ImGui::SliderFloat("Rango Dinamico", &cfg.dynamic_range_db, 20.0f, 100.0f, "%.0f dB");
    ImGui::SameLine(); HelpMarker("0 dBFS arriba, -rango abajo.");

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Resolucion Variable por Rangos:");
    core::AnalysisConfig& an = cfg.analysis;
    bool an_changed = ImGui::Checkbox("FFT larga en graves y corta en agudos", &an.multi_resolution);
    ImGui::SameLine(); HelpMarker("Resolucion en frecuencia y latencia estan ligadas (principio de incertidumbre, documento 10). La FFT larga separa graves cercanos a cambio de retraso; la corta responde antes en agudos.");
    if (an.multi_resolution) {
        static const int kSizes[] = { 2048, 4096, 8192, 16384 };
        static const char* kSizeLabels[] = { "2048 (igual que la base)", "4096", "8192", "16384" };
        int li = 2;
        for (int i = 0; i < 4; ++i) if (an.low_band_fft_size == kSizes[i]) li = i;
        if (ImGui::Combo("FFT graves", &li, kSizeLabels, 4)) { an.low_band_fft_size = kSizes[li]; an_changed = true; }
        if (ImGui::SliderFloat("Graves hasta", &an.low_band_max_hz, 60.0f, 1000.0f, "%.0f Hz")) an_changed = true;
        static const int kHigh[] = { 256, 512, 1024, 2048 };
        static const char* kHighLabels[] = { "256", "512", "1024", "2048 (igual que la base)" };
        int hi = 1;
        for (int i = 0; i < 4; ++i) if (an.high_band_fft_size == kHigh[i]) hi = i;
        if (ImGui::Combo("FFT agudos", &hi, kHighLabels, 4)) { an.high_band_fft_size = kHigh[hi]; an_changed = true; }
        if (ImGui::SliderFloat("Agudos desde", &an.high_band_min_hz, 500.0f, 12000.0f, "%.0f Hz")) an_changed = true;
        const int sr = std::max(1, ctx.audio.sample_rate.load());
        const float low_ms = 1000.0f * (an.low_band_fft_size - core::FFT_SIZE) / (2.0f * sr);
        const float high_ms = 1000.0f * (core::FFT_SIZE - an.high_band_fft_size) / (2.0f * sr);
        const float low_res = static_cast<float>(sr) / an.low_band_fft_size;
        ImGui::TextColored(kWarn, "Graves: %.2f Hz por bin, +%.0f ms de latencia. Agudos: -%.0f ms de latencia.", low_res, low_ms, high_ms);
    }
    if (an_changed) core::ValidateAnalysisConfig(an);

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Distribucion de Frecuencias (modo Barras):");
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

// ---------------------------------------------------------------- Bandas
void TabBands(HudState& state, HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    core::BandConfig& b = cfg.bands;
    bool changed = false;
    ImGui::Spacing();

    ImGui::TextColored(kAccent, "Particion del Espectro:");
    static const char* kModes[] = { "manual", "octaves", "linear", "per_bin" };
    static const char* kModeLabels[] = { "Manual (cortes en Hz)", "Octavas", "Lineal (anchura constante)", "Por bin de la FFT" };
    int mode_idx = 0;
    for (int i = 0; i < 4; ++i) if (b.mode == kModes[i]) mode_idx = i;
    if (ImGui::Combo("Modo", &mode_idx, kModeLabels, 4)) { b.mode = kModes[mode_idx]; changed = true; }
    ImGui::SameLine(); HelpMarker("Manual: cortes editables con nombre y color por banda. Octavas: reparto musical. Lineal: bandas de igual anchura. Por bin: una banda por bin, solo para metricas.");

    if (ImGui::DragFloatRange2("Rango", &b.f_min, &b.f_max, 5.0f, 1.0f, 24000.0f, "Min: %.0f Hz", "Max: %.0f Hz")) changed = true;
    if (b.mode == "octaves") {
        if (ImGui::SliderInt("Divisiones por octava", &b.divisions_per_octave, 1, 12)) changed = true;
    }
    else if (b.mode == "linear") {
        if (ImGui::SliderInt("Numero de bandas", &b.linear_band_count, 1, 64)) changed = true;
    }
    else if (b.mode == "manual") {
        ImGui::TextDisabled("Cortes interiores (Hz):");
        for (size_t i = 0; i < b.cuts_hz.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const float lo = (i == 0 ? b.f_min : b.cuts_hz[i - 1]) + 1.0f;
            const float hi = (i + 1 < b.cuts_hz.size() ? b.cuts_hz[i + 1] : b.f_max) - 1.0f;
            ImGui::SetNextItemWidth(140);
            if (ImGui::DragFloat("##cut", &b.cuts_hz[i], 2.0f, lo, hi, "%.0f Hz")) { b.cuts_hz[i] = std::clamp(b.cuts_hz[i], lo, hi); changed = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("Quitar")) { b.cuts_hz.erase(b.cuts_hz.begin() + i); changed = true; ImGui::PopID(); break; }
            ImGui::PopID();
        }
        if (b.cuts_hz.size() < 63 && ImGui::Button("Anadir corte")) {
            const float last = b.cuts_hz.empty() ? b.f_min : b.cuts_hz.back();
            b.cuts_hz.push_back(std::sqrt(last * b.f_max)); // media geometrica hasta f_max
            changed = true;
        }
    }
    else {
        ImGui::TextDisabled("Una banda por bin entre f_min y f_max. Nombres y colores se generan.");
    }

    ImGui::Separator();
    if (ImGui::Checkbox("Las barras heredan ataque y caida de su banda", &b.bars_inherit_dynamics)) changed = true;
    ImGui::SameLine(); HelpMarker("En el modo Barras, cada barra usa las constantes de la banda que contiene su frecuencia central.");
    if (ImGui::Button("Restaurar preset de 7 bandas")) { b = core::BandConfig(); changed = true; state.Toast("Preset de 7 bandas restaurado", ctx.now); }

    if (changed) core::ValidateBandConfig(b, cfg.attack_ms, cfg.release_ms);

    // Tabla por banda con la partición real (bins) del render.
    ImGui::Separator();
    ImGui::Spacing();
    const core::BandLayout& layout = ctx.bands;
    const int count = layout.count();
    ImGui::TextColored(kAccent, "Bandas (%d) a %.2f Hz por bin:", count, layout.bin_resolution_hz);
    int narrow = 0;
    for (const auto& band : layout.bands) if (band.too_narrow) ++narrow;
    if (narrow > 0) {
        ImGui::TextColored(kWarn, "%d banda(s) mas estrechas que un bin: su nivel es una interpolacion. Resolverlas exige una FFT mayor (mas latencia).", narrow);
    }

    if (count > 64) {
        ImGui::TextDisabled("Demasiadas bandas para editar una a una; se muestran las metricas agregadas.");
        return;
    }
    const int rows = std::min<int>(count, static_cast<int>(b.names.size()));
    if (ImGui::BeginTable("BandTable", 6, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Nombre");
        ImGui::TableSetupColumn("Rango / bins");
        ImGui::TableSetupColumn("Color");
        ImGui::TableSetupColumn("Ataque");
        ImGui::TableSetupColumn("Caida");
        ImGui::TableSetupColumn("Ganancia");
        ImGui::TableHeadersRow();
        for (int i = 0; i < rows; ++i) {
            const core::BandDefinition& def = layout.bands[i];
            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableNextColumn();
            char name_buf[48];
            std::snprintf(name_buf, sizeof(name_buf), "%s", b.names[i].c_str());
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##name", name_buf, sizeof(name_buf))) { b.names[i] = name_buf; changed = true; }

            ImGui::TableNextColumn();
            if (def.too_narrow) ImGui::TextColored(kWarn, "%.0f-%.0f Hz (%d bin)", def.f_low_hz, def.f_high_hz, def.bin_high - def.bin_low);
            else ImGui::Text("%.0f-%.0f Hz (%d bins)", def.f_low_hz, def.f_high_hz, def.bin_high - def.bin_low);

            ImGui::TableNextColumn();
            if (ImGui::ColorEdit3("##color", b.colors_rgb[i].data(), ImGuiColorEditFlags_NoInputs)) changed = true;

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##attack", &b.attack_ms[i], 0.5f, 1.0f, 500.0f, "%.0f ms")) changed = true;

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##release", &b.release_ms[i], 1.0f, 10.0f, 2000.0f, "%.0f ms")) changed = true;

            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##gain", &b.gain[i], 0.01f, 0.0f, 4.0f, "%.2fx")) changed = true;

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // Métricas en vivo de la trama.
    if (ctx.frame.valid() && ctx.frame.band_count() == count && count <= 64) {
        ImGui::Spacing();
        ImGui::TextColored(kAccent, "Metricas por banda (ultima trama):");
        if (ImGui::BeginTable("BandMetrics", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Banda");
            ImGui::TableSetupColumn("Pico dBFS");
            ImGui::TableSetupColumn("RMS dBFS");
            ImGui::TableSetupColumn("Energia %");
            ImGui::TableHeadersRow();
            const float total = std::max(1e-12f, ctx.frame.total_energy);
            for (int i = 0; i < count; ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextUnformatted(layout.bands[i].name.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%.1f", ctx.frame.band_peak_db[i]);
                ImGui::TableNextColumn(); ImGui::Text("%.1f", 20.0f * std::log10(ctx.frame.band_rms[i] + 1e-9f));
                ImGui::TableNextColumn(); ImGui::Text("%.1f", 100.0f * ctx.frame.band_energy[i] / total);
            }
            ImGui::EndTable();
        }
    }

    if (changed) core::ValidateBandConfig(b, cfg.attack_ms, cfg.release_ms);
}

// ---------------------------------------------------------------- Colores
void TabAppearance(HudState& state, HudContext& ctx) {
    core::VisualizerConfig& cfg = ctx.cfg;
    const AppearanceStatus& ap = ctx.appearance;
    ImGui::Spacing();
    ImGui::TextColored(kAccent, "Material del Panel y de la Ventana (Fluent):");
    static const char* kMaterials[] = { "none", "acrylic_app", "mica", "acrylic_system" };
    static const char* kMaterialLabels[] = { "Opaco", "Acrilico propio (panel desenfoca la escena)", "Mica del sistema (Windows 11)", "Acrilico del sistema (Windows 11)" };
    int mat_idx = 1;
    for (int i = 0; i < 4; ++i) if (cfg.material == kMaterials[i]) mat_idx = i;
    if (ImGui::Combo("Material", &mat_idx, kMaterialLabels, 4)) {
        cfg.material = kMaterials[mat_idx];
        if (mat_idx >= 2) state.Toast("Los materiales del sistema se aplican al reiniciar la aplicacion", ctx.now);
    }
    ImGui::SameLine(); HelpMarker("Acrilico propio: el panel muestra la visualizacion desenfocada detras, con tinte, exclusion y ruido. Mica y Acrilico del sistema los compone Windows 11 detras de toda la ventana y requieren reiniciar.");
    ImGui::SliderFloat("Opacidad del tinte", &cfg.material_opacity, 0.6f, 0.95f, "%.2f");
    ImGui::SameLine(); HelpMarker("Por debajo de 0,70 el contraste del texto puede bajar de 4,5:1 (WCAG 2.1).");
    if (cfg.material_opacity < 0.7f) ImGui::TextColored(kWarn, "Opacidad baja: el contraste del texto puede no cumplir 4,5:1.");

    ImGui::Checkbox("Animaciones (apertura del panel, cambio de modo)", &cfg.animations);
    ImGui::Checkbox("Respetar las preferencias de efectos de Windows", &cfg.respect_system_effects);
    ImGui::SameLine(); HelpMarker("Si Windows tiene desactivadas las transparencias o las animaciones, el panel se vuelve opaco y las transiciones instantaneas.");

    ImGui::TextDisabled("Windows: transparencias %s, animaciones %s. Este cuadro: material %s, animaciones %s.",
        ap.system_transparency ? "activadas" : "desactivadas", ap.system_animations ? "activadas" : "desactivadas",
        ap.effects_active ? "activo" : "inactivo", ap.animations_active ? "activas" : "inactivas");
    if (!ap.effects_message.empty()) ImGui::TextColored(kWarn, "%s", ap.effects_message.c_str());
    else if (ap.system_backdrop_active) ImGui::TextColored(kOk, "Material del sistema activo detras de la ventana.");
    else if (!ap.system_backdrop_supported) ImGui::TextDisabled("Los materiales del sistema requieren Windows 11 22H2 o superior.");

    ImGui::Separator();
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

// ---------------------------------------------------------------- Telemetría
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
        ImGui::Text("Bandas:"); ImGui::NextColumn(); ImGui::Text("%d", f.band_count()); ImGui::NextColumn();
        ImGui::Text("Resolucion variable:"); ImGui::NextColumn();
        if (f.multi_resolution) ImGui::Text("graves FFT %d (%.2f Hz/bin), agudos FFT %d", f.low_fft_size, static_cast<float>(f.sample_rate) / f.low_fft_size, f.high_fft_size);
        else ImGui::Text("desactivada");
        ImGui::NextColumn();
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
        if (core::SaveConfig(ctx.shared_config, "config.json")) state.Toast("Configuracion guardada", ctx.now);
        else state.Toast("No se pudo escribir config.json", ctx.now);
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
    // Se sigue dibujando mientras se desvanece al cerrar.
    if (!state.visible && ctx.panel_alpha <= 0.001f) return;
    const float alpha = std::clamp(ctx.panel_alpha, 0.0f, 1.0f);

    ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 640), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    // Con material, el fondo lo pinta el callback del render; el color de ImGui se anula.
    const bool with_material = ctx.material != nullptr;
    if (with_material) ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    bool keep_open = state.visible;
    if (ImGui::Begin("Audio Visualizer 3.0  -  Panel de Control", &keep_open, ImGuiWindowFlags_NoCollapse)) {
        if (with_material) {
            // En la lista de fondo, que se dibuja antes que cualquier ventana: así el material queda
            // debajo de la barra de título y de los controles del panel, no encima.
            const ImVec2 pos = ImGui::GetWindowPos();
            const ImVec2 size = ImGui::GetWindowSize();
            ctx.material->AddPanelBackground(ImGui::GetBackgroundDrawList(), pos.x, pos.y, size.x, size.y, ImGui::GetStyle().WindowRounding, alpha);
        }
        ImGui::TextColored(kAccent, "ESTADO:");
        ImGui::SameLine();
        ImGui::Text("%s", core::VisualizerModeName(ctx.cfg.visual_mode));
        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::TextDisabled("Atajo: Tab / H");
        ImGui::Spacing();

        if (ImGui::BeginTabBar("VisualizerTabBar", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem(" Modos ")) { TabModes(ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" DSP y Audio ")) { TabDsp(state, ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" Bandas ")) { TabBands(state, ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" Color y Apariencia ")) { TabAppearance(state, ctx); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem(" Telemetria ")) { TabTelemetry(state, ctx); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
    if (with_material) ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (state.visible && !keep_open) state.visible = false; // cerrado con la X

    // Publicar los cambios en vivo al hilo de análisis.
    if (state.visible) PublishConfig(ctx);
}

} // namespace ui
