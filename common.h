#pragma once

#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>
#include "config.h"

// Tamaño de la ventana de análisis, en muestras mono. A 48 kHz equivale a ~42,7 ms.
// Cuanto mayor, más resolución en graves; cuanto menor, menos latencia.
constexpr int FFT_SIZE = 2048;

// Cada cuántas muestras nuevas se recalcula el espectro (ventana deslizante con solapamiento).
// A 48 kHz, 256 muestras = 5,3 ms, es decir, hasta ~187 espectros/s. En la práctica el motor
// de audio de Windows entrega paquetes cada ~10 ms, así que se obtienen ~100 espectros/s.
constexpr int HOP_SIZE = 256;

// Búfer circular de captura. Potencia de dos para indexar con máscara.
constexpr int RING_SIZE = FFT_SIZE * 4;
constexpr uint64_t RING_MASK = RING_SIZE - 1;
static_assert((RING_SIZE & (RING_SIZE - 1)) == 0, "RING_SIZE debe ser potencia de dos");

// Datos compartidos entre el hilo de captura y el de procesamiento.
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

// Datos compartidos entre el hilo de procesamiento y el de renderizado.
struct VisualizerData {
    std::mutex mtx;
    // Último espectro publicado: un valor en [0, 1] por barra.
    std::vector<float> spectrum;
    // Se incrementa en cada publicación. El renderizador solo copia si ha cambiado.
    std::atomic<uint64_t> generation{ 0 };
    // Número de barras que quiere el renderizador (ancho del framebuffer en píxeles).
    std::atomic<int> atomic_num_bars{ 1024 };
    // Bandera global de cierre.
    std::atomic<bool> should_terminate{ false };
};
