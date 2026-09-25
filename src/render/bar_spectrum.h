#pragma once

#include <vector>

#include "core/analysis_frame.h"
#include "core/config.h"

// Muestreo del espectro de la trama de análisis a barras de pantalla, en [0, 1] sobre el rango
// dinámico configurado. Es responsabilidad del render, no del análisis: el número de barras
// depende del ancho de la ventana y la escala de frecuencia es una decisión visual.
// Fundamento del mapeo: docs/05, sección 2.5; de la escala dB: sección 2.6.
namespace render {

class BarSpectrum {
public:
    // Recalcula `values()` a partir de la trama. Reconstruye el mapa de bins si cambian el número
    // de barras, la frecuencia de muestreo o los parámetros de escala de `cfg`.
    void Update(const core::AnalysisFrame& frame, int num_bars, const core::VisualizerConfig& cfg);

    const std::vector<float>& values() const { return values_; }

private:
    struct MapKey {
        int num_bars = -1;
        int sample_rate = 0;
        bool log_scale = false;
        float hz_per_bar = 0.0f;
        float min_hz = 0.0f;
        float max_hz = 0.0f;
        bool operator==(const MapKey& o) const {
            return num_bars == o.num_bars && sample_rate == o.sample_rate && log_scale == o.log_scale &&
                   hz_per_bar == o.hz_per_bar && min_hz == o.min_hz && max_hz == o.max_hz;
        }
    };

    void RebuildMap(const MapKey& key, int fft_size);
    static float SampleBand(const std::vector<float>& magnitude, float lo, float hi);

    MapKey key_;
    std::vector<float> lo_;      // posición fraccional (en bins) del inicio de cada barra
    std::vector<float> hi_;      // y del final
    std::vector<float> values_;
};

} // namespace render
