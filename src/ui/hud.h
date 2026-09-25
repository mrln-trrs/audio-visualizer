#pragma once

#include <string>

#include "core/config.h"
#include "core/shared_state.h"
#include "core/band_layout.h"
#include "ui/telemetry.h"
#include "ui/panel_material.h"

// Panel de control Dear ImGui: modos, DSP, bandas, dispositivos, color y apariencia, telemetría
// y persistencia. El material del panel y las transiciones siguen docs/12.
namespace ui {

struct HudState {
    bool visible = false;
    std::string toast;
    double toast_time = 0.0;

    void Toast(const std::string& message, double now) { toast = message; toast_time = now; }
};

// Estado de la apariencia que el render conoce y el HUD muestra o edita.
struct AppearanceStatus {
    bool system_transparency = true;   // preferencia de Windows
    bool system_animations = true;
    bool effects_active = false;       // material propio en uso este cuadro
    bool animations_active = false;
    bool system_backdrop_active = false;
    bool system_backdrop_supported = false;
    std::string effects_message;       // motivo si algo no se pudo aplicar
};

struct HudContext {
    core::VisualizerConfig& cfg;          // copia local del render; el HUD la edita en vivo
    core::SharedConfigData& shared_config;
    core::VisualizerData& vis;
    core::AudioData& audio;
    const Telemetry& telemetry;
    const core::AnalysisFrame& frame;     // última trama de análisis
    const core::BandLayout& bands;        // partición actual, para mostrar bins y avisos
    const AppearanceStatus& appearance;
    IPanelMaterial* material;             // nulo si el panel debe ser opaco
    float panel_alpha;                    // 0 a 1, transición de apertura y cierre
    double now;
};

// Dibuja el panel si es visible o se está desvaneciendo. Debe llamarse entre ImGui::NewFrame() e
// ImGui::Render(). Los cambios en `ctx.cfg` se publican a `shared_config` con incremento de versión.
void DrawHud(HudState& state, HudContext& ctx);

} // namespace ui
