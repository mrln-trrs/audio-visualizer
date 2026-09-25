#include "core/types.h"

namespace core {

const char* VisualizerModeName(int mode) {
    switch (mode) {
    case MODE_BARS: return "Barras";
    case MODE_RADIAL: return "Radial";
    case MODE_WAVEFORM: return "Osciloscopio";
    case MODE_WATERFALL: return "Cascada";
    case MODE_BAND_METERS: return "Medidores";
    default: return "Desconocido";
    }
}

} // namespace core
