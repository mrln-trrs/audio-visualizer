#pragma once

#include <vector>
#include <GL/glew.h>

// Texturas de un canal en coma flotante que llevan los datos de audio a los shaders.
namespace render {

class DataTextures {
public:
    static constexpr int WATERFALL_WIDTH = 512;
    static constexpr int WATERFALL_HEIGHT = 256;

    bool Init();
    void Shutdown();

    // Textura 1D (ancho x 1) con las alturas animadas de las barras.
    void UploadSpectrum(const std::vector<float>& heights);
    // Textura 1D con las muestras crudas en [-1, 1].
    void UploadWaveform(const std::vector<float>& waveform);
    // Escribe una fila nueva en la textura circular del espectrograma, remuestreando `spectrum`
    // a WATERFALL_WIDTH columnas y aplicando `gain`.
    void PushWaterfallRow(const std::vector<float>& spectrum, float gain);

    GLuint spectrum() const { return tex_spectrum_; }
    GLuint waveform() const { return tex_waveform_; }
    GLuint waterfall() const { return tex_waterfall_; }
    // Posición de la fila de escritura en [0, 1), para el desplazamiento del shader.
    float waterfall_head() const { return static_cast<float>(waterfall_row_) / WATERFALL_HEIGHT; }

private:
    static GLuint CreateFloatTexture(GLint wrap_t);

    GLuint tex_spectrum_ = 0;
    GLuint tex_waveform_ = 0;
    GLuint tex_waterfall_ = 0;
    int waterfall_row_ = 0;
    std::vector<float> row_scratch_;
};

} // namespace render
