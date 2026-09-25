#include "analysis/analysis_thread.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <fftw3.h>

#include "analysis/window_function.h"
#include "analysis/band_metrics.h"
#include "analysis/band_synthesizer.h"
#include "analysis/multi_resolution.h"
#include "core/band_layout.h"

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

// Copia las `count` muestras más recientes del anillo, en orden temporal. Debe llamarse con
// audio.mtx tomado. count no puede superar RING_SIZE.
void CopyLatest(const core::AudioData& audio, int count, std::vector<float>& dst) {
    dst.resize(count);
    const int64_t start = static_cast<int64_t>(audio.total_samples) - count;
    for (int n = 0; n < count; ++n) {
        const int64_t idx = start + n;
        dst[n] = (idx < 0) ? 0.0f : audio.ring[static_cast<uint64_t>(idx) & RING_MASK];
    }
}

void PreallocateFrame(core::AnalysisFrame& f) {
    f.magnitude.assign(SPECTRUM_BINS, 0.0f);
    f.magnitude_db.assign(SPECTRUM_BINS, -180.0f);
    f.phase.assign(SPECTRUM_BINS, 0.0f);
    f.mix_waveform.assign(core::WAVEFORM_SNAPSHOT_SIZE, 0.0f);
}

// Mantiene la partición en bandas sincronizada con la configuración y la frecuencia de muestreo.
class BandLayoutTracker {
public:
    // Devuelve true si la partición cambió.
    bool Refresh(core::SharedConfigData& shared, int sample_rate) {
        const uint64_t version = shared.version.load(std::memory_order_relaxed);
        bool config_changed = false;
        analysis_changed_ = false;
        if (version != seen_version_) {
            core::BandConfig bands;
            core::AnalysisConfig analysis;
            float attack = 12.0f, release = 160.0f;
            {
                std::lock_guard<std::mutex> lock(shared.mtx);
                bands = shared.config.bands;
                analysis = shared.config.analysis;
                attack = shared.config.attack_ms;
                release = shared.config.release_ms;
            }
            seen_version_ = version;
            core::ValidateBandConfig(bands, attack, release);
            core::ValidateAnalysisConfig(analysis);
            if (bands != bands_) {
                bands_ = bands;
                config_changed = true;
            }
            if (analysis != analysis_) {
                analysis_ = analysis;
                analysis_changed_ = true;
            }
        }
        if (config_changed || sample_rate != layout_.sample_rate) {
            layout_ = core::BuildBandLayout(bands_, sample_rate, FFT_SIZE);
            ++layout_version_;
            return true;
        }
        return false;
    }

    const core::BandLayout& layout() const { return layout_; }
    uint64_t version() const { return layout_version_; }
    float mask_ramp_bins() const { return bands_.mask_ramp_bins; }
    const core::AnalysisConfig& analysis() const { return analysis_; }
    bool analysis_changed() const { return analysis_changed_; }

private:
    uint64_t seen_version_ = ~0ull;
    core::BandConfig bands_;
    core::AnalysisConfig analysis_;
    bool analysis_changed_ = false;
    core::BandLayout layout_;
    uint64_t layout_version_ = 0;
};

} // namespace

void AnalysisThread(core::AudioData& audio, core::VisualizerData& vis, core::SharedConfigData& config) {
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

    BandLayoutTracker bands;
    BandMetrics band_metrics; // vectores de trabajo que se intercambian con la trama de salida
    BandSynthesizer synth(FFT_SIZE, HOP_SIZE);
    bool synth_active = false; // configurado para la partición actual y con acumuladores válidos
    std::vector<float> band_waves;
    MultiResolution multi;          // resolución variable (fase D), solo si la configuración lo pide
    std::vector<float> latest_long; // muestras para la FFT larga
    bool multi_configured = false;
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
            if (bands.analysis().multi_resolution) CopyLatest(audio, bands.analysis().low_band_fft_size, latest_long);
            consumed = audio.total_samples;
            sample_position = consumed;
        }

        const int sample_rate = audio.sample_rate.load();
        if (sample_rate <= 0) continue;

        const bool layout_changed = bands.Refresh(config, sample_rate);
        const core::BandLayout& layout = bands.layout();
        if (layout_changed) synth_active = false;
        if (bands.analysis_changed()) multi_configured = false;

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
        double flux = 0.0, weighted = 0.0, total = 0.0, energy = 0.0;
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
            energy += static_cast<double>(m) * m;
            previous_magnitude[k] = m;
        }
        out.spectral_flux = static_cast<float>(flux);
        out.spectral_centroid_hz = total > 0.0 ? static_cast<float>(weighted / total) : 0.0f;
        out.total_energy = static_cast<float>(energy);

        // Métricas por banda (docs/11, sección 3). Los vectores solo cambian de tamaño cuando
        // cambia la partición.
        out.band_layout_version = bands.version();
        band_metrics.energy.swap(out.band_energy);
        band_metrics.rms.swap(out.band_rms);
        band_metrics.peak_db.swap(out.band_peak_db);
        ComputeBandMetrics(out.magnitude, out.magnitude_db, layout, band_metrics);
        band_metrics.energy.swap(out.band_energy);
        band_metrics.rms.swap(out.band_rms);
        band_metrics.peak_db.swap(out.band_peak_db);

        std::copy(waveform.begin(), waveform.end(), out.mix_waveform.begin());

        // Ondas por banda (fase C): solo si algún modo las pide y la partición es razonable.
        const bool want_waves = vis.band_waveforms_requested.load(std::memory_order_relaxed) &&
                                layout.count() > 0 && layout.count() <= core::MAX_WAVEFORM_BANDS;
        if (want_waves) {
            if (!synth_active) {
                synth.Configure(layout, bands.mask_ramp_bins(), window);
                synth_active = true;
            }
            band_waves.swap(out.band_waveform);
            synth.Process(fft_out, band_waves);
            band_waves.swap(out.band_waveform);
            out.band_waveform_rows = synth.rows();
            out.band_waveform_valid = true;
            // Las hop_size muestras de la mezcla alineadas con las ondas reconstruidas: las más
            // antiguas de la ventana, que ya recibieron todas las contribuciones del solapamiento.
            out.mix_hop_waveform.assign(raw.begin(), raw.begin() + HOP_SIZE);
        }
        else {
            synth_active = false; // al reactivar se reconfigura y se vacían los acumuladores
            out.band_waveform_valid = false;
            out.band_waveform_rows = 0;
        }

        // Resolución variable (fase D): FFT larga en graves y corta en agudos sobre las mismas
        // muestras finales. La copia larga se tomó en el snapshot de esta trama. La primera trama
        // tras activar la opción no la tiene todavía y se publica sin espectros adicionales.
        if (bands.analysis().multi_resolution && static_cast<int>(latest_long.size()) == bands.analysis().low_band_fft_size) {
            if (!multi_configured) {
                multi.Configure(bands.analysis());
                multi_configured = true;
            }
            multi.Process(latest_long, out);
        }
        else {
            out.multi_resolution = false;
            out.low_fft_size = 0;
            out.high_fft_size = 0;
        }

        vis.analysis.Publish();
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fft_out);
}

} // namespace analysis
