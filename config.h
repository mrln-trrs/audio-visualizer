#pragma once

#include <vector>
#include <mutex>
#include <string>

// Configuración del visualizador. Los valores por defecto se usan cuando
// la clave no aparece en config.json.
struct VisualizerConfig {
    // Tiempo de respuesta al subir (ms). Bajo = ataque inmediato.
    float attack_ms = 12.0f;
    // Tiempo de caída (ms). Controla cuánto tardan las barras en bajar.
    float release_ms = 160.0f;
    // Ganancia visual aplicada al espectro normalizado.
    float amplitude_factor = 1.0f;
    // Rango dinámico mostrado en dB: 0 dBFS arriba, -dynamic_range_db abajo.
    float dynamic_range_db = 60.0f;
    // Color base RGB en [0, 1].
    std::vector<float> base_color_rgb{ 0.65f, 0.15f, 0.15f };
    // Ancho de banda por barra en Hz (solo con frequency_scale = "linear").
    float bin_grouping_factor = 10.0f;
    // "linear" o "log".
    std::string frequency_scale = "linear";
    // Límites del eje en Hz (solo con frequency_scale = "log").
    float min_frequency = 30.0f;
    float max_frequency = 16000.0f;
    // Sincronía vertical. true = un cuadro por refresco del monitor.
    bool vsync = true;
    // Límite de cuadros por segundo si el driver ignora la sincronía vertical.
    // 0 = usar la tasa de refresco del monitor donde está la ventana.
    int max_fps = 0;
};

// La configuración se carga una vez al arrancar y después solo se lee,
// pero se protege con mutex por si en el futuro se recarga en caliente.
struct SharedConfigData {
    VisualizerConfig config;
    std::mutex mtx;
};

void LoadConfig(SharedConfigData& sharedConfigData, const std::string& filename);
