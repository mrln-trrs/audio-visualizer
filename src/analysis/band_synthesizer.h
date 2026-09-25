#pragma once

#include <vector>
#include <fftw3.h>

#include "core/band_layout.h"
#include "analysis/window_function.h"

// Reconstrucción de la onda de cada banda por IFFT enmascarada con solapamiento y suma.
// Fundamento: docs/10, secciones 4.1 a 4.4. Diseño: docs/11, sección 5.3.
//
// Para cada banda b, y_b = ISTFT(M_b X). Las máscaras M_b tienen rampas de coseno alzado en los
// bordes, complementarias entre bandas vecinas, y una banda implícita "resto" recoge los bins que
// no pertenecen a ninguna, de modo que sum_b M_b = 1 en todo k y, por linealidad,
// sum_b y_b = x exactamente (teorema 4.3 del documento 10).
//
// Cada llamada a Process con el espectro de la ventana actual entrega las HOP muestras que han
// recibido todas sus contribuciones (las más antiguas de la ventana). El render las concatena.
namespace analysis {

class BandSynthesizer {
public:
    BandSynthesizer(int fft_size, int hop_size);
    ~BandSynthesizer();
    BandSynthesizer(const BandSynthesizer&) = delete;
    BandSynthesizer& operator=(const BandSynthesizer&) = delete;

    // Reconstruye máscaras y acumuladores para la partición dada. Vacía el historial.
    void Configure(const core::BandLayout& layout, float ramp_bins, const AnalysisWindow& window);

    // Filas producidas: bandas más la fila "resto" al final. 0 si no está configurado.
    int rows() const { return rows_; }
    int hop_size() const { return hop_; }
    int fft_size() const { return n_; }

    // `spectrum` tiene fft_size/2+1 bins. `out` se redimensiona a rows() * hop_size(), por filas.
    void Process(const fftwf_complex* spectrum, std::vector<float>& out);

    // Máscara de la fila r (fft_size/2+1 valores). Para pruebas.
    const std::vector<float>& mask(int row) const { return masks_[row]; }

private:
    int n_;
    int hop_;
    int bins_;
    int rows_ = 0;
    std::vector<std::vector<float>> masks_;   // rows x bins
    std::vector<std::vector<float>> accum_;   // rows x n: solapamiento y suma pendiente
    std::vector<float> synthesis_window_;     // w[n] * (1/N)
    std::vector<float> ola_norm_;             // 1 / sum_m w^2[n - mH], por índice dentro del salto
    fftwf_complex* scratch_ = nullptr;
    float* time_out_ = nullptr;
    fftwf_plan plan_ = nullptr;
};

} // namespace analysis
