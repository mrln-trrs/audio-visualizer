#include "audio/device_enumerator.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <propsys.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>

namespace audio {
namespace {

std::wstring DeviceId(IMMDevice* device) {
    LPWSTR id = nullptr;
    std::wstring result;
    if (device && SUCCEEDED(device->GetId(&id)) && id) {
        result = id;
        CoTaskMemFree(id);
    }
    return result;
}

} // namespace

std::string GetDeviceFriendlyName(IMMDevice* device) {
    if (!device) return "Desconocido";
    IPropertyStore* props = nullptr;
    std::string name = "Dispositivo de audio";
    if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
        PROPVARIANT var;
        PropVariantInit(&var);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR && var.pwszVal) {
            const int needed = WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, nullptr, 0, nullptr, nullptr);
            if (needed > 1) {
                name.resize(static_cast<size_t>(needed) - 1);
                WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, &name[0], needed, nullptr, nullptr);
            }
        }
        PropVariantClear(&var);
        props->Release();
    }
    return name;
}

std::vector<core::AudioDeviceInfo> EnumerateAudioDevices() {
    std::vector<core::AudioDeviceInfo> list;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator))) {
        return list;
    }

    std::wstring default_id;
    IMMDevice* default_device = nullptr;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_device))) {
        default_id = DeviceId(default_device);
        default_device->Release();
    }

    IMMDeviceCollection* collection = nullptr;
    if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
        UINT count = 0;
        collection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(collection->Item(i, &device))) {
                core::AudioDeviceInfo info;
                info.id = DeviceId(device);
                info.name = GetDeviceFriendlyName(device);
                info.is_default = (info.id == default_id);
                list.push_back(info);
                device->Release();
            }
        }
        collection->Release();
    }

    enumerator->Release();
    return list;
}

} // namespace audio
