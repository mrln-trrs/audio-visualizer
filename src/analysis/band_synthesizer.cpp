#include "analysis/band_synthesizer.h"

#include <algorithm>
#include <cmath>

namespace analysis {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Transición suave de 0 a 1 centrada en `edge` con anchura `ramp` bins (coseno alzado).
// Con ramp = 0 es un escalón. Evaluada en el centro del bin k.
float Step(float k, float edge, float ramp) {
    if (ramp <= 0.0f) return k >= edge ? 1.0f : 0.0f;
    const float t = std::clamp((k - edge + 0.5f * ramp) / ramp, 0.0f, 1.0f);
    return 0.5f * (1.0f - std::cos(static_cast<float>(kPi) * t));
}

} // namespace

BandSynthesizer::BandSynthesizer(int fft_size, int hop_size)
    : n_(fft_size), hop_(hop_size), bins_(fft_size / 2 + 1) {
    scratch_ = fftwf_alloc_complex(bins_);
    time_out_ = fftwf_alloc_real(n_);
    plan_ = fftwf_plan_dft_c2r_1d(n_, scratch_, time_out_, FFTW_MEASURE);
}

BandSynthesizer::~BandSynthesizer() {
    if (plan_) fftwf_destroy_plan(plan_);
    if (scratch_) fftwf_free(scratch_);
    if (time_out_) fftwf_free(time_out_);
}

void BandSynthesizer::Configure(const core::BandLayout& layout, float ramp_bins, const AnalysisWindow& window) {
    const int k = layout.count();
    rows_ = k + 1; // la última fila es la banda implícita "resto"
    masks_.assign(rows_, std::vector<float>(bins_, 0.0f));
    accum_.assign(rows_, std::vector<float>(n_, 0.0f));

    // Máscaras: M_b[k] = s(k; bin_low) - s(k; bin_high). Las bandas vecinas comparten el borde, así
    // que la suma telescopa y el resto la completa hasta 1 exactamente.
    for (int b = 0; b < k; ++b) {
        const float lo = static_cast<float>(layout.bands[b].bin_low);
        const float hi = static_cast<float>(layout.bands[b].bin_high);
        for (int i = 0; i < bins_; ++i) {
            const float x = static_cast<float>(i);
            masks_[b][i] = std::max(0.0f, Step(x, lo, ramp_bins) - Step(x, hi, ramp_bins));
        }
    }
    for (int i = 0; i < bins_; ++i) {
        float sum = 0.0f;
        for (int b = 0; b < k; ++b) sum += masks_[b][i];
        masks_[k][i] = std::max(0.0f, 1.0f - sum);
    }

    // Ventana de síntesis (igual a la de análisis) con el 1/N de la IFFT sin normalizar de FFTW.
    synthesis_window_.resize(n_);
    for (int i = 0; i < n_; ++i) synthesis_window_[i] = window.coefficients[i] / static_cast<float>(n_);

    // Normalización por sum_m w^2[n - mH]. Es constante para Hann periódica con N/H >= 4, pero se
    // calcula por índice para no depender de esa propiedad.
    ola_norm_.assign(hop_, 0.0f);
    const int frames = n_ / hop_;
    for (int i = 0; i < hop_; ++i) {
        double s = 0.0;
        for (int m = 0; m < frames; ++m) {
            const float w = window.coefficients[(i + m * hop_) % n_];
            s += static_cast<double>(w) * w;
        }
        ola_norm_[i] = s > 0.0 ? static_cast<float>(1.0 / s) : 0.0f;
    }
}

void BandSynthesizer::Process(const fftwf_complex* spectrum, std::vector<float>& out) {
    out.resize(static_cast<size_t>(rows_) * hop_);
    for (int r = 0; r < rows_; ++r) {
        const std::vector<float>& mask = masks_[r];
        for (int i = 0; i < bins_; ++i) {
            scratch_[i][0] = spectrum[i][0] * mask[i];
            scratch_[i][1] = spectrum[i][1] * mask[i];
        }
        fftwf_execute(plan_);

        std::vector<float>& acc = accum_[r];
        for (int i = 0; i < n_; ++i) acc[i] += time_out_[i] * synthesis_window_[i];

        // Las primeras `hop_` muestras ya tienen todas sus contribuciones.
        float* dst = out.data() + static_cast<size_t>(r) * hop_;
        for (int i = 0; i < hop_; ++i) dst[i] = acc[i] * ola_norm_[i];

        // Desplazar el acumulador un salto y vaciar la cola.
        std::copy(acc.begin() + hop_, acc.end(), acc.begin());
        std::fill(acc.end() - hop_, acc.end(), 0.0f);
    }
}

} // namespace analysis
