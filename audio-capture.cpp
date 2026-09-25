#include "audio-capture.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstring>
#include <string>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <timeapi.h>
#include <propsys.h>
#include <propkey.h>
#include <functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

namespace {

// Subformatos de WAVEFORMATEXTENSIBLE. Definidos aquí para no depender de ksmedia.h.
const GUID kSubtypePcm = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
const GUID kSubtypeIeeeFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

enum class SampleFormat { Float32, Pcm16, Pcm24, Pcm32, Unknown };

SampleFormat DetectFormat(const WAVEFORMATEX* wfx) {
    WORD tag = wfx->wFormatTag;
    if (tag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wfx);
        if (IsEqualGUID(ext->SubFormat, kSubtypeIeeeFloat)) tag = WAVE_FORMAT_IEEE_FLOAT;
        else if (IsEqualGUID(ext->SubFormat, kSubtypePcm)) tag = WAVE_FORMAT_PCM;
    }
    if (tag == WAVE_FORMAT_IEEE_FLOAT && wfx->wBitsPerSample == 32) return SampleFormat::Float32;
    if (tag == WAVE_FORMAT_PCM) {
        switch (wfx->wBitsPerSample) {
        case 16: return SampleFormat::Pcm16;
        case 24: return SampleFormat::Pcm24;
        case 32: return SampleFormat::Pcm32;
        default: break;
        }
    }
    return SampleFormat::Unknown;
}

std::string GetDeviceFriendlyName(IMMDevice* pDevice) {
    if (!pDevice) return "Desconocido";
    IPropertyStore* pProps = nullptr;
    std::string name = "Dispositivo de audio";
    if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
            if (varName.vt == VT_LPWSTR && varName.pwszVal) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, NULL, 0, NULL, NULL);
                if (size_needed > 1) {
                    name.resize(size_needed - 1);
                    WideCharToMultiByte(CP_UTF8, 0, varName.pwszVal, -1, &name[0], size_needed, NULL, NULL);
                }
            }
        }
        PropVariantClear(&varName);
        pProps->Release();
    }
    return name;
}

// Mezcla un paquete a mono (media de canales), lo convierte a float y lo escribe en el anillo.
void PushPacket(AudioData& data, const BYTE* p, UINT32 frames, int channels, SampleFormat fmt, bool silent, std::vector<float>& scratch) {
    scratch.resize(frames);
    if (silent || p == nullptr) {
        std::fill(scratch.begin(), scratch.end(), 0.0f);
    }
    else {
        const float inv = 1.0f / static_cast<float>(channels);
        switch (fmt) {
        case SampleFormat::Float32: {
            const float* s = reinterpret_cast<const float*>(p);
            for (UINT32 f = 0; f < frames; ++f) {
                float acc = 0.0f;
                for (int c = 0; c < channels; ++c) acc += s[f * channels + c];
                scratch[f] = acc * inv;
            }
            break;
        }
        case SampleFormat::Pcm16: {
            const int16_t* s = reinterpret_cast<const int16_t*>(p);
            for (UINT32 f = 0; f < frames; ++f) {
                float acc = 0.0f;
                for (int c = 0; c < channels; ++c) acc += s[f * channels + c] * (1.0f / 32768.0f);
                scratch[f] = acc * inv;
            }
            break;
        }
        case SampleFormat::Pcm32: {
            const int32_t* s = reinterpret_cast<const int32_t*>(p);
            for (UINT32 f = 0; f < frames; ++f) {
                float acc = 0.0f;
                for (int c = 0; c < channels; ++c) acc += s[f * channels + c] * (1.0f / 2147483648.0f);
                scratch[f] = acc * inv;
            }
            break;
        }
        case SampleFormat::Pcm24: {
            // Empaquetado de 3 bytes little-endian con signo.
            for (UINT32 f = 0; f < frames; ++f) {
                float acc = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    const BYTE* b = p + (f * channels + c) * 3;
                    int32_t val = (static_cast<int32_t>(b[0])) |
                                  (static_cast<int32_t>(b[1]) << 8) |
                                  (static_cast<int32_t>(b[2]) << 16);
                    if (val & 0x00800000) val |= 0xFF000000;
                    acc += val * (1.0f / 8388608.0f);
                }
                scratch[f] = acc * inv;
            }
            break;
        }
        default:
            std::fill(scratch.begin(), scratch.end(), 0.0f);
            break;
        }
    }

    {
        std::lock_guard<std::mutex> lock(data.mtx);
        for (UINT32 f = 0; f < frames; ++f) {
            data.ring[(data.total_samples + f) & RING_MASK] = scratch[f];
        }
        data.total_samples += frames;
    }
    data.cv.notify_one();
}

// Ejecuta una sesión de captura sobre el endpoint configurado.
bool RunCaptureSession(AudioData& sharedData, VisualizerData& visualizerData) {
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;
    bool retry = false;
    std::wstring targetDeviceId;

    auto fail = [](const char* msg, HRESULT hr) {
        std::cerr << "Captura: " << msg << " (hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << ")" << std::endl;
    };

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) { fail("no se pudo crear el enumerador de dispositivos", hr); goto cleanup; }

    {
        std::lock_guard<std::mutex> lock(visualizerData.dev_mtx);
        targetDeviceId = visualizerData.requested_device_id;
    }

    if (targetDeviceId.empty()) {
        hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr)) { fail("no hay dispositivo de salida por defecto", hr); retry = true; goto cleanup; }
    }
    else {
        hr = pEnumerator->GetDevice(targetDeviceId.c_str(), &pDevice);
        if (FAILED(hr)) {
            std::cerr << "Captura: endpoint solicitado no disponible, usando predeterminado." << std::endl;
            hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
            if (FAILED(hr)) { fail("no hay dispositivo de salida", hr); retry = true; goto cleanup; }
        }
    }

    {
        std::string currentName = GetDeviceFriendlyName(pDevice);
        std::lock_guard<std::mutex> lock(visualizerData.dev_mtx);
        visualizerData.current_device_name = currentName;
        visualizerData.device_change_pending.store(false);
    }

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) { fail("no se pudo activar IAudioClient", hr); retry = true; goto cleanup; }

    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) { fail("no se pudo obtener el formato de mezcla", hr); retry = true; goto cleanup; }

    {
        const SampleFormat fmt = DetectFormat(pwfx);
        if (fmt == SampleFormat::Unknown) {
            std::cerr << "Captura: formato no soportado (tag=" << pwfx->wFormatTag << ", bits=" << pwfx->wBitsPerSample << ")" << std::endl;
            goto cleanup;
        }
        const int channels = pwfx->nChannels;

        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, nullptr);
        if (FAILED(hr)) { fail("no se pudo inicializar el loopback", hr); retry = true; goto cleanup; }

        hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
        if (FAILED(hr)) { fail("no se pudo obtener IAudioCaptureClient", hr); retry = true; goto cleanup; }

        sharedData.sample_rate.store(static_cast<int>(pwfx->nSamplesPerSec));

        hr = pAudioClient->Start();
        if (FAILED(hr)) { fail("no se pudo iniciar la captura", hr); retry = true; goto cleanup; }

        std::cout << "Captura: loopback activo (" << visualizerData.current_device_name
                  << ", " << pwfx->nSamplesPerSec << " Hz, " << channels << " ch, "
                  << pwfx->wBitsPerSample << " bits)." << std::endl;

        std::vector<float> scratch;
        scratch.reserve(4096);

        while (!visualizerData.should_terminate.load()) {
            if (visualizerData.device_change_pending.load(std::memory_order_relaxed)) {
                // Usuario solicitó conmutar a otro endpoint de audio
                retry = true;
                break;
            }

            UINT32 packetFrames = 0;
            hr = pCaptureClient->GetNextPacketSize(&packetFrames);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) { retry = true; break; }
            if (FAILED(hr)) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

            if (packetFrames == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            while (packetFrames != 0) {
                BYTE* pData = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                hr = pCaptureClient->GetBuffer(&pData, &frames, &flags, nullptr, nullptr);
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED) { retry = true; break; }
                if (FAILED(hr)) break;

                PushPacket(sharedData, pData, frames, channels, fmt, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0, scratch);
                pCaptureClient->ReleaseBuffer(frames);

                hr = pCaptureClient->GetNextPacketSize(&packetFrames);
                if (FAILED(hr)) break;
            }
            if (retry) break;
        }

        pAudioClient->Stop();
    }

cleanup:
    if (pCaptureClient) pCaptureClient->Release();
    if (pAudioClient) pAudioClient->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    if (pwfx) CoTaskMemFree(pwfx);
    return retry && !visualizerData.should_terminate.load();
}

} // namespace

std::vector<AudioDeviceInfo> EnumerateAudioDevices() {
    std::vector<AudioDeviceInfo> list;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return list;

    IMMDevice* pDefaultDevice = nullptr;
    std::wstring defaultId = L"";
    if (SUCCEEDED(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDefaultDevice))) {
        LPWSTR pStrId = nullptr;
        if (SUCCEEDED(pDefaultDevice->GetId(&pStrId))) {
            defaultId = pStrId;
            CoTaskMemFree(pStrId);
        }
        pDefaultDevice->Release();
    }

    IMMDeviceCollection* pCollection = nullptr;
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (SUCCEEDED(hr)) {
        UINT count = 0;
        pCollection->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pCollection->Item(i, &pDevice))) {
                LPWSTR pStrId = nullptr;
                pDevice->GetId(&pStrId);
                std::wstring devId = pStrId ? pStrId : L"";
                if (pStrId) CoTaskMemFree(pStrId);

                std::string devName = GetDeviceFriendlyName(pDevice);

                AudioDeviceInfo info;
                info.id = devId;
                info.name = devName;
                info.is_default = (devId == defaultId);
                list.push_back(info);

                pDevice->Release();
            }
        }
        pCollection->Release();
    }

    pEnumerator->Release();
    return list;
}

void AudioCaptureThread(AudioData& sharedData, VisualizerData& visualizerData) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Captura: no se pudo inicializar COM." << std::endl;
        return;
    }

    // Enumerar dispositivos disponibles al inicio y resolver el guardado en config.json
    {
        std::vector<AudioDeviceInfo> devs = EnumerateAudioDevices();
        std::lock_guard<std::mutex> lock(visualizerData.dev_mtx);
        visualizerData.devices = devs;
        if (!visualizerData.requested_device_name.empty() && visualizerData.requested_device_id.empty()) {
            for (const auto& d : devs) {
                if (d.name == visualizerData.requested_device_name) {
                    visualizerData.requested_device_id = d.id;
                    break;
                }
            }
            if (visualizerData.requested_device_id.empty()) {
                std::cerr << "Captura: el dispositivo guardado \"" << visualizerData.requested_device_name
                          << "\" no está disponible, se usa el predeterminado." << std::endl;
            }
        }
    }

    timeBeginPeriod(1);
    DWORD taskIndex = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (!visualizerData.should_terminate.load()) {
        const bool retry = RunCaptureSession(sharedData, visualizerData);
        if (!retry) break;
        
        // Pausa breve antes de reintentar si se invalidó el dispositivo
        for (int i = 0; i < 20 && !visualizerData.should_terminate.load() && !visualizerData.device_change_pending.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    timeEndPeriod(1);
    CoUninitialize();
}
