#pragma once

#include <vector>
#include <cstdint>

#include "core/constants.h"

// Trama de análisis: todo lo que el hilo de análisis extrae de una ventana de audio.
// Diseño y presupuesto en docs/11_analysis_frame_architecture.md, sección 3.
// Los renderizadores consumen esta estructura y ninguno vuelve a tocar el audio crudo.
namespace core {

struct AnalysisFrame {
    // Identidad de la trama.
    uint64_t sequence = 0;         // contador monótono, crece en cada publicación
    uint64_t sample_position = 0;  // total_samples al final de la ventana analizada
    int sample_rate = 0;
    int fft_size = FFT_SIZE;
    int hop_size = HOP_SIZE;

    // Espectro completo, SPECTRUM_BINS elementos cada vector.
    std::vector<float> magnitude;     // |X[k]| normalizada: seno a escala completa = 1.0
    std::vector<float> magnitude_db;  // 20 log10(|X[k]| + 1e-9), sin normalizar a [0, 1]
    std::vector<float> phase;         // arg X[k] en (-pi, pi]

    // Métricas globales de la ventana.
    float rms = 0.0f;                  // raíz de la media de x^2 sobre la ventana (sin enventanar)
    float peak = 0.0f;                 // max |x| en la ventana
    float spectral_flux = 0.0f;        // sum_k max(0, |X_m[k]| - |X_{m-1}[k]|)
    float spectral_centroid_hz = 0.0f; // sum_k f_k |X[k]| / sum_k |X[k]|

    // Mezcla en el dominio del tiempo: las WAVEFORM_SNAPSHOT_SIZE muestras más recientes.
    std::vector<float> mix_waveform;

    // Fase B (docs/11, sección 3): band_energy, band_rms, band_peak_db, band_waveform.

    float bin_resolution_hz() const { return sample_rate > 0 ? static_cast<float>(sample_rate) / fft_size : 0.0f; }
    bool valid() const { return sequence > 0 && sample_rate > 0 && !magnitude.empty(); }
};

} // namespace core
