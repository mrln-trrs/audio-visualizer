#pragma once

#include "core/shared_state.h"

// Hilo de análisis: ventana deslizante, Hann, FFT, magnitud, fase, dB, métricas globales y
// publicación de la trama de análisis en el triple búfer. No depende de la configuración
// visual: el mapeo a barras y la normalización a [0, 1] son responsabilidad del render.
namespace analysis {

void AnalysisThread(core::AudioData& audio, core::VisualizerData& vis);

} // namespace analysis
