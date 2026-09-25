#include "config.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

template <typename T>
void Read(const json& j, const char* key, T& out) {
    if (j.contains(key)) {
        try {
            out = j.at(key).get<T>();
        }
        catch (const json::exception& e) {
            std::cerr << "Config: la clave '" << key << "' tiene un tipo inesperado (" << e.what() << "), se mantiene el valor por defecto." << std::endl;
        }
    }
}

} // namespace

void LoadConfig(SharedConfigData& sharedConfigData, const std::string& filename) {
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

    if (!data.contains("estilos")) {
        return;
    }
    const json& estilos = data["estilos"];

    std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
    VisualizerConfig& c = sharedConfigData.config;

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

    std::vector<float> color;
    Read(estilos, "base_color_rgb", color);
    if (color.size() == 3) {
        c.base_color_rgb = color;
    }
    else if (!color.empty()) {
        std::cerr << "Config: base_color_rgb debe tener 3 componentes, se mantiene el color por defecto." << std::endl;
    }

    if (c.frequency_scale != "linear" && c.frequency_scale != "log") {
        std::cerr << "Config: frequency_scale debe ser \"linear\" o \"log\", se usa \"linear\"." << std::endl;
        c.frequency_scale = "linear";
    }
}
