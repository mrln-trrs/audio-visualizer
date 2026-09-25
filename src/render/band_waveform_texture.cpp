#include "render/band_waveform_texture.h"

#include <vector>

namespace render {

void BandWaveformTexture::Init() {
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    rows_ = 0;
    head_ = 0;
    appended_ = 0;
}

void BandWaveformTexture::Shutdown() {
    if (tex_) glDeleteTextures(1, &tex_);
    tex_ = 0;
    rows_ = 0;
}

float BandWaveformTexture::head() const {
    return static_cast<float>(head_) / core::BAND_WAVEFORM_HISTORY;
}

void BandWaveformTexture::Append(const core::AnalysisFrame& frame) {
    if (!frame.band_waveform_valid || frame.band_waveform_rows <= 0) return;
    const int hop = frame.hop_size;
    const int rows = frame.band_waveform_rows + 1; // + mezcla
    if (static_cast<int>(frame.band_waveform.size()) < frame.band_waveform_rows * hop ||
        static_cast<int>(frame.mix_hop_waveform.size()) < hop) return;

    glBindTexture(GL_TEXTURE_2D, tex_);
    if (rows != rows_) {
        std::vector<float> zeros(static_cast<size_t>(core::BAND_WAVEFORM_HISTORY) * rows, 0.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, core::BAND_WAVEFORM_HISTORY, rows, 0, GL_RED, GL_FLOAT, zeros.data());
        rows_ = rows;
        head_ = 0;
        appended_ = 0;
    }

    // Bloque de hop columnas: las filas de bandas y resto vienen contiguas con paso hop, así que
    // una sola subida las cubre; la mezcla va en la última fila. BAND_WAVEFORM_HISTORY es múltiplo
    // de hop, por lo que el bloque nunca cruza el final del anillo.
    glTexSubImage2D(GL_TEXTURE_2D, 0, head_, 0, hop, frame.band_waveform_rows, GL_RED, GL_FLOAT, frame.band_waveform.data());
    glTexSubImage2D(GL_TEXTURE_2D, 0, head_, rows_ - 1, hop, 1, GL_RED, GL_FLOAT, frame.mix_hop_waveform.data());
    head_ = (head_ + hop) % core::BAND_WAVEFORM_HISTORY;
    ++appended_;
}

} // namespace render
