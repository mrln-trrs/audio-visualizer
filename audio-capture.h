#pragma once

#include "common.h"
#include <vector>

// Enumerar dispositivos de salida de audio activos en Windows
std::vector<AudioDeviceInfo> EnumerateAudioDevices();

// Hilo de captura WASAPI loopback
void AudioCaptureThread(AudioData& sharedData, VisualizerData& visualizerData);
