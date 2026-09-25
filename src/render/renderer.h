#pragma once

#include "core/shared_state.h"
#include "core/config.h"

// Bucle de render. Corre en el hilo principal y devuelve cuando se cierra la ventana.
namespace render {

void RenderThread(core::VisualizerData& vis, core::SharedConfigData& config, core::AudioData& audio);

} // namespace render
