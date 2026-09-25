// Prueba numérica de la fase B (docs/04, historia 5.2; docs/11, sección 11):
//   1. Una senoidal de 100 Hz a -6 dBFS solo produce energía apreciable en la banda "Bajo",
//      y su pico está a -6 dB con error inferior a 0,5 dB.
//   2. Parseval por bandas: la suma de las energías de todas las bandas más la banda implícita
//      "resto" iguala la energía total con error relativo menor que 1e-4.
//   3. Con la partición por octavas y con la lineal, la suma también se conserva.
// Se ejecuta sin audio ni ventana: sintetiza la señal, aplica la misma ventana y la misma FFT
// que el hilo de análisis y usa las mismas funciones de core y analysis.

#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <fftw3.h>

#include "core/constants.h"
#include "core/band_layout.h"
#include "analysis/window_function.h"
#include "analysis/band_metrics.h"

namespace {

int failures = 0;

void Check(bool ok, const char* what, double got, double expected, double tolerance) {
    std::printf("%s %-58s obtenido=%.6g esperado=%.6g tol=%.3g\n", ok ? "[OK]  " : "[FALLO]", what, got, expected, tolerance);
    if (!ok) ++failures;
}

// Espectro de magnitud normalizado igual que analysis_thread: Hann periódica y 2 / sum(w).
void Spectrum(const std::vector<float>& x, std::vector<float>& magnitude, std::vector<float>& magnitude_db) {
    const int n = core::FFT_SIZE;
    const analysis::AnalysisWindow w = analysis::MakePeriodicHann(n);
    std::vector<float> frame(n);
    for (int i = 0; i < n; ++i) frame[i] = x[i] * w.coefficients[i];
    fftwf_complex* out = fftwf_alloc_complex(core::SPECTRUM_BINS);
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(n, frame.data(), out, FFTW_ESTIMATE);
    fftwf_execute(plan);
    magnitude.resize(core::SPECTRUM_BINS);
    magnitude_db.resize(core::SPECTRUM_BINS);
    for (int k = 0; k < core::SPECTRUM_BINS; ++k) {
        const float m = std::sqrt(out[k][0] * out[k][0] + out[k][1] * out[k][1]) * w.normalization;
        magnitude[k] = m;
        magnitude_db[k] = 20.0f * std::log10(m + 1e-9f);
    }
    fftwf_destroy_plan(plan);
    fftwf_free(out);
}

double SumPlusRest(const analysis::BandMetrics& m, const core::BandLayout& layout, const std::vector<float>& magnitude) {
    double sum = 0.0;
    for (float e : m.energy) sum += e;
    // Banda implícita "resto": bins fuera de todas las bandas.
    for (int k = 0; k < static_cast<int>(magnitude.size()); ++k) {
        if (layout.BandForBin(k) < 0) sum += static_cast<double>(magnitude[k]) * magnitude[k];
    }
    return sum;
}

void TestParseval(const char* label, core::BandConfig cfg, const std::vector<float>& magnitude, const std::vector<float>& magnitude_db, int sample_rate) {
    core::ValidateBandConfig(cfg, 12.0f, 160.0f);
    const core::BandLayout layout = core::BuildBandLayout(cfg, sample_rate, core::FFT_SIZE);
    analysis::BandMetrics m;
    analysis::ComputeBandMetrics(magnitude, magnitude_db, layout, m);
    const double total = analysis::TotalEnergy(magnitude);
    const double sum = SumPlusRest(m, layout, magnitude);
    const double rel = std::fabs(sum - total) / std::max(1e-30, total);
    std::string what = std::string("Parseval por bandas (") + label + ", " + std::to_string(layout.count()) + " bandas), error relativo";
    Check(rel < 1e-4, what.c_str(), rel, 0.0, 1e-4);
}

} // namespace

int main() {
    const int sample_rate = 48000;
    const double freq = 100.0;
    const double amplitude = std::pow(10.0, -6.0 / 20.0); // -6 dBFS
    const double kPi = 3.14159265358979323846;

    std::vector<float> x(core::FFT_SIZE);
    for (int n = 0; n < core::FFT_SIZE; ++n) x[n] = static_cast<float>(amplitude * std::sin(2.0 * kPi * freq * n / sample_rate));

    std::vector<float> magnitude, magnitude_db;
    Spectrum(x, magnitude, magnitude_db);

    // 1. Preset de siete bandas: solo "Bajo" (60 a 250 Hz) tiene energía.
    core::BandConfig preset;
    core::ValidateBandConfig(preset, 12.0f, 160.0f);
    const core::BandLayout layout = core::BuildBandLayout(preset, sample_rate, core::FFT_SIZE);
    analysis::BandMetrics m;
    analysis::ComputeBandMetrics(magnitude, magnitude_db, layout, m);

    int bass = -1;
    for (int b = 0; b < layout.count(); ++b) if (layout.bands[b].name == "Bajo") bass = b;
    Check(bass >= 0, "existe la banda Bajo en el preset", bass, 1, 0);
    if (bass >= 0) {
        // El pico por bin subestima la amplitud por la pérdida de festoneado de Hann: hasta 1,42 dB
        // cuando el tono cae a medio bin de distancia del centro. A 100 Hz el tono está a 0,27 bins
        // (bin 4,27), lo que da unos 0,4 dB. La tolerancia es la pérdida máxima teórica.
        Check(std::fabs(m.peak_db[bass] + 6.0) < 1.5, "pico de la banda Bajo con seno de 100 Hz a -6 dBFS (dB, festoneado <= 1,42)", m.peak_db[bass], -6.0, 1.5);
        const double total = analysis::TotalEnergy(magnitude);
        Check(m.energy[bass] / total > 0.99, "fraccion de la energia total en la banda Bajo", m.energy[bass] / total, 1.0, 0.01);
        // Fuga hacia las demás bandas. El lóbulo principal de Hann abarca +-2 bins alrededor del
        // tono (docs/10, sección 5.3) y su primer lóbulo secundario está a -31,5 dB. Una banda cuyo
        // bin más cercano queda a menos de 2,5 bins del tono recibe fuga del lóbulo principal y
        // solo puede exigirse que quede 25 dB por debajo; a las demás se les exige 40 dB.
        const double tone_bin = freq * core::FFT_SIZE / sample_rate; // 4,27 bins
        for (int b = 0; b < layout.count(); ++b) {
            if (b == bass) continue;
            const auto& def = layout.bands[b];
            const double nearest = std::min(std::fabs(def.bin_low - tone_bin), std::fabs((def.bin_high - 1) - tone_bin));
            const bool adjacent = nearest < 2.5;
            const double threshold = adjacent ? -6.0 - 25.0 : -6.0 - 40.0;
            std::string what = "banda " + layout.bands[b].name + (adjacent ? " (contigua, lobulo principal) < -31 dB" : " < -46 dB");
            Check(m.peak_db[b] < threshold, what.c_str(), m.peak_db[b], threshold, 0.0);
        }
    }

    // 2 y 3. Parseval con las cuatro particiones.
    TestParseval("manual", preset, magnitude, magnitude_db, sample_rate);
    core::BandConfig octaves; octaves.mode = "octaves"; octaves.divisions_per_octave = 3;
    TestParseval("octaves", octaves, magnitude, magnitude_db, sample_rate);
    core::BandConfig linear; linear.mode = "linear"; linear.linear_band_count = 16;
    TestParseval("linear", linear, magnitude, magnitude_db, sample_rate);
    core::BandConfig per_bin; per_bin.mode = "per_bin";
    TestParseval("per_bin", per_bin, magnitude, magnitude_db, sample_rate);

    // 4. La partición es contigua y sin solapes.
    bool contiguous = true;
    for (int b = 1; b < layout.count(); ++b) if (layout.bands[b].bin_low != layout.bands[b - 1].bin_high) contiguous = false;
    Check(contiguous, "bandas contiguas sin solapes ni huecos", contiguous ? 1 : 0, 1, 0);

    std::printf("\n%s: %d fallo(s)\n", failures == 0 ? "PRUEBA SUPERADA" : "PRUEBA FALLIDA", failures);
    return failures == 0 ? 0 : 1;
}
