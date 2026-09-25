#pragma once

#include "core/shared_state.h"
#include "core/config.h"

// Hilo de análisis: ventana deslizante, Hann, FFT, magnitud, fase, dB, métricas globales,
// métricas por banda y publicación de la trama en el triple búfer. De la configuración solo
// usa la sección de bandas (partición del espectro); el mapeo a barras y la normalización a
// [0, 1] son responsabilidad del render.
namespace analysis {

void AnalysisThread(core::AudioData& audio, core::VisualizerData& vis, core::SharedConfigData& config);

} // namespace analysis
