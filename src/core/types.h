#pragma once

#include <string>

// Tipos simples compartidos entre capas. Sin dependencias de Windows ni de OpenGL.
namespace core {

// Modos de visualización. Los valores enteros se guardan en config.json, así que no se reordenan.
enum VisualizerMode {
    MODE_BARS = 0,
    MODE_RADIAL = 1,
    MODE_WAVEFORM = 2,
    MODE_WATERFALL = 3,
    MODE_BAND_METERS = 4,
    MODE_STACKED_OSCILLOSCOPE = 5,
    MODE_COUNT
};

const char* VisualizerModeName(int mode);

// Dispositivo de salida de audio de Windows.
struct AudioDeviceInfo {
    std::wstring id;         // identificador WASAPI (IMMDevice::GetId)
    std::string name;        // nombre amigable en UTF-8
    bool is_default = false; // es el dispositivo de salida por defecto del sistema
};

} // namespace core
