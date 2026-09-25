#include "audio/capture_session.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "audio/sample_format.h"
#include "audio/device_enumerator.h"

#include <mmdeviceapi.h>
#include <audioclient.h>

namespace audio {
namespace {

void Fail(const char* msg, HRESULT hr) {
    std::cerr << "Captura: " << msg << " (hr=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << ")" << std::endl;
}

// Escribe muestras mono en el anillo y despierta al hilo de análisis.
void PushToRing(core::AudioData& audio, const std::vector<float>& samples) {
    {
        std::lock_guard<std::mutex> lock(audio.mtx);
        for (size_t i = 0; i < samples.size(); ++i) {
            audio.ring[(audio.total_samples + i) & core::RING_MASK] = samples[i];
        }
        audio.total_samples += samples.size();
    }
    audio.cv.notify_one();
}

// Resuelve el endpoint a usar: el pedido por la UI o, si falla, el predeterminado.
IMMDevice* OpenRequestedDevice(IMMDeviceEnumerator* enumerator, core::VisualizerData& vis, HRESULT& hr) {
    std::wstring requested;
    {
        std::lock_guard<std::mutex> lock(vis.dev_mtx);
        requested = vis.requested_device_id;
    }
    IMMDevice* device = nullptr;
    if (!requested.empty()) {
        hr = enumerator->GetDevice(requested.c_str(), &device);
        if (SUCCEEDED(hr)) return device;
        std::cerr << "Captura: endpoint solicitado no disponible, usando predeterminado." << std::endl;
    }
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    return SUCCEEDED(hr) ? device : nullptr;
}

} // namespace

bool RunCaptureSession(core::AudioData& audio, core::VisualizerData& vis) {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice* device = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* wfx = nullptr;
    bool retry = false;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if (FAILED(hr)) { Fail("no se pudo crear el enumerador de dispositivos", hr); goto cleanup; }

    device = OpenRequestedDevice(enumerator, vis, hr);
    if (!device) { Fail("no hay dispositivo de salida", hr); retry = true; goto cleanup; }

    {
        const std::string name = GetDeviceFriendlyName(device);
        std::lock_guard<std::mutex> lock(vis.dev_mtx);
        vis.current_device_name = name;
        vis.device_change_pending.store(false);
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
    if (FAILED(hr)) { Fail("no se pudo activar IAudioClient", hr); retry = true; goto cleanup; }

    hr = client->GetMixFormat(&wfx);
    if (FAILED(hr)) { Fail("no se pudo obtener el formato de mezcla", hr); retry = true; goto cleanup; }

    {
        const SampleFormat format = DetectSampleFormat(wfx);
        if (format == SampleFormat::Unknown) {
            std::cerr << "Captura: formato no soportado (tag=" << wfx->wFormatTag << ", bits=" << wfx->wBitsPerSample << ")" << std::endl;
            goto cleanup;
        }
        const int channels = wfx->nChannels;

        // Duración 0 = periodo por defecto del motor (~10 ms). El loopback sigue el periodo del
        // dispositivo de render, así que pedir menos no reduce la latencia.
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, wfx, nullptr);
        if (FAILED(hr)) { Fail("no se pudo inicializar el loopback", hr); retry = true; goto cleanup; }

        hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
        if (FAILED(hr)) { Fail("no se pudo obtener IAudioCaptureClient", hr); retry = true; goto cleanup; }

        audio.sample_rate.store(static_cast<int>(wfx->nSamplesPerSec));

        hr = client->Start();
        if (FAILED(hr)) { Fail("no se pudo iniciar la captura", hr); retry = true; goto cleanup; }

        std::cout << "Captura: loopback activo (" << vis.current_device_name << ", " << wfx->nSamplesPerSec << " Hz, "
                  << channels << " canales, " << SampleFormatName(format) << ")." << std::endl;

        std::vector<float> mono;
        mono.reserve(4096);

        while (!vis.should_terminate.load()) {
            if (vis.device_change_pending.load(std::memory_order_relaxed)) { retry = true; break; }

            UINT32 packet_frames = 0;
            hr = capture->GetNextPacketSize(&packet_frames);
            if (hr == AUDCLNT_E_DEVICE_INVALIDATED) { retry = true; break; }
            if (FAILED(hr) || packet_frames == 0) {
                // Con timeBeginPeriod(1) activo en el hilo, este sleep dura ~1 ms de verdad.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            // Vaciar todos los paquetes disponibles de una vez.
            while (packet_frames != 0) {
                BYTE* data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED) { retry = true; break; }
                if (FAILED(hr)) break;

                ConvertToMono(data, frames, channels, format, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0, mono);
                PushToRing(audio, mono);
                capture->ReleaseBuffer(frames);

                hr = capture->GetNextPacketSize(&packet_frames);
                if (FAILED(hr)) break;
            }
            if (retry) break;
        }

        client->Stop();
    }

cleanup:
    if (capture) capture->Release();
    if (client) client->Release();
    if (device) device->Release();
    if (enumerator) enumerator->Release();
    if (wfx) CoTaskMemFree(wfx);
    return retry && !vis.should_terminate.load();
}

} // namespace audio
