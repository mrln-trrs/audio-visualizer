#pragma once

#include <vector>
#include <GL/glew.h>

#include "core/analysis_frame.h"

// Historial circular en GPU de las ondas por banda: una fila por banda reconstruida, una fila para
// la banda "resto" y una última fila para la mezcla original alineada. Cada trama nueva aporta
// hop_size muestras por fila; el historial es BAND_WAVEFORM_HISTORY muestras (85 ms a 48 kHz).
namespace render {

class BandWaveformTexture {
public:
    void Init();
    void Shutdown();

    // Añade las ondas de la trama. Recrea la textura si cambia el número de filas.
    void Append(const core::AnalysisFrame& frame);

    GLuint texture() const { return tex_; }
    int rows() const { return rows_; }              // filas de la textura: bandas + resto + mezcla
    int band_rows() const { return rows_ > 0 ? rows_ - 2 : 0; }
    int mix_row() const { return rows_ - 1; }
    // Posición de la muestra más antigua, en [0, 1), para el desplazamiento del shader.
    float head() const;
    bool has_data() const { return rows_ > 0 && appended_ > 0; }

private:
    GLuint tex_ = 0;
    int rows_ = 0;
    int head_ = 0;
    int appended_ = 0;
};

} // namespace render
