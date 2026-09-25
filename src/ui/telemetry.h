#pragma once

#include <string>

// Métricas de rendimiento para el título de la ventana y la pestaña de telemetría del HUD.
namespace ui {

struct Telemetry {
    static constexpr int HISTORY = 60;

    float fps_history[HISTORY] = {};
    int history_offset = 0;
    float last_fps = 0.0f;
    float last_ups = 0.0f;   // espectros publicados por segundo
    int monitor_hz = 0;
    bool limiter_active = false;
    int num_bars = 0;

    // Registra una muestra de un segundo.
    void Record(float fps, float ups);
};

// Título de la ventana con las métricas del último segundo.
std::string BuildWindowTitle(const char* mode_name, const Telemetry& t, int sample_rate);

} // namespace ui
