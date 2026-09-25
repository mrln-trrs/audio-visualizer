#pragma once

#include <vector>
#include <cstdint>

#include "core/analysis_frame.h"
#include "core/config.h"
#include "core/band_layout.h"

// Muestreo del espectro de la trama de análisis a barras de pantalla, en [0, 1] sobre el rango
// dinámico configurado. Es responsabilidad del render, no del análisis: el número de barras
// depende del ancho de la ventana y la escala de frecuencia es una decisión visual.
// Fundamento del mapeo: docs/05, sección 2.5; de la escala dB: sección 2.6.
//
// Con resolución variable (docs/11, fase D) cada barra lee del espectro cuya ventana conviene a su
// frecuencia central: la FFT larga en graves, la corta en agudos, la base en medios.
namespace render {

class BarSpectrum {
public:
    void Update(const core::AnalysisFrame& frame, int num_bars, const core::VisualizerConfig& cfg,
                const core::BandLayout* bands, uint64_t band_layout_version);

    const std::vector<float>& values() const { return values_; }
    // Constantes por barra heredadas de su banda. Vacíos si no hay partición.
    const std::vector<float>& attack_ms() const { return attack_ms_; }
    const std::vector<float>& release_ms() const { return release_ms_; }
    // Barras que leen de cada fuente en el último mapa: [base, graves, agudos].
    const int* source_counts() const { return source_counts_; }

private:
    enum Source : unsigned char { kBase = 0, kLow = 1, kHigh = 2 };

    struct MapKey {
        int num_bars = -1;
        int sample_rate = 0;
        bool log_scale = false;
        float hz_per_bar = 0.0f;
        float min_hz = 0.0f;
        float max_hz = 0.0f;
        bool multi = false;
        int low_fft = 0;
        float low_max_hz = 0.0f;
        int high_fft = 0;
        float high_min_hz = 0.0f;
        bool operator==(const MapKey& o) const {
            return num_bars == o.num_bars && sample_rate == o.sample_rate && log_scale == o.log_scale &&
                   hz_per_bar == o.hz_per_bar && min_hz == o.min_hz && max_hz == o.max_hz && multi == o.multi &&
                   low_fft == o.low_fft && low_max_hz == o.low_max_hz && high_fft == o.high_fft && high_min_hz == o.high_min_hz;
        }
    };

    void RebuildMap(const MapKey& key, int fft_size);
    void AssignBands(const core::BandLayout& bands, const core::VisualizerConfig& cfg);
    static float SampleBand(const std::vector<float>& magnitude, float lo, float hi);

    MapKey key_;
    uint64_t assigned_layout_version_ = ~0ull;
    std::vector<float> lo_;             // posición fraccional (en bins de su fuente) del inicio
    std::vector<float> hi_;             // y del final
    std::vector<float> center_hz_;      // frecuencia central de la barra
    std::vector<unsigned char> source_;
    int source_counts_[3] = { 0, 0, 0 };
    std::vector<float> values_;
    std::vector<float> attack_ms_;
    std::vector<float> release_ms_;
};

} // namespace render
