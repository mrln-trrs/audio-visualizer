#pragma once

#include <vector>

#include "core/band_layout.h"

// Métricas por banda a partir del espectro de magnitud. Función pura, compartida por el hilo de
// análisis y por las pruebas (tests/band_metrics_test.cpp).
namespace analysis {

struct BandMetrics {
    std::vector<float> energy;   // sum |X[k]|^2 sobre los bins de la banda
    std::vector<float> rms;      // raíz de energía / número de bins
    std::vector<float> peak_db;  // máximo de magnitude_db en la banda
};

// Redimensiona `out` al número de bandas y la rellena. `magnitude` y `magnitude_db` tienen
// SPECTRUM_BINS elementos; los intervalos de bins vienen de `layout`.
void ComputeBandMetrics(const std::vector<float>& magnitude, const std::vector<float>& magnitude_db,
                        const core::BandLayout& layout, BandMetrics& out);

// Energía total sum |X[k]|^2 sobre todos los bins (Parseval).
float TotalEnergy(const std::vector<float>& magnitude);

} // namespace analysis
