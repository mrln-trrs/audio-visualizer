#include "analysis/band_metrics.h"

#include <algorithm>
#include <cmath>

namespace analysis {

void ComputeBandMetrics(const std::vector<float>& magnitude, const std::vector<float>& magnitude_db,
                        const core::BandLayout& layout, BandMetrics& out) {
    const int k = layout.count();
    if (static_cast<int>(out.energy.size()) != k) {
        out.energy.assign(k, 0.0f);
        out.rms.assign(k, 0.0f);
        out.peak_db.assign(k, -180.0f);
    }
    const int bins = static_cast<int>(magnitude.size());
    for (int b = 0; b < k; ++b) {
        const core::BandDefinition& def = layout.bands[b];
        const int lo = std::clamp(def.bin_low, 0, bins);
        const int hi = std::clamp(def.bin_high, lo, bins);
        double e = 0.0;
        float peak = -180.0f;
        for (int i = lo; i < hi; ++i) {
            e += static_cast<double>(magnitude[i]) * magnitude[i];
            peak = std::max(peak, magnitude_db[i]);
        }
        out.energy[b] = static_cast<float>(e);
        out.rms[b] = static_cast<float>(std::sqrt(e / std::max(1, hi - lo)));
        out.peak_db[b] = peak;
    }
}

float TotalEnergy(const std::vector<float>& magnitude) {
    double e = 0.0;
    for (float m : magnitude) e += static_cast<double>(m) * m;
    return static_cast<float>(e);
}

} // namespace analysis
