#pragma once

#include <string>
#include <vector>

#include "core/types.h"

struct IMMDevice;

// Enumeración de dispositivos de salida WASAPI. Requiere COM inicializado en el hilo llamante.
namespace audio {

// Nombre amigable (PKEY_Device_FriendlyName) en UTF-8. Nunca devuelve cadena vacía.
std::string GetDeviceFriendlyName(IMMDevice* device);

// Endpoints de render activos, con el predeterminado marcado.
std::vector<core::AudioDeviceInfo> EnumerateAudioDevices();

} // namespace audio
