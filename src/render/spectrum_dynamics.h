#pragma once

#include <vector>

#include "core/config.h"

// Animación temporal de las barras: filtro de primer orden con ataque y caída en milisegundos,
// más marcadores de pico con retardo y caída por gravedad. Fundamento: docs/05, sección 3.
namespace render {

class SpectrumDynamics {
public:
    // `spectrum` es el último publicado (puede tener otra longitud que `num_bars` durante un
    // redimensionado; las barras sin dato se tratan como 0). `dt` en segundos.
    void Update(const std::vector<float>& spectrum, int num_bars, const core::VisualizerConfig& cfg, float dt);

    const std::vector<float>& heights() const { return heights_; }
    const std::vector<float>& peaks() const { return peaks_; }

private:
    std::vector<float> heights_;
    std::vector<float> peaks_;
    std::vector<float> peak_timers_;
};

} // namespace render
