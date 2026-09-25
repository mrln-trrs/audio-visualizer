#pragma once

#include "core/shared_state.h"
#include "core/config.h"

// Hilo de análisis: ventana deslizante, Hann, FFT, magnitud, dB, bandas y publicación.
namespace analysis {

void AnalysisThread(core::AudioData& audio, core::VisualizerData& vis, core::SharedConfigData& config);

} // namespace analysis
