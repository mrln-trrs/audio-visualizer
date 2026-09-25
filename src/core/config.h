#pragma once

#include <vector>
#include <mutex>
#include <string>
#include <atomic>
#include <cstdint>

#include "core/band_layout.h"

// Configuración del visualizador y su persistencia en config.json.
// Referencia de cada clave: docs/07_user_manual_and_config.md, secciones 4 y 6.3.
namespace core {

struct VisualizerConfig {
    // Sección "estilos".
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
    // Modo de visualización: ver core::VisualizerMode.
    int visual_mode = 0;
    // Marcadores de pico en los modos de barras y medidores.
    bool peak_hold_enabled = true;
    float peak_hold_time_ms = 350.0f;
    float peak_decay_speed = 1.8f;
    std::vector<float> peak_color_rgb{ 1.0f, 0.85f, 0.2f };
    // Dispositivo de audio seleccionado (vacío = predeterminado de Windows).
    std::string selected_device_name;

    // Sección "bandas": partición del espectro y dinámica por banda (docs/11, sección 4).
    BandConfig bands;
};

bool operator==(const BandConfig& a, const BandConfig& b);
inline bool operator!=(const BandConfig& a, const BandConfig& b) { return !(a == b); }

// La configuración se lee y se guarda en tiempo real desde la UI. El contador de versión
// permite al hilo de análisis detectar cambios sin tomar el mutex en cada trama.
struct SharedConfigData {
    VisualizerConfig config;
    std::mutex mtx;
    std::atomic<uint64_t> version{ 0 };
};

// Carga config.json. Las claves ausentes conservan su valor por defecto; un tipo incorrecto
// se avisa por cerr sin abortar. La sección de bandas se valida y normaliza.
void LoadConfig(SharedConfigData& shared, const std::string& filename);

// Escribe la configuración actual en disco. Devuelve false si no se pudo abrir el archivo.
bool SaveConfig(SharedConfigData& shared, const std::string& filename);

} // namespace core
