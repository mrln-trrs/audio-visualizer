#pragma once

#include <atomic>

// Triple búfer para un productor y un consumidor, sin bloqueos ni copias.
//
// El productor escribe siempre en un búfer libre y publica con un intercambio atómico; el
// consumidor toma el último completo con otro intercambio. Si el productor publica varias veces
// entre dos lecturas, las tramas intermedias se descartan: el consumidor siempre ve la más
// reciente y nunca una a medio escribir. Diseño en docs/11, sección 5.2.
namespace core {

template <typename T>
class TripleBuffer {
public:
    // Búfer en el que el productor debe escribir la siguiente trama. Solo el productor lo llama.
    T& BeginWrite() { return buffers_[write_]; }

    // Publica el búfer de escritura y toma como nuevo búfer de escritura el que quedó libre.
    void Publish() {
        const int previous = middle_.exchange(write_ | kDirty, std::memory_order_acq_rel);
        write_ = previous & kIndexMask;
    }

    // Último búfer publicado. Si hay uno nuevo desde la última llamada lo adopta; si no,
    // devuelve el mismo que la vez anterior. Solo el consumidor lo llama.
    const T& Read() {
        if (middle_.load(std::memory_order_acquire) & kDirty) {
            const int previous = middle_.exchange(read_, std::memory_order_acq_rel);
            read_ = previous & kIndexMask;
        }
        return buffers_[read_];
    }

    // Acceso a los tres búferes para preasignar memoria antes de arrancar los hilos.
    T& slot(int i) { return buffers_[i]; }
    static constexpr int kSlots = 3;

private:
    static constexpr int kDirty = 4;
    static constexpr int kIndexMask = 3;

    T buffers_[3];
    int write_ = 0;                 // solo productor
    int read_ = 2;                  // solo consumidor
    std::atomic<int> middle_{ 1 };  // índice intermedio, con bit kDirty si contiene una trama sin leer
};

} // namespace core
