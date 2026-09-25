#include "analysis/multi_resolution.h"

#include <cmath>
#include <algorithm>

namespace analysis {

void MultiResolution::Stage::Setup(int size) {
    if (n == size && plan) return;
    Destroy();
    n = size;
    window = MakePeriodicHann(n);
    frame.assign(n, 0.0f);
    out = fftwf_alloc_complex(n / 2 + 1);
    // FFTW_ESTIMATE: la planificación con medida de una FFT de 16384 tardaría decenas de ms y este
    // método puede llamarse en caliente al cambiar la configuración desde el HUD.
    plan = fftwf_plan_dft_r2c_1d(n, frame.data(), out, FFTW_ESTIMATE);
}

void MultiResolution::Stage::Destroy() {
    if (plan) fftwf_destroy_plan(plan);
    if (out) fftwf_free(out);
    plan = nullptr;
    out = nullptr;
    n = 0;
}

void MultiResolution::Stage::Run(const std::vector<float>& latest, std::vector<float>& magnitude) {
    // Las n muestras más recientes, enventanadas.
    const int available = static_cast<int>(latest.size());
    for (int i = 0; i < n; ++i) {
        const int src = available - n + i;
        frame[i] = (src >= 0 ? latest[src] : 0.0f) * window.coefficients[i];
    }
    fftwf_execute(plan);
    const int bins = n / 2 + 1;
    if (static_cast<int>(magnitude.size()) != bins) magnitude.assign(bins, 0.0f);
    for (int k = 0; k < bins; ++k) {
        magnitude[k] = std::sqrt(out[k][0] * out[k][0] + out[k][1] * out[k][1]) * window.normalization;
    }
}

MultiResolution::~MultiResolution() {
    low_.Destroy();
    high_.Destroy();
}

void MultiResolution::Configure(const core::AnalysisConfig& cfg) {
    cfg_ = cfg;
    low_n_ = cfg.low_band_fft_size;
    high_n_ = cfg.high_band_fft_size;
    low_.Setup(low_n_);
    high_.Setup(high_n_);
    low_plan_ = low_.plan;
    high_plan_ = high_.plan;
}

void MultiResolution::Process(const std::vector<float>& latest, core::AnalysisFrame& out) {
    if (!configured()) return;
    low_.Run(latest, out.low_magnitude);
    high_.Run(latest, out.high_magnitude);
    out.multi_resolution = true;
    out.low_fft_size = low_n_;
    out.low_band_max_hz = cfg_.low_band_max_hz;
    out.high_fft_size = high_n_;
    out.high_band_min_hz = cfg_.high_band_min_hz;
}

} // namespace analysis
