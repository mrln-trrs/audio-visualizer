#pragma once

#include <cstdint>

// Constantes del análisis de señal compartidas por captura, análisis y render.
// La justificación de cada valor está en docs/05_architecture_and_pipeline.md
// y docs/10_signal_decomposition_theory.md.
namespace core {

// Tamaño de la ventana de análisis, en muestras mono. A 48 kHz equivale a ~42,7 ms.
// Cuanto mayor, más resolución en graves; cuanto menor, menos latencia.
constexpr int FFT_SIZE = 2048;

// Cada cuántas muestras nuevas se recalcula el espectro (ventana deslizante con solapamiento).
// A 48 kHz, 256 muestras = 5,3 ms, es decir, hasta ~187 espectros/s. En la práctica el motor
// de audio de Windows entrega paquetes cada ~10 ms, así que se obtienen ~100 espectros/s.
constexpr int HOP_SIZE = 256;

// Búfer circular de captura. Potencia de dos para indexar con máscara. Debe contener la ventana
// más larga que se analice (resolución variable, docs/10 sección 7.4: hasta 16384 muestras).
constexpr int MAX_LOW_BAND_FFT_SIZE = 16384;
constexpr int RING_SIZE = FFT_SIZE * 8;
constexpr uint64_t RING_MASK = RING_SIZE - 1;
static_assert((RING_SIZE & (RING_SIZE - 1)) == 0, "RING_SIZE debe ser potencia de dos");
static_assert(RING_SIZE >= MAX_LOW_BAND_FFT_SIZE, "el anillo debe contener la ventana larga de graves");
static_assert(FFT_SIZE % HOP_SIZE == 0 && FFT_SIZE / HOP_SIZE >= 4,
              "FFT_SIZE / HOP_SIZE debe ser entero y al menos 4 (condicion de solapamiento constante)");

// Bins de la FFT real a compleja: k = 0 .. N/2.
constexpr int SPECTRUM_BINS = FFT_SIZE / 2 + 1;

// Muestras crudas publicadas para el osciloscopio.
constexpr int WAVEFORM_SNAPSHOT_SIZE = 1024;

// Máximo de bandas para las que se reconstruye la onda (una IFFT por banda y trama).
// Por encima, por ejemplo en el modo per_bin, no se sintetiza (docs/10, sección 6.3).
constexpr int MAX_WAVEFORM_BANDS = 32;

// Historial de ondas por banda que conserva el render, en muestras. Múltiplo de HOP_SIZE para
// que cada trama nueva ocupe un bloque entero sin partirse en el anillo.
constexpr int BAND_WAVEFORM_HISTORY = 4096;
static_assert(BAND_WAVEFORM_HISTORY % HOP_SIZE == 0, "BAND_WAVEFORM_HISTORY debe ser multiplo de HOP_SIZE");

} // namespace core
