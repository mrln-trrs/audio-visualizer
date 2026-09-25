#include "audio-capture.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdint>
#include <cstring>

#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <timeapi.h>

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

namespace {

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

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
            for (UINT32 f = 0; f < frames; ++f) {
                float acc = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    const BYTE* b = p + (f * channels + c) * 3;
                    // Alineado a 32 bits con signo: el byte alto ocupa los bits 24..31.
                    const int32_t v = static_cast<int32_t>((static_cast<uint32_t>(b[0]) << 8) |
                                                           (static_cast<uint32_t>(b[1]) << 16) |
                                                           (static_cast<uint32_t>(b[2]) << 24));
                    acc += v * (1.0f / 2147483648.0f);
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
            data.ring[data.total_samples & RING_MASK] = scratch[f];
            ++data.total_samples;
        }
    }
    data.cv.notify_one();
}

// Abre el dispositivo de salida por defecto en modo loopback y captura hasta que se pida terminar
// o el dispositivo deje de ser válido. Devuelve true si hay que reintentar (cambio de dispositivo).
bool RunCaptureSession(AudioData& sharedData, VisualizerData& visualizerData) {
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioClient* pAudioClient = nullptr;
    IAudioCaptureClient* pCaptureClient = nullptr;
    WAVEFORMATEX* pwfx = nullptr;
    bool retry = false;

    auto fail = [](const char* msg, HRESULT hr) {
        std::cerr << "Captura: " << msg << " (hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << ")" << std::endl;
    };

    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    if (FAILED(hr)) { fail("no se pudo crear el enumerador de dispositivos", hr); goto cleanup; }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) { fail("no hay dispositivo de salida por defecto", hr); retry = true; goto cleanup; }

    hr = pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    if (FAILED(hr)) { fail("no se pudo activar IAudioClient", hr); retry = true; goto cleanup; }

    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) { fail("no se pudo obtener el formato de mezcla", hr); retry = true; goto cleanup; }

    {
        const SampleFormat fmt = DetectFormat(pwfx);
        if (fmt == SampleFormat::Unknown) {
            std::cerr << "Captura: formato de muestra no soportado (tag=" << pwfx->wFormatTag << ", bits=" << pwfx->wBitsPerSample << ")" << std::endl;
            goto cleanup;
        }
        const int channels = pwfx->nChannels;

        // Duración 0 = periodo por defecto del motor (~10 ms). El loopback sigue el periodo del
        // dispositivo de render, así que pedir menos no reduce la latencia.
        hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, nullptr);
        if (FAILED(hr)) { fail("no se pudo inicializar el loopback", hr); retry = true; goto cleanup; }

        hr = pAudioClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
        if (FAILED(hr)) { fail("no se pudo obtener IAudioCaptureClient", hr); retry = true; goto cleanup; }

        sharedData.sample_rate.store(static_cast<int>(pwfx->nSamplesPerSec));

        hr = pAudioClient->Start();
        if (FAILED(hr)) { fail("no se pudo iniciar la captura", hr); retry = true; goto cleanup; }

        std::cout << "Captura: loopback a " << pwfx->nSamplesPerSec << " Hz, " << channels << " canales, "
            << pwfx->wBitsPerSample << " bits." << std::endl;

        std::vector<float> scratch;
        scratch.reserve(4096);

        while (!visualizerData.should_terminate.load()) {
            UINT32 packetFrames = 0;
            hr = pCaptureClient->GetNextPacketSize(&packetFrames);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) { retry = true; break; }
            if (FAILED(hr)) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

            if (packetFrames == 0) {
                // Con timeBeginPeriod(1) este sleep dura ~1 ms de verdad.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // Vaciar todos los paquetes disponibles de una vez.
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

void AudioCaptureThread(AudioData& sharedData, VisualizerData& visualizerData) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "Captura: no se pudo inicializar COM." << std::endl;
        return;
    }

    // Resolución de 1 ms para los sleeps del bucle de captura.
    timeBeginPeriod(1);
    // Prioridad de hilo de audio profesional (MMCSS).
    DWORD taskIndex = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    while (!visualizerData.should_terminate.load()) {
        const bool retry = RunCaptureSession(sharedData, visualizerData);
        if (!retry) break;
        // El dispositivo por defecto cambió o se desconectó: esperar y reabrir.
        std::cerr << "Captura: dispositivo invalidado, reintentando..." << std::endl;
        for (int i = 0; i < 50 && !visualizerData.should_terminate.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    timeEndPeriod(1);
    CoUninitialize();
}
