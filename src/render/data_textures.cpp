#include "render/data_textures.h"

namespace render {

GLuint DataTextures::CreateFloatTexture(GLint wrap_t) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);
    return tex;
}

bool DataTextures::Init() {
    tex_spectrum_ = CreateFloatTexture(GL_CLAMP_TO_EDGE);
    tex_waveform_ = CreateFloatTexture(GL_CLAMP_TO_EDGE);
    tex_waterfall_ = CreateFloatTexture(GL_REPEAT);

    std::vector<float> zeros(WATERFALL_WIDTH * WATERFALL_HEIGHT, 0.0f);
    glBindTexture(GL_TEXTURE_2D, tex_waterfall_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, WATERFALL_WIDTH, WATERFALL_HEIGHT, 0, GL_RED, GL_FLOAT, zeros.data());
    waterfall_row_ = 0;
    row_scratch_.assign(WATERFALL_WIDTH, 0.0f);
    return tex_spectrum_ && tex_waveform_ && tex_waterfall_;
}

void DataTextures::Shutdown() {
    if (tex_spectrum_) glDeleteTextures(1, &tex_spectrum_);
    if (tex_waveform_) glDeleteTextures(1, &tex_waveform_);
    if (tex_waterfall_) glDeleteTextures(1, &tex_waterfall_);
    tex_spectrum_ = tex_waveform_ = tex_waterfall_ = 0;
}

void DataTextures::UploadSpectrum(const std::vector<float>& heights) {
    if (heights.empty()) return;
    glBindTexture(GL_TEXTURE_2D, tex_spectrum_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, static_cast<GLsizei>(heights.size()), 1, 0, GL_RED, GL_FLOAT, heights.data());
}

void DataTextures::UploadWaveform(const std::vector<float>& waveform) {
    if (waveform.empty()) return;
    glBindTexture(GL_TEXTURE_2D, tex_waveform_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, static_cast<GLsizei>(waveform.size()), 1, 0, GL_RED, GL_FLOAT, waveform.data());
}

void DataTextures::PushWaterfallRow(const std::vector<float>& spectrum, float gain) {
    if (spectrum.empty()) return;
    const int n = static_cast<int>(spectrum.size());
    for (int x = 0; x < WATERFALL_WIDTH; ++x) {
        const int idx = static_cast<int>(static_cast<float>(x) / WATERFALL_WIDTH * n);
        row_scratch_[x] = (idx < n) ? spectrum[idx] * gain : 0.0f;
    }
    glBindTexture(GL_TEXTURE_2D, tex_waterfall_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, waterfall_row_, WATERFALL_WIDTH, 1, GL_RED, GL_FLOAT, row_scratch_.data());
    waterfall_row_ = (waterfall_row_ + 1) % WATERFALL_HEIGHT;
}

} // namespace render
