#include "core/band_layout.h"

#include <algorithm>
#include <cmath>

namespace core {
namespace {

// Frecuencias de corte (K+1 valores, incluidos f_min y f_max) según el modo.
std::vector<float> Edges(const BandConfig& cfg, int fft_size, int sample_rate) {
    std::vector<float> edges;
    if (cfg.mode == "octaves") {
        const double ratio = std::pow(2.0, 1.0 / std::max(1, cfg.divisions_per_octave));
        double f = cfg.f_min;
        edges.push_back(static_cast<float>(f));
        while (f * ratio < cfg.f_max * 0.999 && edges.size() < 512) {
            f *= ratio;
            edges.push_back(static_cast<float>(f));
        }
        edges.push_back(cfg.f_max);
    }
    else if (cfg.mode == "linear") {
        const int n = std::clamp(cfg.linear_band_count, 1, 512);
        for (int i = 0; i <= n; ++i) edges.push_back(cfg.f_min + (cfg.f_max - cfg.f_min) * i / n);
    }
    else if (cfg.mode == "per_bin") {
        // Una banda por bin dentro del rango. Sin frecuencia de muestreo se aproxima con 48 kHz.
        const double bin_hz = static_cast<double>(sample_rate > 0 ? sample_rate : 48000) / fft_size;
        const int k0 = std::max(1, static_cast<int>(std::floor(cfg.f_min / bin_hz)));
        const int k1 = std::min(fft_size / 2, static_cast<int>(std::ceil(cfg.f_max / bin_hz)));
        for (int k = k0; k <= std::max(k0 + 1, k1); ++k) edges.push_back(static_cast<float>(k * bin_hz));
    }
    else { // manual
        edges.push_back(cfg.f_min);
        for (float c : cfg.cuts_hz) edges.push_back(c);
        edges.push_back(cfg.f_max);
    }
    return edges;
}

template <typename T>
void FitList(std::vector<T>& list, size_t count, const T& fill) {
    if (list.size() > count) list.resize(count);
    while (list.size() < count) list.push_back(fill);
}

} // namespace

int BandLayout::BandForBin(int bin) const {
    for (int i = 0; i < count(); ++i) {
        if (bin >= bands[i].bin_low && bin < bands[i].bin_high) return i;
    }
    return -1;
}

std::array<float, 3> DefaultBandColor(int index, int count) {
    // Matiz repartido de violeta a rojo pasando por azul, verde y amarillo.
    const float t = count > 1 ? static_cast<float>(index) / (count - 1) : 0.0f;
    const float h = (1.0f - t) * 0.78f; // 0.78 = violeta, 0 = rojo
    const float s = 0.85f, v = 1.0f;
    const float c = v * s, x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f)), m = v - c;
    float r = 0, g = 0, b = 0;
    const int sector = static_cast<int>(h * 6.0f) % 6;
    switch (sector) {
    case 0: r = c; g = x; break;
    case 1: r = x; g = c; break;
    case 2: g = c; b = x; break;
    case 3: g = x; b = c; break;
    case 4: r = x; b = c; break;
    default: r = c; b = x; break;
    }
    return { r + m, g + m, b + m };
}

int BandCount(const BandConfig& cfg, int fft_size, int sample_rate_hint) {
    return static_cast<int>(Edges(cfg, fft_size, sample_rate_hint).size()) - 1;
}

std::vector<std::string> ValidateBandConfig(BandConfig& cfg, float default_attack_ms, float default_release_ms) {
    std::vector<std::string> warnings;

    if (cfg.mode != "octaves" && cfg.mode != "linear" && cfg.mode != "manual" && cfg.mode != "per_bin") {
        warnings.push_back("bandas.mode debe ser octaves, linear, manual o per_bin; se usa manual.");
        cfg.mode = "manual";
    }
    cfg.f_min = std::clamp(cfg.f_min, 1.0f, 24000.0f);
    cfg.f_max = std::clamp(cfg.f_max, 2.0f, 24000.0f);
    if (cfg.f_max <= cfg.f_min * 1.01f) {
        warnings.push_back("bandas.f_max debe ser mayor que f_min; se usa el rango 20 a 20000 Hz.");
        cfg.f_min = 20.0f;
        cfg.f_max = 20000.0f;
    }
    cfg.divisions_per_octave = std::clamp(cfg.divisions_per_octave, 1, 24);
    cfg.linear_band_count = std::clamp(cfg.linear_band_count, 1, 512);

    // Cortes manuales: dentro del rango, únicos y crecientes.
    std::vector<float> cuts;
    for (float c : cfg.cuts_hz) {
        if (c > cfg.f_min && c < cfg.f_max) cuts.push_back(c);
        else warnings.push_back("bandas.cuts_hz: corte fuera del rango descartado.");
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.erase(std::unique(cuts.begin(), cuts.end()), cuts.end());
    if (cuts.size() != cfg.cuts_hz.size()) warnings.push_back("bandas.cuts_hz se ha reordenado o depurado.");
    cfg.cuts_hz = cuts;

    // Listas por banda. El recuento no depende de la frecuencia de muestreo salvo en per_bin,
    // donde las listas se generan y no se editan.
    const int k = BandCount(cfg, 2048, 48000);
    const size_t count = static_cast<size_t>(std::max(1, k));
    for (size_t i = cfg.names.size(); i < count; ++i) cfg.names.push_back("Banda " + std::to_string(i + 1));
    FitList(cfg.names, count, std::string("Banda"));
    FitList(cfg.attack_ms, count, default_attack_ms);
    FitList(cfg.release_ms, count, default_release_ms);
    FitList(cfg.gain, count, 1.0f);
    for (size_t i = cfg.colors_rgb.size(); i < count; ++i) cfg.colors_rgb.push_back(DefaultBandColor(static_cast<int>(i), static_cast<int>(count)));
    FitList(cfg.colors_rgb, count, std::array<float, 3>{ 1.0f, 1.0f, 1.0f });
    for (auto& a : cfg.attack_ms) a = std::clamp(a, 0.1f, 2000.0f);
    for (auto& r : cfg.release_ms) r = std::clamp(r, 0.1f, 5000.0f);
    for (auto& g : cfg.gain) g = std::clamp(g, 0.0f, 10.0f);
    cfg.mask_ramp_bins = std::clamp(cfg.mask_ramp_bins, 0.0f, 8.0f);
    return warnings;
}

BandLayout BuildBandLayout(const BandConfig& cfg, int sample_rate, int fft_size) {
    BandLayout layout;
    layout.sample_rate = sample_rate;
    layout.fft_size = fft_size;
    layout.bin_resolution_hz = sample_rate > 0 ? static_cast<float>(sample_rate) / fft_size : 0.0f;
    if (sample_rate <= 0) return layout;

    const std::vector<float> edges = Edges(cfg, fft_size, sample_rate);
    const int k = static_cast<int>(edges.size()) - 1;
    const int max_bin = fft_size / 2 + 1;
    const double bin_hz = layout.bin_resolution_hz;

    layout.bands.resize(k);
    int previous_high = 0;
    for (int i = 0; i < k; ++i) {
        BandDefinition& b = layout.bands[i];
        b.f_low_hz = edges[i];
        b.f_high_hz = edges[i + 1];
        // Bins por redondeo al centro; contiguos y sin solapes gracias al arrastre de previous_high.
        int lo = static_cast<int>(std::lround(b.f_low_hz / bin_hz));
        int hi = static_cast<int>(std::lround(b.f_high_hz / bin_hz));
        lo = std::clamp(std::max(lo, previous_high), 0, max_bin - 1);
        hi = std::clamp(hi, lo + 1, max_bin);
        b.too_narrow = (b.f_high_hz - b.f_low_hz) < bin_hz;
        b.bin_low = lo;
        b.bin_high = hi;
        previous_high = hi;

        const size_t idx = static_cast<size_t>(i);
        b.name = idx < cfg.names.size() ? cfg.names[idx] : ("Banda " + std::to_string(i + 1));
        b.color = idx < cfg.colors_rgb.size() ? cfg.colors_rgb[idx] : DefaultBandColor(i, k);
        b.attack_ms = idx < cfg.attack_ms.size() ? cfg.attack_ms[idx] : 12.0f;
        b.release_ms = idx < cfg.release_ms.size() ? cfg.release_ms[idx] : 160.0f;
        b.gain = idx < cfg.gain.size() ? cfg.gain[idx] : 1.0f;
    }
    return layout;
}

} // namespace core
