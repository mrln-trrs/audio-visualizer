#pragma once

#include <vector>

#include "core/config.h"

// Mapeo de bins de la FFT a barras de pantalla. Fundamento: docs/05, sección 2.5.
namespace analysis {

// Posición fraccional (en bins) del inicio y fin de cada barra.
struct BandMap {
    std::vector<float> lo;
    std::vector<float> hi;
};

// Construye el mapa para `num_bars` barras según frequency_scale ("linear": bin_grouping_factor
// Hz por barra; "log": reparto por octavas entre min_frequency y max_frequency).
BandMap BuildBandMap(int num_bars, const core::VisualizerConfig& cfg, double bin_resolution_hz, int fft_size);

// Valor de una banda: si abarca menos de un bin, interpola en su centro (evita barras
// permanentemente vacías); si abarca varios, toma el pico.
float SampleBand(const std::vector<float>& magnitude, float lo, float hi);

} // namespace analysis
