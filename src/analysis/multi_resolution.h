#pragma once

#include <vector>
#include <fftw3.h>

#include "core/config.h"
#include "core/analysis_frame.h"
#include "analysis/window_function.h"

// Resolución variable por rangos (docs/10, sección 7.4; docs/11, fase D): una FFT larga para los
// graves y una corta para los agudos, ambas con ventana de Hann periódica y la misma normalización
// que la FFT base, y ambas terminando en la muestra más reciente de la ventana base.
//
// Es la forma práctica de la transformada Q constante en tiempo real: en lugar de una ventana
// por bin, tres resoluciones. Lo que se gana y lo que cuesta está cuantificado en docs/10, 7.3.
namespace analysis {

class MultiResolution {
public:
    ~MultiResolution();
    MultiResolution() = default;
    MultiResolution(const MultiResolution&) = delete;
    MultiResolution& operator=(const MultiResolution&) = delete;

    // Crea planes y ventanas para la configuración. Barato de repetir si no cambia.
    void Configure(const core::AnalysisConfig& cfg);
    bool configured() const { return low_plan_ != nullptr && high_plan_ != nullptr; }
    int low_size() const { return low_n_; }
    int high_size() const { return high_n_; }

    // `latest` contiene al menos low_size() muestras crudas ordenadas en el tiempo; la última es la
    // más reciente. Rellena los espectros adicionales de `out`.
    void Process(const std::vector<float>& latest, core::AnalysisFrame& out);

private:
    struct Stage {
        int n = 0;
        AnalysisWindow window;
        std::vector<float> frame;
        fftwf_complex* out = nullptr;
        fftwf_plan plan = nullptr;
        void Setup(int size);
        void Destroy();
        void Run(const std::vector<float>& latest, std::vector<float>& magnitude);
    };

    core::AnalysisConfig cfg_;
    int low_n_ = 0;
    int high_n_ = 0;
    Stage low_;
    Stage high_;
    fftwf_plan low_plan_ = nullptr;   // alias para configured()
    fftwf_plan high_plan_ = nullptr;
};

} // namespace analysis
