#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include <string>

#include "core/constants.h"
#include "core/types.h"
#include "core/analysis_frame.h"
#include "core/triple_buffer.h"

// Estructuras compartidas entre hilos. La tabla de quién escribe y quién lee cada campo
// está en docs/05_architecture_and_pipeline.md, sección 1, y docs/11, sección 5.
namespace core {

// Punto de intercambio 1: captura -> análisis.
struct AudioData {
    std::mutex mtx;
    std::condition_variable cv;
    // Muestras mono. La posición de escritura es total_samples & RING_MASK.
    std::vector<float> ring = std::vector<float>(RING_SIZE, 0.0f);
    // Muestras escritas desde el inicio. Protegido por mtx.
    uint64_t total_samples = 0;
    // Frecuencia de muestreo real del dispositivo. La fija el hilo de captura.
    std::atomic<int> sample_rate{ 0 };
};

// Punto de intercambio 2: análisis -> render, más el control de dispositivos y el cierre.
struct VisualizerData {
    // Tramas de análisis. Productor: hilo de análisis. Consumidor: hilo de render.
    TripleBuffer<AnalysisFrame> analysis;

    // Bandera global de cierre. Se pone a true con AudioData::mtx tomado.
    std::atomic<bool> should_terminate{ false };

    // Gestión de dispositivos WASAPI. Protegido por dev_mtx salvo el atómico.
    std::mutex dev_mtx;
    std::vector<AudioDeviceInfo> devices;
    // Dispositivo pedido por id (desde la UI) o por nombre (desde config.json al arrancar).
    // El hilo de captura resuelve el nombre a id tras enumerar los dispositivos.
    std::wstring requested_device_id;
    std::string requested_device_name;
    std::string current_device_name;
    std::atomic<bool> device_change_pending{ false };
};

} // namespace core
