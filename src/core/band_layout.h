#pragma once

#include <string>
#include <vector>
#include <array>

// Definición de bandas de frecuencia: configuración, validación y partición del espectro en
// intervalos de bins. Diseño en docs/11_analysis_frame_architecture.md, sección 4.
namespace core {

// Sección "bandas" de config.json.
struct BandConfig {
    // "octaves", "linear", "manual" o "per_bin".
    std::string mode = "manual";
    // Rango cubierto por las bandas. Fuera de él queda la banda implícita "resto".
    float f_min = 20.0f;
    float f_max = 20000.0f;
    // Modo octaves: divisiones por octava. Modo linear: número de bandas.
    int divisions_per_octave = 1;
    int linear_band_count = 8;
    // Modo manual: cortes interiores estrictamente crecientes dentro de (f_min, f_max).
    std::vector<float> cuts_hz{ 60.0f, 250.0f, 500.0f, 2000.0f, 4000.0f, 6000.0f };
    // Propiedades por banda. Longitud = número de bandas; si falta alguna se rellena.
    std::vector<std::string> names{ "Sub", "Bajo", "Medios bajos", "Medios", "Medios altos", "Presencia", "Brillo" };
    std::vector<float> attack_ms{ 10.0f, 12.0f, 12.0f, 10.0f, 8.0f, 6.0f, 5.0f };
    std::vector<float> release_ms{ 250.0f, 200.0f, 160.0f, 140.0f, 110.0f, 90.0f, 80.0f };
    std::vector<float> gain{ 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    std::vector<std::array<float, 3>> colors_rgb{
        { 0.60f, 0.10f, 0.80f }, { 0.20f, 0.30f, 1.00f }, { 0.10f, 0.70f, 0.90f }, { 0.20f, 0.90f, 0.40f },
        { 0.90f, 0.90f, 0.20f }, { 1.00f, 0.60f, 0.10f }, { 1.00f, 0.30f, 0.30f } };
    // Anchura de la rampa de coseno alzado en el borde de cada banda, en bins (fase C).
    float mask_ramp_bins = 1.5f;
    // Las barras del modo 1 heredan el ataque y la caída de la banda a la que pertenecen.
    bool bars_inherit_dynamics = true;
};

struct BandDefinition {
    std::string name;
    float f_low_hz = 0.0f;
    float f_high_hz = 0.0f;
    int bin_low = 0;   // intervalo [bin_low, bin_high)
    int bin_high = 0;
    bool too_narrow = false; // la banda pide menos de un bin con la FFT actual
    std::array<float, 3> color{ 1.0f, 1.0f, 1.0f };
    float attack_ms = 12.0f;
    float release_ms = 160.0f;
    float gain = 1.0f;
};

struct BandLayout {
    std::vector<BandDefinition> bands;
    int sample_rate = 0;
    int fft_size = 0;
    float bin_resolution_hz = 0.0f;
    int count() const { return static_cast<int>(bands.size()); }
    // Índice de la banda que contiene el bin, o -1 si cae en la banda implícita "resto".
    int BandForBin(int bin) const;
};

// Número de bandas que produce la configuración (sin necesitar la frecuencia de muestreo).
int BandCount(const BandConfig& cfg, int fft_size, int sample_rate_hint);

// Normaliza la configuración: modo válido, f_min < f_max, cortes ordenados y dentro del rango,
// listas por banda de la longitud correcta rellenadas con valores por defecto. Devuelve avisos.
std::vector<std::string> ValidateBandConfig(BandConfig& cfg, float default_attack_ms, float default_release_ms);

// Particiona el espectro según la configuración ya validada.
BandLayout BuildBandLayout(const BandConfig& cfg, int sample_rate, int fft_size);

// Color de la paleta por defecto para la banda i de K (matiz repartido).
std::array<float, 3> DefaultBandColor(int index, int count);

} // namespace core
