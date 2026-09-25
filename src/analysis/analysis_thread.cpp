#include "analysis/analysis_thread.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fftw3.h>

#include "analysis/window_function.h"
#include "analysis/band_mapper.h"

namespace analysis {
namespace {

using core::FFT_SIZE;
using core::HOP_SIZE;
using core::RING_MASK;

// Copia las FFT_SIZE muestras más recientes del anillo, ya enventanadas, y las últimas
// WAVEFORM_SNAPSHOT_SIZE muestras crudas. Debe llamarse con audio.mtx tomado.
void SnapshotFromRing(const core::AudioData& audio, const AnalysisWindow& window, std::vector<float>& frame, std::vector<float>& waveform) {
    const uint64_t end = audio.total_samples;
    const int64_t start = static_cast<int64_t>(end) - FFT_SIZE;
    for (int n = 0; n < FFT_SIZE; ++n) {
        const int64_t idx = start + n;
        frame[n] = (idx < 0) ? 0.0f : audio.ring[static_cast<uint64_t>(idx) & RING_MASK] * window.coefficients[n];
    }
    const int wave_len = static_cast<int>(waveform.size());
    const int64_t wave_start = static_cast<int64_t>(end) - wave_len;
    for (int n = 0; n < wave_len; ++n) {
        const int64_t idx = wave_start + n;
        waveform[n] = (idx < 0) ? 0.0f : audio.ring[static_cast<uint64_t>(idx) & RING_MASK];
    }
}

} // namespace

void AnalysisThread(core::AudioData& audio, core::VisualizerData& vis, core::SharedConfigData& shared_config) {
    std::cout << "Analisis: hilo iniciado (FFT " << FFT_SIZE << ", salto " << HOP_SIZE << ")." << std::endl;

    core::VisualizerConfig cfg;
    uint64_t seen_config_version = 0;
    {
        std::lock_guard<std::mutex> lock(shared_config.mtx);
        cfg = shared_config.config;
        seen_config_version = shared_config.version.load();
    }

    const AnalysisWindow window = MakePeriodicHann(FFT_SIZE);

    std::vector<float> frame(FFT_SIZE);
    fftwf_complex* fft_out = fftwf_alloc_complex(FFT_SIZE / 2 + 1);
    if (!fft_out) {
        std::cerr << "Analisis: sin memoria para la FFT." << std::endl;
        return;
    }
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(FFT_SIZE, frame.data(), fft_out, FFTW_MEASURE);

    std::vector<float> magnitude(FFT_SIZE / 2 + 1);
    std::vector<float> spectrum;
    std::vector<float> waveform(core::WAVEFORM_SNAPSHOT_SIZE, 0.0f);
    BandMap bands;
    int bands_num_bars = -1;
    int bands_sample_rate = 0;
    uint64_t consumed = 0; // total_samples en la última ventana procesada

    while (true) {
        {
            std::unique_lock<std::mutex> lock(audio.mtx);
            audio.cv.wait(lock, [&] {
                return vis.should_terminate.load() || audio.total_samples >= consumed + HOP_SIZE;
            });
            if (vis.should_terminate.load()) break;
            // Siempre las muestras más recientes: si llegan varios saltos de golpe se salta al
            // final en lugar de acumular retraso.
            SnapshotFromRing(audio, window, frame, waveform);
            consumed = audio.total_samples;
        }

        // Recarga de configuración en caliente (cambios desde el HUD).
        const uint64_t version = shared_config.version.load(std::memory_order_relaxed);
        if (version != seen_config_version) {
            std::lock_guard<std::mutex> lock(shared_config.mtx);
            cfg = shared_config.config;
            seen_config_version = version;
            bands_num_bars = -1; // fuerza reconstruir el mapa de bandas
        }

        const int sample_rate = audio.sample_rate.load();
        if (sample_rate <= 0) continue;

        fftwf_execute(plan);
        for (int k = 0; k <= FFT_SIZE / 2; ++k) {
            const float re = fft_out[k][0];
            const float im = fft_out[k][1];
            magnitude[k] = std::sqrt(re * re + im * im) * window.normalization;
        }

        const int num_bars = std::max(1, vis.atomic_num_bars.load());
        if (num_bars != bands_num_bars || sample_rate != bands_sample_rate) {
            bands = BuildBandMap(num_bars, cfg, static_cast<double>(sample_rate) / FFT_SIZE, FFT_SIZE);
            bands_num_bars = num_bars;
            bands_sample_rate = sample_rate;
        }

        // Magnitud -> dB -> [0, 1] sobre el rango dinámico configurado.
        const float range_db = std::max(1.0f, cfg.dynamic_range_db);
        spectrum.resize(num_bars);
        for (int i = 0; i < num_bars; ++i) {
            const float m = SampleBand(magnitude, bands.lo[i], bands.hi[i]);
            const float db = 20.0f * std::log10(m + 1e-9f);
            spectrum[i] = std::clamp((db + range_db) / range_db, 0.0f, 1.0f);
        }

        {
            std::lock_guard<std::mutex> lock(vis.mtx);
            vis.spectrum.swap(spectrum);
            vis.waveform = waveform;
        }
        vis.generation.fetch_add(1, std::memory_order_release);
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fft_out);
}

} // namespace analysis
