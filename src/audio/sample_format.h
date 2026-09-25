#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmreg.h>
#include <vector>

// Detección del formato de muestra de WASAPI y conversión a mono normalizado.
namespace audio {

enum class SampleFormat { Float32, Pcm16, Pcm24, Pcm32, Unknown };

// Interpreta WAVEFORMATEX o WAVEFORMATEXTENSIBLE. Devuelve Unknown si no está soportado.
SampleFormat DetectSampleFormat(const WAVEFORMATEX* wfx);

const char* SampleFormatName(SampleFormat format);

// Mezcla un paquete intercalado de `channels` canales a mono (media de canales) en [-1, 1].
// Si `silent` es true o `data` es nulo, escribe ceros. `out` se redimensiona a `frames`.
void ConvertToMono(const BYTE* data, UINT32 frames, int channels, SampleFormat format, bool silent, std::vector<float>& out);

} // namespace audio
