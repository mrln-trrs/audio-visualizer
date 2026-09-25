#pragma once

#include <vector>

#include "core/config.h"

// Animación temporal de un conjunto de valores en [0, 1]: filtro de primer orden con ataque y
// caída en milisegundos, más marcadores de pico con retardo y caída por gravedad.
// Fundamento: docs/05, sección 3. Las constantes pueden ser globales o por elemento
// (docs/11, sección 7), así se usa tanto para las barras como para las bandas.
namespace render {

class SpectrumDynamics {
public:
    // Constantes globales de `cfg` para todos los elementos.
    void Update(const std::vector<float>& values, int count, const core::VisualizerConfig& cfg, float dt);

    // Constantes por elemento. `attack_ms` y `release_ms` tienen al menos `count` elementos, o son
    // nulos para usar las globales de `cfg`.
    void Update(const std::vector<float>& values, int count, const core::VisualizerConfig& cfg,
                const float* attack_ms, const float* release_ms, float dt);

    const std::vector<float>& heights() const { return heights_; }
    const std::vector<float>& peaks() const { return peaks_; }

private:
    std::vector<float> heights_;
    std::vector<float> peaks_;
    std::vector<float> peak_timers_;
};

} // namespace render
