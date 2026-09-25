#include "analysis/band_mapper.h"

#include <algorithm>
#include <cmath>

namespace analysis {

BandMap BuildBandMap(int num_bars, const core::VisualizerConfig& cfg, double bin_resolution_hz, int fft_size) {
    BandMap map;
    map.lo.resize(num_bars);
    map.hi.resize(num_bars);
    const double max_bin = fft_size / 2;

    const bool log_scale = (cfg.frequency_scale == "log");
    const double fmin = std::max(1.0, static_cast<double>(cfg.min_frequency));
    const double fmax = std::max(fmin * 1.01, static_cast<double>(cfg.max_frequency));
    const double log_ratio = std::log(fmax / fmin);

    for (int i = 0; i < num_bars; ++i) {
        double f0, f1;
        if (log_scale) {
            f0 = fmin * std::exp(log_ratio * i / num_bars);
            f1 = fmin * std::exp(log_ratio * (i + 1) / num_bars);
        }
        else {
            f0 = static_cast<double>(i) * cfg.bin_grouping_factor;
            f1 = static_cast<double>(i + 1) * cfg.bin_grouping_factor;
        }
        map.lo[i] = static_cast<float>(std::clamp(f0 / bin_resolution_hz, 0.0, max_bin));
        map.hi[i] = static_cast<float>(std::clamp(f1 / bin_resolution_hz, 0.0, max_bin));
    }
    return map;
}

float SampleBand(const std::vector<float>& magnitude, float lo, float hi) {
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

} // namespace analysis
