#include "analysis/analysis_thread.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fftw3.h>

#include "analysis/window_function.h"

namespace analysis {
namespace {

using core::FFT_SIZE;
using core::HOP_SIZE;
using core::RING_MASK;
using core::SPECTRUM_BINS;

// Copia las FFT_SIZE muestras más recientes del anillo (crudas) y las últimas
// WAVEFORM_SNAPSHOT_SIZE para el osciloscopio. Debe llamarse con audio.mtx tomado.
void SnapshotFromRing(const core::AudioData& audio, std::vector<float>& raw, std::vector<float>& waveform) {
    const uint64_t end = audio.total_samples;
    const int64_t start = static_cast<int64_t>(end) - FFT_SIZE;
    for (int n = 0; n < FFT_SIZE; ++n) {
        const int64_t idx = start + n;
        raw[n] = (idx < 0) ? 0.0f : audio.ring[static_cast<uint64_t>(idx) & RING_MASK];
    }
    const int wave_len = static_cast<int>(waveform.size());
    const int64_t wave_start = static_cast<int64_t>(end) - wave_len;
    for (int n = 0; n < wave_len; ++n) {
        const int64_t idx = wave_start + n;
        waveform[n] = (idx < 0) ? 0.0f : audio.ring[static_cast<uint64_t>(idx) & RING_MASK];
    }
}

void PreallocateFrame(core::AnalysisFrame& f) {
    f.magnitude.assign(SPECTRUM_BINS, 0.0f);
    f.magnitude_db.assign(SPECTRUM_BINS, -180.0f);
    f.phase.assign(SPECTRUM_BINS, 0.0f);
    f.mix_waveform.assign(core::WAVEFORM_SNAPSHOT_SIZE, 0.0f);
}

} // namespace

void AnalysisThread(core::AudioData& audio, core::VisualizerData& vis) {
    std::cout << "Analisis: hilo iniciado (FFT " << FFT_SIZE << ", salto " << HOP_SIZE << ", " << SPECTRUM_BINS << " bins)." << std::endl;

    const AnalysisWindow window = MakePeriodicHann(FFT_SIZE);

    std::vector<float> raw(FFT_SIZE);
    std::vector<float> frame(FFT_SIZE);
    std::vector<float> waveform(core::WAVEFORM_SNAPSHOT_SIZE, 0.0f);
    std::vector<float> previous_magnitude(SPECTRUM_BINS, 0.0f);

    fftwf_complex* fft_out = fftwf_alloc_complex(SPECTRUM_BINS);
    if (!fft_out) {
        std::cerr << "Analisis: sin memoria para la FFT." << std::endl;
        return;
    }
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(FFT_SIZE, frame.data(), fft_out, FFTW_MEASURE);

    // Sin asignaciones de memoria en el bucle: los tres búferes se preasignan aquí.
    for (int i = 0; i < core::TripleBuffer<core::AnalysisFrame>::kSlots; ++i) {
        PreallocateFrame(vis.analysis.slot(i));
    }

    uint64_t consumed = 0; // total_samples en la última ventana procesada
    uint64_t sequence = 0;

    while (true) {
        uint64_t sample_position = 0;
        {
            std::unique_lock<std::mutex> lock(audio.mtx);
            audio.cv.wait(lock, [&] {
                return vis.should_terminate.load() || audio.total_samples >= consumed + HOP_SIZE;
            });
            if (vis.should_terminate.load()) break;
            // Siempre las muestras más recientes: si llegan varios saltos de golpe se salta al
            // final en lugar de acumular retraso.
            SnapshotFromRing(audio, raw, waveform);
            consumed = audio.total_samples;
            sample_position = consumed;
        }

        const int sample_rate = audio.sample_rate.load();
        if (sample_rate <= 0) continue;

        // Métricas en el dominio del tiempo sobre la ventana sin enventanar.
        double sum_sq = 0.0;
        float peak = 0.0f;
        for (int n = 0; n < FFT_SIZE; ++n) {
            const float x = raw[n];
            sum_sq += static_cast<double>(x) * x;
            peak = std::max(peak, std::fabs(x));
            frame[n] = x * window.coefficients[n];
        }

        fftwf_execute(plan);

        core::AnalysisFrame& out = vis.analysis.BeginWrite();
        out.sequence = ++sequence;
        out.sample_position = sample_position;
        out.sample_rate = sample_rate;
        out.fft_size = FFT_SIZE;
        out.hop_size = HOP_SIZE;
        out.rms = static_cast<float>(std::sqrt(sum_sq / FFT_SIZE));
        out.peak = peak;

        const float bin_hz = static_cast<float>(sample_rate) / FFT_SIZE;
        double flux = 0.0, weighted = 0.0, total = 0.0;
        for (int k = 0; k < SPECTRUM_BINS; ++k) {
            const float re = fft_out[k][0];
            const float im = fft_out[k][1];
            const float m = std::sqrt(re * re + im * im) * window.normalization;
            out.magnitude[k] = m;
            out.magnitude_db[k] = 20.0f * std::log10(m + 1e-9f);
            out.phase[k] = std::atan2(im, re);
            flux += std::max(0.0f, m - previous_magnitude[k]);
            weighted += static_cast<double>(k) * bin_hz * m;
            total += m;
            previous_magnitude[k] = m;
        }
        out.spectral_flux = static_cast<float>(flux);
        out.spectral_centroid_hz = total > 0.0 ? static_cast<float>(weighted / total) : 0.0f;
        std::copy(waveform.begin(), waveform.end(), out.mix_waveform.begin());

        vis.analysis.Publish();
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fft_out);
}

} // namespace analysis
