#pragma once

#include "common.h"
#include "config.h"

// Hilo de renderizado. Devuelve cuando se cierra la ventana.
// Recibe AudioData solo para mostrar la frecuencia de muestreo en el título.
void RenderThread(VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData, AudioData& sharedAudioData);
