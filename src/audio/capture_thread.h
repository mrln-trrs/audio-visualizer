#pragma once

#include "core/shared_state.h"

// Hilo de captura: COM, prioridad de audio, enumeración inicial y bucle de sesiones.
namespace audio {

void CaptureThread(core::AudioData& audio, core::VisualizerData& vis);

} // namespace audio
