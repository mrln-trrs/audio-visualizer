#include "audio/capture_thread.h"

#include <iostream>
#include <thread>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>   // CoInitializeEx (WIN32_LEAN_AND_MEAN excluye COM de windows.h)
#include <avrt.h>
#include <timeapi.h>

#include "audio/capture_session.h"
#include "audio/device_enumerator.h"

#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")

namespace audio {
namespace {

// Enumera los dispositivos y resuelve el nombre guardado en config.json a un id WASAPI.
void PublishDeviceList(core::VisualizerData& vis) {
    std::vector<core::AudioDeviceInfo> devices = EnumerateAudioDevices();
    std::lock_guard<std::mutex> lock(vis.dev_mtx);
    vis.devices = devices;
    if (!vis.requested_device_name.empty() && vis.requested_device_id.empty()) {
        for (const auto& d : devices) {
            if (d.name == vis.requested_device_name) {
                vis.requested_device_id = d.id;
                break;
            }
        }
        if (vis.requested_device_id.empty()) {
            std::cerr << "Captura: el dispositivo guardado \"" << vis.requested_device_name
                      << "\" no está disponible, se usa el predeterminado." << std::endl;
        }
    }
}

} // namespace

void CaptureThread(core::AudioData& audio, core::VisualizerData& vis) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        std::cerr << "Captura: no se pudo inicializar COM." << std::endl;
        return;
    }

    PublishDeviceList(vis);

    // Resolución de 1 ms para los sleeps del bucle y prioridad de audio profesional (MMCSS).
    timeBeginPeriod(1);
    DWORD task_index = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

    while (!vis.should_terminate.load()) {
        if (!RunCaptureSession(audio, vis)) break;
        // Dispositivo invalidado o cambio pedido: breve pausa antes de reabrir, salvo que el
        // cambio venga de la UI, en cuyo caso se reabre de inmediato.
        for (int i = 0; i < 20 && !vis.should_terminate.load() && !vis.device_change_pending.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
    timeEndPeriod(1);
    CoUninitialize();
}

} // namespace audio
