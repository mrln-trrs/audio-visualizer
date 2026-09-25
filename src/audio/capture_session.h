#pragma once

#include "core/shared_state.h"

// Una sesión de captura WASAPI en modo loopback sobre un endpoint.
namespace audio {

// Abre el endpoint pedido en VisualizerData (o el predeterminado), captura hasta que se pida
// terminar, el usuario cambie de dispositivo o el dispositivo deje de ser válido.
// Devuelve true si el hilo debe abrir una sesión nueva; false si debe terminar.
// Requiere COM inicializado en el hilo llamante.
bool RunCaptureSession(core::AudioData& audio, core::VisualizerData& vis);

} // namespace audio
