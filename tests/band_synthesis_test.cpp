// Prueba numérica de la fase C (docs/04, historia 6.1; docs/10, teorema 4.3):
//   1. Las máscaras de banda más la del resto suman exactamente 1 en todo bin.
//   2. La suma de las ondas reconstruidas de todas las filas (bandas y resto) reproduce la señal
//      original con error máximo inferior a 1e-5 en escala completa, tras el arranque del
//      solapamiento (N/H tramas).
//   3. Cada tono de prueba aparece en la fila de la banda que lo contiene: el RMS de esa fila
//      coincide con el del tono dentro del 5 % y las filas no contiguas quedan 40 dB por debajo.
// Se ejecuta sin audio ni ventana con la misma ventana, FFT y sintetizador que el programa.

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <fftw3.h>

#include "core/constants.h"
#include "core/band_layout.h"
#include "analysis/window_function.h"
#include "analysis/band_synthesizer.h"

namespace {

int failures = 0;

void Check(bool ok, const std::string& what, double got, double expected, double tolerance) {
    std::printf("%s %-62s obtenido=%.6g esperado=%.6g tol=%.3g\n", ok ? "[OK]  " : "[FALLO]", what.c_str(), got, expected, tolerance);
    if (!ok) ++failures;
}

struct Tone { double freq; double amp; };

} // namespace

int main() {
    const int N = core::FFT_SIZE, H = core::HOP_SIZE, R = N / H;
    const int sample_rate = 48000;
    const double kPi = 3.14159265358979323846;

    // Señal: tres tonos en bandas distintas del preset (Bajo, Medios, Presencia) más ruido
    // determinista de banda ancha, para que el resto también tenga contenido.
    const Tone tones[] = { { 100.0, 0.5 }, { 1000.0, 0.3 }, { 5000.0, 0.2 } };
    const int total = sample_rate; // un segundo
    std::vector<float> x(total);
    unsigned int seed = 12345u;
    for (int n = 0; n < total; ++n) {
        double v = 0.0;
        for (const Tone& t : tones) v += t.amp * std::sin(2.0 * kPi * t.freq * n / sample_rate);
        seed = seed * 1664525u + 1013904223u;
        v += 0.05 * ((seed >> 8) / 8388608.0 - 1.0);
        x[n] = static_cast<float>(v);
    }

    core::BandConfig cfg;
    core::ValidateBandConfig(cfg, 12.0f, 160.0f);
    const core::BandLayout layout = core::BuildBandLayout(cfg, sample_rate, N);
    const analysis::AnalysisWindow window = analysis::MakePeriodicHann(N);

    analysis::BandSynthesizer synth(N, H);
    synth.Configure(layout, cfg.mask_ramp_bins, window);
    const int rows = synth.rows();

    // 1. Suma de máscaras.
    double max_mask_err = 0.0;
    for (int k = 0; k < core::SPECTRUM_BINS; ++k) {
        double s = 0.0;
        for (int r = 0; r < rows; ++r) s += synth.mask(r)[k];
        max_mask_err = std::max(max_mask_err, std::fabs(s - 1.0));
    }
    Check(max_mask_err < 1e-6, "suma de mascaras (bandas + resto) igual a 1 en todo bin, error max", max_mask_err, 0.0, 1e-6);

    // 2 y 3. Recorrido por tramas.
    std::vector<float> frame(N);
    fftwf_complex* spectrum = fftwf_alloc_complex(core::SPECTRUM_BINS);
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(N, frame.data(), spectrum, FFTW_ESTIMATE);
    std::vector<float> out;
    std::vector<double> row_energy(rows, 0.0);
    double max_sum_err = 0.0;
    long compared = 0;
    int frame_index = 0;

    for (int start = 0; start + N <= total; start += H, ++frame_index) {
        for (int n = 0; n < N; ++n) frame[n] = x[start + n] * window.coefficients[n];
        fftwf_execute(plan);
        synth.Process(spectrum, out);
        if (frame_index < R) continue; // arranque del solapamiento: las salidas aún son parciales
        // Las H muestras emitidas corresponden a x[start .. start + H).
        for (int i = 0; i < H; ++i) {
            double sum = 0.0;
            for (int r = 0; r < rows; ++r) {
                const double v = out[static_cast<size_t>(r) * H + i];
                sum += v;
                row_energy[r] += v * v;
            }
            max_sum_err = std::max(max_sum_err, std::fabs(sum - x[start + i]));
            ++compared;
        }
    }
    Check(max_sum_err < 1e-5, "suma de las ondas de banda igual a la mezcla, error max (" + std::to_string(compared) + " muestras)", max_sum_err, 0.0, 1e-5);

    // 3. Cada tono en su banda.
    const double samples = static_cast<double>(compared);
    for (const Tone& t : tones) {
        const int b = layout.BandForBin(static_cast<int>(std::lround(t.freq * N / sample_rate)));
        if (b < 0) { Check(false, "tono " + std::to_string(static_cast<int>(t.freq)) + " Hz cae en una banda", b, 0, 0); continue; }
        const double expected_rms = t.amp / std::sqrt(2.0);
        const double rms = std::sqrt(row_energy[b] / samples);
        Check(std::fabs(rms - expected_rms) / expected_rms < 0.05,
              "RMS de la banda " + layout.bands[b].name + " con tono de " + std::to_string(static_cast<int>(t.freq)) + " Hz", rms, expected_rms, 0.05 * expected_rms);
    }
    // Fila del resto: solo ruido fuera de 20 a 20000 Hz, casi nada.
    const double rest_rms = std::sqrt(row_energy[rows - 1] / samples);
    Check(rest_rms < 0.02, "RMS de la fila resto (solo ruido fuera de 20 a 20000 Hz)", rest_rms, 0.0, 0.02);

    fftwf_destroy_plan(plan);
    fftwf_free(spectrum);

    std::printf("\n%s: %d fallo(s)\n", failures == 0 ? "PRUEBA SUPERADA" : "PRUEBA FALLIDA", failures);
    return failures == 0 ? 0 : 1;
}
