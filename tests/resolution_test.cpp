// Prueba de la fase D (docs/10, secciones 5 y 7; docs/11, fase D): el compromiso entre resolución
// en frecuencia y latencia es real y la FFT larga lo desplaza hacia la resolución.
//   1. Dos tonos de 40 y 55 Hz (15 Hz de separación) aparecen como dos picos distintos con la FFT
//      de 8192 (5,86 Hz por bin, lóbulo principal de Hann de 23 Hz) y como un solo pico con la de
//      2048 (23,4 Hz por bin, lóbulo de 94 Hz).
//   2. Dos tonos de 40 y 42,5 Hz (un semitono) NO se separan ni con 8192: exigen ventanas de casi
//      medio segundo (docs/10, tabla 5.3). El criterio original del documento 11 era inconsistente
//      con la teoría y se corrige aquí.
//   3. La latencia añadida al centro de la ventana larga es (N_low - N_base) / (2 fs) = 64 ms.

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

#include "core/constants.h"
#include "core/config.h"
#include "core/analysis_frame.h"
#include "analysis/multi_resolution.h"
#include "analysis/window_function.h"

namespace {

int failures = 0;
void Check(bool ok, const std::string& what, double got, double expected) {
    std::printf("%s %-70s obtenido=%.6g esperado=%.6g\n", ok ? "[OK]  " : "[FALLO]", what.c_str(), got, expected);
    if (!ok) ++failures;
}

// Cuenta máximos locales de la magnitud entre f0 y f1 que superen el 20 % del máximo del rango.
int CountPeaks(const std::vector<float>& mag, int fft_size, int sample_rate, double f0, double f1) {
    const double bin_hz = static_cast<double>(sample_rate) / fft_size;
    const int k0 = std::max(1, static_cast<int>(f0 / bin_hz));
    const int k1 = std::min(static_cast<int>(mag.size()) - 2, static_cast<int>(f1 / bin_hz));
    float top = 0.0f;
    for (int k = k0; k <= k1; ++k) top = std::max(top, mag[k]);
    int peaks = 0;
    for (int k = k0; k <= k1; ++k) {
        if (mag[k] > mag[k - 1] && mag[k] >= mag[k + 1] && mag[k] > 0.2f * top) ++peaks;
    }
    return peaks;
}

std::vector<float> Synth(int n, int sample_rate, double fa, double fb) {
    const double kPi = 3.14159265358979323846;
    std::vector<float> x(n);
    for (int i = 0; i < n; ++i) x[i] = static_cast<float>(0.4 * std::sin(2 * kPi * fa * i / sample_rate) + 0.4 * std::sin(2 * kPi * fb * i / sample_rate));
    return x;
}

std::vector<float> BaseMagnitude(const std::vector<float>& x, int n) {
    // Misma cadena que la FFT base del programa, calculada aquí con la ventana periódica.
    const analysis::AnalysisWindow w = analysis::MakePeriodicHann(n);
    std::vector<float> frame(n);
    const int avail = static_cast<int>(x.size());
    for (int i = 0; i < n; ++i) frame[i] = x[avail - n + i] * w.coefficients[i];
    fftwf_complex* out = fftwf_alloc_complex(n / 2 + 1);
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(n, frame.data(), out, FFTW_ESTIMATE);
    fftwf_execute(plan);
    std::vector<float> mag(n / 2 + 1);
    for (int k = 0; k <= n / 2; ++k) mag[k] = std::sqrt(out[k][0] * out[k][0] + out[k][1] * out[k][1]) * w.normalization;
    fftwf_destroy_plan(plan);
    fftwf_free(out);
    return mag;
}

} // namespace

int main() {
    const int sample_rate = 48000;
    core::AnalysisConfig cfg;
    cfg.multi_resolution = true;
    core::ValidateAnalysisConfig(cfg);
    analysis::MultiResolution mr;
    mr.Configure(cfg);
    Check(mr.configured(), "MultiResolution configurada (8192 graves, 512 agudos)", mr.configured() ? 1 : 0, 1);

    // 1. 40 y 55 Hz.
    {
        const std::vector<float> x = Synth(core::MAX_LOW_BAND_FFT_SIZE, sample_rate, 40.0, 55.0);
        core::AnalysisFrame frame;
        mr.Process(x, frame);
        const int peaks_low = CountPeaks(frame.low_magnitude, frame.low_fft_size, sample_rate, 25.0, 75.0);
        const std::vector<float> base = BaseMagnitude(x, core::FFT_SIZE);
        const int peaks_base = CountPeaks(base, core::FFT_SIZE, sample_rate, 25.0, 75.0);
        Check(peaks_low == 2, "40 y 55 Hz: dos picos con la FFT larga de 8192", peaks_low, 2);
        Check(peaks_base == 1, "40 y 55 Hz: un solo pico con la FFT base de 2048 (se funden)", peaks_base, 1);
    }

    // 2. 40 y 42,5 Hz: tampoco se separan con 8192 (5,86 Hz por bin).
    {
        const std::vector<float> x = Synth(core::MAX_LOW_BAND_FFT_SIZE, sample_rate, 40.0, 42.5);
        core::AnalysisFrame frame;
        mr.Process(x, frame);
        const int peaks_low = CountPeaks(frame.low_magnitude, frame.low_fft_size, sample_rate, 25.0, 75.0);
        Check(peaks_low == 1, "40 y 42,5 Hz (un semitono): un solo pico incluso con 8192, como predice 5.1", peaks_low, 1);
    }

    // 3. Latencia añadida al centro de la ventana larga.
    {
        const double added_ms = 1000.0 * (cfg.low_band_fft_size - core::FFT_SIZE) / (2.0 * sample_rate);
        Check(std::fabs(added_ms - 64.0) < 0.01, "latencia añadida en graves por la ventana de 8192 (ms)", added_ms, 64.0);
        const double saved_ms = 1000.0 * (core::FFT_SIZE - cfg.high_band_fft_size) / (2.0 * sample_rate);
        Check(std::fabs(saved_ms - 16.0) < 0.01, "latencia ahorrada en agudos por la ventana de 512 (ms)", saved_ms, 16.0);
    }

    // 4. Normalización coherente entre resoluciones: un seno de 1 kHz a -6 dBFS da el mismo pico.
    {
        const double kPi = 3.14159265358979323846;
        std::vector<float> x(core::MAX_LOW_BAND_FFT_SIZE);
        for (size_t i = 0; i < x.size(); ++i) x[i] = static_cast<float>(0.5 * std::sin(2 * kPi * 1000.0 * i / sample_rate));
        core::AnalysisFrame frame;
        mr.Process(x, frame);
        const std::vector<float> base = BaseMagnitude(x, core::FFT_SIZE);
        const float p_base = *std::max_element(base.begin(), base.end());
        const float p_low = *std::max_element(frame.low_magnitude.begin(), frame.low_magnitude.end());
        const float p_high = *std::max_element(frame.high_magnitude.begin(), frame.high_magnitude.end());
        // Diferencias solo por festoneado (hasta 1,42 dB): el tono cae en posiciones distintas del bin.
        Check(std::fabs(20 * std::log10(p_low / p_base)) < 1.5, "pico de 1 kHz coherente entre 8192 y 2048 (dB de diferencia)", 20 * std::log10(p_low / p_base), 0.0);
        Check(std::fabs(20 * std::log10(p_high / p_base)) < 1.5, "pico de 1 kHz coherente entre 512 y 2048 (dB de diferencia)", 20 * std::log10(p_high / p_base), 0.0);
    }

    std::printf("\n%s: %d fallo(s)\n", failures == 0 ? "PRUEBA SUPERADA" : "PRUEBA FALLIDA", failures);
    return failures == 0 ? 0 : 1;
}
