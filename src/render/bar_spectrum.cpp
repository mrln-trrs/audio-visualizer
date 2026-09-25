#include "render/bar_spectrum.h"

#include <algorithm>
#include <cmath>

namespace render {

void BarSpectrum::RebuildMap(const MapKey& key, int fft_size) {
    key_ = key;
    lo_.resize(key.num_bars);
    hi_.resize(key.num_bars);
    const double bin_hz = static_cast<double>(key.sample_rate) / fft_size;
    const double max_bin = fft_size / 2;

    const double fmin = std::max(1.0, static_cast<double>(key.min_hz));
    const double fmax = std::max(fmin * 1.01, static_cast<double>(key.max_hz));
    const double log_ratio = std::log(fmax / fmin);

    for (int i = 0; i < key.num_bars; ++i) {
        double f0, f1;
        if (key.log_scale) {
            f0 = fmin * std::exp(log_ratio * i / key.num_bars);
            f1 = fmin * std::exp(log_ratio * (i + 1) / key.num_bars);
        }
        else {
            f0 = static_cast<double>(i) * key.hz_per_bar;
            f1 = static_cast<double>(i + 1) * key.hz_per_bar;
        }
        lo_[i] = static_cast<float>(std::clamp(f0 / bin_hz, 0.0, max_bin));
        hi_[i] = static_cast<float>(std::clamp(f1 / bin_hz, 0.0, max_bin));
    }
    assigned_layout_version_ = ~0ull; // fuerza reasignar bandas
}

void BarSpectrum::AssignBands(const core::BandLayout& bands, const core::VisualizerConfig& cfg) {
    const int n = key_.num_bars;
    attack_ms_.resize(n);
    release_ms_.resize(n);
    for (int i = 0; i < n; ++i) {
        const int center_bin = static_cast<int>(0.5f * (lo_[i] + hi_[i]));
        const int b = bands.BandForBin(center_bin);
        attack_ms_[i] = b >= 0 ? bands.bands[b].attack_ms : cfg.attack_ms;
        release_ms_[i] = b >= 0 ? bands.bands[b].release_ms : cfg.release_ms;
    }
}

// Si la banda abarca menos de un bin, interpola en su centro (evita barras permanentemente
// vacías); si abarca varios, toma el pico.
float BarSpectrum::SampleBand(const std::vector<float>& magnitude, float lo, float hi) {
    const int last = static_cast<int>(magnitude.size()) - 1;
    if (hi - lo < 1.0f) {
        const float center = 0.5f * (lo + hi);
        const int i0 = std::clamp(static_cast<int>(center), 0, last);
        const int i1 = std::min(i0 + 1, last);
        const float t = center - static_cast<float>(i0);
        return magnitude[i0] * (1.0f - t) + magnitude[i1] * t;
    }
    const int i0 = std::clamp(static_cast<int>(lo), 0, last);
    const int i1 = std::clamp(static_cast<int>(std::ceil(hi)), i0 + 1, last + 1);
    float peak = 0.0f;
    for (int j = i0; j < i1; ++j) peak = std::max(peak, magnitude[j]);
    return peak;
}

void BarSpectrum::Update(const core::AnalysisFrame& frame, int num_bars, const core::VisualizerConfig& cfg,
                         const core::BandLayout* bands, uint64_t band_layout_version) {
    if (!frame.valid() || num_bars <= 0) {
        values_.assign(std::max(0, num_bars), 0.0f);
        attack_ms_.clear();
        release_ms_.clear();
        return;
    }

    MapKey key;
    key.num_bars = num_bars;
    key.sample_rate = frame.sample_rate;
    key.log_scale = (cfg.frequency_scale == "log");
    key.hz_per_bar = cfg.bin_grouping_factor;
    key.min_hz = cfg.min_frequency;
    key.max_hz = cfg.max_frequency;
    if (!(key == key_)) RebuildMap(key, frame.fft_size);

    if (bands && bands->count() > 0) {
        if (assigned_layout_version_ != band_layout_version) {
            AssignBands(*bands, cfg);
            assigned_layout_version_ = band_layout_version;
        }
    }
    else {
        attack_ms_.clear();
        release_ms_.clear();
    }

    // Magnitud lineal -> dB -> [0, 1] sobre el rango dinámico configurado.
    const float range_db = std::max(1.0f, cfg.dynamic_range_db);
    values_.resize(num_bars);
    for (int i = 0; i < num_bars; ++i) {
        const float m = SampleBand(frame.magnitude, lo_[i], hi_[i]);
        const float db = 20.0f * std::log10(m + 1e-9f);
        values_[i] = std::clamp((db + range_db) / range_db, 0.0f, 1.0f);
    }
}

} // namespace render
