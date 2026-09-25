#include "render/post_process.h"

#include <algorithm>
#include <iostream>

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {

// ---------------------------------------------------------------- RenderTarget
bool RenderTarget::Init(int width, int height) {
    glGenFramebuffers(1, &fbo_);
    glGenTextures(1, &tex_);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    Resize(std::max(1, width), std::max(1, height));
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    const bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (!ok) std::cerr << "Render: framebuffer incompleto." << std::endl;
    return ok;
}

void RenderTarget::Resize(int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    glBindTexture(GL_TEXTURE_2D, tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void RenderTarget::Shutdown() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (tex_) glDeleteTextures(1, &tex_);
    fbo_ = tex_ = 0;
    width_ = height_ = 0;
}

void RenderTarget::Bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width_, height_);
}

// ---------------------------------------------------------------- BlurChain
bool BlurChain::Init() {
    const std::string quad = LoadShaderSource("shaders/quad.vert", embedded::kQuadVert);
    blit_program_ = CreateProgram(quad, LoadShaderSource("shaders/blit.frag", embedded::kBlitFrag), "Blit");
    blur_program_ = CreateProgram(quad, LoadShaderSource("shaders/blur.frag", embedded::kBlurFrag), "Blur");
    return ping_.Init(4, 4) && pong_.Init(4, 4) && blit_program_ && blur_program_;
}

void BlurChain::Shutdown() {
    ping_.Shutdown();
    pong_.Shutdown();
    if (blit_program_) glDeleteProgram(blit_program_);
    if (blur_program_) glDeleteProgram(blur_program_);
    blit_program_ = blur_program_ = 0;
}

void BlurChain::Run(GLuint source, int src_w, int src_h, float radius_texels, int iterations, const FullscreenQuad& quad) {
    const int w = std::max(1, src_w / 4);
    const int h = std::max(1, src_h / 4);
    ping_.Resize(w, h);
    pong_.Resize(w, h);
    glDisable(GL_BLEND);

    // Reducción x4 con filtrado bilineal.
    pong_.Bind();
    glUseProgram(blit_program_);
    glUniform1f(glGetUniformLocation(blit_program_, "u_alpha"), 1.0f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source);
    glUniform1i(glGetUniformLocation(blit_program_, "u_tex"), 0);
    quad.Draw();

    // Pasadas separables: horizontal (pong -> ping), vertical (ping -> pong), repetidas.
    glUseProgram(blur_program_);
    glUniform1i(glGetUniformLocation(blur_program_, "u_tex"), 0);
    for (int i = 0; i < std::max(1, iterations); ++i) {
        ping_.Bind();
        glBindTexture(GL_TEXTURE_2D, pong_.texture());
        glUniform2f(glGetUniformLocation(blur_program_, "u_step"), radius_texels / w, 0.0f);
        quad.Draw();
        pong_.Bind();
        glBindTexture(GL_TEXTURE_2D, ping_.texture());
        glUniform2f(glGetUniformLocation(blur_program_, "u_step"), 0.0f, radius_texels / h);
        quad.Draw();
    }
    // El resultado final queda en pong_; lo copiamos a ping_ para exponerlo por blurred().
    ping_.Bind();
    glUseProgram(blit_program_);
    glBindTexture(GL_TEXTURE_2D, pong_.texture());
    quad.Draw();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// ---------------------------------------------------------------- Presenter
bool Presenter::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert),
                             LoadShaderSource("shaders/blit.frag", embedded::kBlitFrag), "Present");
    return program_ != 0;
}

void Presenter::Shutdown() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

void Presenter::Draw(GLuint texture, float alpha, const FullscreenQuad& quad) const {
    glUseProgram(program_);
    glUniform1f(glGetUniformLocation(program_, "u_alpha"), alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program_, "u_tex"), 0);
    quad.Draw();
}

} // namespace render
