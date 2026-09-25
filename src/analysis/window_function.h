#pragma once

#include <vector>

// Ventanas de análisis. Fundamento: docs/10_signal_decomposition_theory.md, secciones 3 y 4.2.
namespace analysis {

struct AnalysisWindow {
    std::vector<float> coefficients; // w[n], n = 0..size-1
    float normalization = 1.0f;      // 2 / sum(w): un seno a escala completa da magnitud 1.0
};

// Hann periódica: w[n] = 0.5 (1 - cos(2 pi n / N)). Es la forma que cumple la condición de
// solapamiento constante para N/H entero >= 2, necesaria para la reconstrucción exacta.
AnalysisWindow MakePeriodicHann(int size);

} // namespace analysis
