#include "core/config.h"
#include "core/types.h"
#include "core/constants.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace core {
namespace {

template <typename T>
void Read(const json& j, const char* key, T& out) {
    if (!j.contains(key)) return;
    try {
        out = j.at(key).get<T>();
    }
    catch (const json::exception& e) {
        std::cerr << "Config: la clave '" << key << "' tiene un tipo inesperado (" << e.what()
                  << "), se mantiene el valor por defecto." << std::endl;
    }
}

void ReadColor(const json& j, const char* key, std::vector<float>& out) {
    std::vector<float> color;
    Read(j, key, color);
    if (color.size() == 3) {
        out = color;
    }
    else if (!color.empty()) {
        std::cerr << "Config: " << key << " debe tener 3 componentes, se mantiene el color por defecto." << std::endl;
    }
}

void ReadBands(const json& j, BandConfig& b) {
    Read(j, "mode", b.mode);
    Read(j, "f_min", b.f_min);
    Read(j, "f_max", b.f_max);
    Read(j, "divisions_per_octave", b.divisions_per_octave);
    Read(j, "linear_band_count", b.linear_band_count);
    Read(j, "cuts_hz", b.cuts_hz);
    Read(j, "names", b.names);
    Read(j, "attack_ms", b.attack_ms);
    Read(j, "release_ms", b.release_ms);
    Read(j, "gain", b.gain);
    Read(j, "mask_ramp_bins", b.mask_ramp_bins);
    Read(j, "bars_inherit_dynamics", b.bars_inherit_dynamics);
    if (j.contains("colors_rgb")) {
        try {
            std::vector<std::vector<float>> colors = j.at("colors_rgb").get<std::vector<std::vector<float>>>();
            b.colors_rgb.clear();
            for (const auto& c : colors) {
                if (c.size() == 3) b.colors_rgb.push_back({ c[0], c[1], c[2] });
            }
        }
        catch (const json::exception& e) {
            std::cerr << "Config: bandas.colors_rgb con formato inesperado (" << e.what() << ")." << std::endl;
        }
    }
}

json BandsToJson(const BandConfig& b) {
    std::vector<std::vector<float>> colors;
    for (const auto& c : b.colors_rgb) colors.push_back({ c[0], c[1], c[2] });
    return {
        { "mode", b.mode },
        { "f_min", b.f_min },
        { "f_max", b.f_max },
        { "divisions_per_octave", b.divisions_per_octave },
        { "linear_band_count", b.linear_band_count },
        { "cuts_hz", b.cuts_hz },
        { "names", b.names },
        { "attack_ms", b.attack_ms },
        { "release_ms", b.release_ms },
        { "gain", b.gain },
        { "colors_rgb", colors },
        { "mask_ramp_bins", b.mask_ramp_bins },
        { "bars_inherit_dynamics", b.bars_inherit_dynamics }
    };
}

} // namespace

bool operator==(const AnalysisConfig& a, const AnalysisConfig& b) {
    return a.multi_resolution == b.multi_resolution && a.low_band_fft_size == b.low_band_fft_size &&
           a.low_band_max_hz == b.low_band_max_hz && a.high_band_fft_size == b.high_band_fft_size &&
           a.high_band_min_hz == b.high_band_min_hz;
}

namespace {
int ClampPowerOfTwo(int v, int lo, int hi) {
    int p = lo;
    while (p * 2 <= v && p * 2 <= hi) p *= 2;
    return p;
}
}

std::vector<std::string> ValidateAnalysisConfig(AnalysisConfig& c) {
    std::vector<std::string> warnings;
    const int low = ClampPowerOfTwo(std::max(c.low_band_fft_size, FFT_SIZE), FFT_SIZE, MAX_LOW_BAND_FFT_SIZE);
    if (low != c.low_band_fft_size) { warnings.push_back("analisis.low_band_fft_size debe ser potencia de dos entre " + std::to_string(FFT_SIZE) + " y " + std::to_string(MAX_LOW_BAND_FFT_SIZE) + "; se usa " + std::to_string(low)); c.low_band_fft_size = low; }
    const int high = ClampPowerOfTwo(std::max(c.high_band_fft_size, 256), 256, FFT_SIZE);
    if (high != c.high_band_fft_size) { warnings.push_back("analisis.high_band_fft_size debe ser potencia de dos entre 256 y " + std::to_string(FFT_SIZE) + "; se usa " + std::to_string(high)); c.high_band_fft_size = high; }
    c.low_band_max_hz = std::clamp(c.low_band_max_hz, 20.0f, 2000.0f);
    c.high_band_min_hz = std::clamp(c.high_band_min_hz, c.low_band_max_hz + 1.0f, 20000.0f);
    return warnings;
}

bool operator==(const BandConfig& a, const BandConfig& b) {
    return a.mode == b.mode && a.f_min == b.f_min && a.f_max == b.f_max &&
           a.divisions_per_octave == b.divisions_per_octave && a.linear_band_count == b.linear_band_count &&
           a.cuts_hz == b.cuts_hz && a.names == b.names && a.attack_ms == b.attack_ms &&
           a.release_ms == b.release_ms && a.gain == b.gain && a.colors_rgb == b.colors_rgb &&
           a.mask_ramp_bins == b.mask_ramp_bins && a.bars_inherit_dynamics == b.bars_inherit_dynamics;
}

void LoadConfig(SharedConfigData& shared, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Config: no se pudo abrir " << filename << ", se usan los valores por defecto." << std::endl;
        return;
    }

    json data;
    try {
        file >> data;
    }
    catch (const json::parse_error& e) {
        std::cerr << "Config: error de parseo en " << filename << ": " << e.what() << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(shared.mtx);
    VisualizerConfig& c = shared.config;

    if (data.contains("estilos")) {
        const json& estilos = data["estilos"];
        Read(estilos, "attack_ms", c.attack_ms);
        Read(estilos, "release_ms", c.release_ms);
        Read(estilos, "amplitude_factor", c.amplitude_factor);
        Read(estilos, "dynamic_range_db", c.dynamic_range_db);
        Read(estilos, "bin_grouping_factor", c.bin_grouping_factor);
        Read(estilos, "frequency_scale", c.frequency_scale);
        Read(estilos, "min_frequency", c.min_frequency);
        Read(estilos, "max_frequency", c.max_frequency);
        Read(estilos, "vsync", c.vsync);
        Read(estilos, "max_fps", c.max_fps);
        Read(estilos, "visual_mode", c.visual_mode);
        Read(estilos, "peak_hold_enabled", c.peak_hold_enabled);
        Read(estilos, "peak_hold_time_ms", c.peak_hold_time_ms);
        Read(estilos, "peak_decay_speed", c.peak_decay_speed);
        Read(estilos, "selected_device_name", c.selected_device_name);
        ReadColor(estilos, "base_color_rgb", c.base_color_rgb);
        ReadColor(estilos, "peak_color_rgb", c.peak_color_rgb);
        Read(estilos, "material", c.material);
        Read(estilos, "material_opacity", c.material_opacity);
        Read(estilos, "animations", c.animations);
        Read(estilos, "respect_system_effects", c.respect_system_effects);
    }

    if (c.material != "none" && c.material != "acrylic_app" && c.material != "mica" && c.material != "acrylic_system") {
        std::cerr << "Config: material debe ser none, acrylic_app, mica o acrylic_system; se usa acrylic_app." << std::endl;
        c.material = "acrylic_app";
    }
    if (c.material_opacity < 0.6f || c.material_opacity > 0.95f) {
        std::cerr << "Config: material_opacity fuera de [0.6, 0.95], se acota." << std::endl;
        c.material_opacity = c.material_opacity < 0.6f ? 0.6f : 0.95f;
    }

    if (c.frequency_scale != "linear" && c.frequency_scale != "log") {
        std::cerr << "Config: frequency_scale debe ser \"linear\" o \"log\", se usa \"linear\"." << std::endl;
        c.frequency_scale = "linear";
    }
    if (c.visual_mode < 0 || c.visual_mode >= MODE_COUNT) {
        std::cerr << "Config: visual_mode fuera de rango, se usa 0." << std::endl;
        c.visual_mode = 0;
    }

    if (data.contains("bandas")) ReadBands(data["bandas"], c.bands);
    for (const std::string& w : ValidateBandConfig(c.bands, c.attack_ms, c.release_ms)) {
        std::cerr << "Config: " << w << std::endl;
    }

    if (data.contains("analisis")) {
        const json& a = data["analisis"];
        Read(a, "multi_resolution", c.analysis.multi_resolution);
        Read(a, "low_band_fft_size", c.analysis.low_band_fft_size);
        Read(a, "low_band_max_hz", c.analysis.low_band_max_hz);
        Read(a, "high_band_fft_size", c.analysis.high_band_fft_size);
        Read(a, "high_band_min_hz", c.analysis.high_band_min_hz);
    }
    for (const std::string& w : ValidateAnalysisConfig(c.analysis)) {
        std::cerr << "Config: " << w << std::endl;
    }
}

bool SaveConfig(SharedConfigData& shared, const std::string& filename) {
    VisualizerConfig c;
    {
        std::lock_guard<std::mutex> lock(shared.mtx);
        c = shared.config;
    }

    json j;
    j["estilos"] = {
        { "attack_ms", c.attack_ms },
        { "release_ms", c.release_ms },
        { "amplitude_factor", c.amplitude_factor },
        { "dynamic_range_db", c.dynamic_range_db },
        { "base_color_rgb", c.base_color_rgb },
        { "bin_grouping_factor", c.bin_grouping_factor },
        { "frequency_scale", c.frequency_scale },
        { "min_frequency", c.min_frequency },
        { "max_frequency", c.max_frequency },
        { "vsync", c.vsync },
        { "max_fps", c.max_fps },
        { "visual_mode", c.visual_mode },
        { "peak_hold_enabled", c.peak_hold_enabled },
        { "peak_hold_time_ms", c.peak_hold_time_ms },
        { "peak_decay_speed", c.peak_decay_speed },
        { "peak_color_rgb", c.peak_color_rgb },
        { "selected_device_name", c.selected_device_name },
        { "material", c.material },
        { "material_opacity", c.material_opacity },
        { "animations", c.animations },
        { "respect_system_effects", c.respect_system_effects }
    };
    j["bandas"] = BandsToJson(c.bands);
    j["analisis"] = {
        { "multi_resolution", c.analysis.multi_resolution },
        { "low_band_fft_size", c.analysis.low_band_fft_size },
        { "low_band_max_hz", c.analysis.low_band_max_hz },
        { "high_band_fft_size", c.analysis.high_band_fft_size },
        { "high_band_min_hz", c.analysis.high_band_min_hz }
    };

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Config: no se pudo abrir " << filename << " para escritura." << std::endl;
        return false;
    }
    try {
        file << j.dump(2) << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Config: error al guardar en " << filename << ": " << e.what() << std::endl;
        return false;
    }
}

} // namespace core
