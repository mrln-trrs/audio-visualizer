#pragma once

#include <string>

#include "core/config.h"
#include "core/shared_state.h"
#include "ui/telemetry.h"

// Panel de control Dear ImGui: modos, DSP, dispositivos, colores, telemetría y persistencia.
namespace ui {

struct HudState {
    bool visible = false;
    std::string toast;
    double toast_time = 0.0;

    void Toast(const std::string& message, double now) { toast = message; toast_time = now; }
};

struct HudContext {
    core::VisualizerConfig& cfg;          // copia local del render; el HUD la edita en vivo
    core::SharedConfigData& shared_config;
    core::VisualizerData& vis;
    core::AudioData& audio;
    const Telemetry& telemetry;
    const core::AnalysisFrame& frame;     // última trama de análisis, para la pestaña de telemetría
    double now;
};

// Dibuja el panel si `state.visible`. Debe llamarse entre ImGui::NewFrame() e ImGui::Render().
// Los cambios en `ctx.cfg` se publican a `shared_config` con incremento de versión.
void DrawHud(HudState& state, HudContext& ctx);

} // namespace ui
