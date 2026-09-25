#include "audio-processing.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fftw3.h>

namespace {

constexpr double kPi = 3.14159265358979323846;

// Posición fraccional (en bins de la FFT) del inicio y fin de cada barra.
struct BandMap {
    std::vector<float> lo;
    std::vector<float> hi;
};

BandMap BuildBandMap(int num_bars, const VisualizerConfig& cfg, double bin_resolution_hz) {
    BandMap map;
    map.lo.resize(num_bars);
    map.hi.resize(num_bars);
    const double max_bin = FFT_SIZE / 2;

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

// Valor de una banda: si abarca menos de un bin, interpola en su centro (evita barras
// permanentemente vacías); si abarca varios, toma el pico.
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

} // namespace

void AudioProcessingThread(AudioData& sharedData, VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData) {
    std::cout << "Procesado: hilo iniciado (FFT " << FFT_SIZE << ", salto " << HOP_SIZE << ")." << std::endl;

    VisualizerConfig cfg;
    {
        std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
        cfg = sharedConfigData.config;
    }

    // Ventana de Hann y factor de normalización: un seno a escala completa da magnitud 1.0.
    std::vector<float> window(FFT_SIZE);
    double window_sum = 0.0;
    for (int n = 0; n < FFT_SIZE; ++n) {
        const double w = 0.5 * (1.0 - std::cos(2.0 * kPi * n / (FFT_SIZE - 1)));
        window[n] = static_cast<float>(w);
        window_sum += w;
    }
    const float norm = static_cast<float>(2.0 / window_sum);

    std::vector<float> frame(FFT_SIZE);
    fftwf_complex* fft_out = fftwf_alloc_complex(FFT_SIZE / 2 + 1);
    if (!fft_out) {
        std::cerr << "Procesado: sin memoria para la FFT." << std::endl;
        return;
    }
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(FFT_SIZE, frame.data(), fft_out, FFTW_MEASURE);

    std::vector<float> magnitude(FFT_SIZE / 2 + 1);
    std::vector<float> spectrum;
    BandMap bands;
    int bands_num_bars = -1;
    int bands_sample_rate = 0;

    uint64_t consumed = 0; // total_samples en la última ventana procesada
    uint64_t seen_config_version = 0;
    std::vector<float> waveform_snapshot(1024, 0.0f);

    while (true) {
        {
            std::unique_lock<std::mutex> lock(sharedData.mtx);
            sharedData.cv.wait(lock, [&] {
                return sharedVisualizerData.should_terminate.load() || sharedData.total_samples >= consumed + HOP_SIZE;
            });
            if (sharedVisualizerData.should_terminate.load()) break;

            // Copiar siempre las FFT_SIZE muestras más recientes: mínima latencia.
            const uint64_t end = sharedData.total_samples;
            const int64_t start = static_cast<int64_t>(end) - FFT_SIZE;
            for (int n = 0; n < FFT_SIZE; ++n) {
                const int64_t idx = start + n;
                frame[n] = (idx < 0) ? 0.0f : sharedData.ring[static_cast<uint64_t>(idx) & RING_MASK] * window[n];
            }

            // Extraer las últimas muestras crudas para el osciloscopio en el dominio del tiempo
            const int wave_len = static_cast<int>(waveform_snapshot.size());
            const int64_t wave_start = static_cast<int64_t>(end) - wave_len;
            for (int n = 0; n < wave_len; ++n) {
                const int64_t idx = wave_start + n;
                waveform_snapshot[n] = (idx < 0) ? 0.0f : sharedData.ring[static_cast<uint64_t>(idx) & RING_MASK];
            }

            consumed = end;
        }

        // Si la configuración cambió en la UI, recargarla y forzar reconstrucción de bandas
        const uint64_t cur_cfg_ver = sharedConfigData.version.load(std::memory_order_relaxed);
        if (cur_cfg_ver != seen_config_version) {
            std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
            cfg = sharedConfigData.config;
            seen_config_version = cur_cfg_ver;
            bands_num_bars = -1;
        }

        const int sample_rate = sharedData.sample_rate.load();
        if (sample_rate <= 0) continue;

        fftwf_execute(plan);
        for (int k = 0; k <= FFT_SIZE / 2; ++k) {
            const float re = fft_out[k][0];
            const float im = fft_out[k][1];
            magnitude[k] = std::sqrt(re * re + im * im) * norm;
        }

        const int num_bars = std::max(1, sharedVisualizerData.atomic_num_bars.load());
        if (num_bars != bands_num_bars || sample_rate != bands_sample_rate) {
            bands = BuildBandMap(num_bars, cfg, static_cast<double>(sample_rate) / FFT_SIZE);
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
            std::lock_guard<std::mutex> lock(sharedVisualizerData.mtx);
            sharedVisualizerData.spectrum.swap(spectrum);
            sharedVisualizerData.waveform = waveform_snapshot;
        }
        sharedVisualizerData.generation.fetch_add(1, std::memory_order_release);
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fft_out);
}
