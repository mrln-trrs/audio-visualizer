#pragma once

#include <GL/glew.h>

#include "render/fullscreen_quad.h"

// Framebuffers y pasadas de post-procesado: escena a textura, reducción y desenfoque gaussiano
// separable, y presentación con opacidad. Fundamento y coste: docs/12, secciones 4 y 12.
namespace render {

// Textura de color con su framebuffer. Se redimensiona sin reasignar si el tamaño no cambia.
class RenderTarget {
public:
    bool Init(int width, int height);
    void Resize(int width, int height);
    void Shutdown();
    void Bind() const;           // como destino de dibujo, con viewport a su tamaño
    GLuint texture() const { return tex_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    GLuint fbo_ = 0;
    GLuint tex_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// Reducción a un cuarto de resolución y desenfoque gaussiano separable en dos pasadas, repetido
// `iterations` veces para ensanchar el núcleo. La separabilidad se demuestra en docs/12, 4.2.
class BlurChain {
public:
    bool Init();
    void Shutdown();
    // Desenfoca `source` (de `src_w` x `src_h` píxeles). Deja el resultado en blurred().
    void Run(GLuint source, int src_w, int src_h, float radius_texels, int iterations, const FullscreenQuad& quad);
    GLuint blurred() const { return ping_.texture(); }

private:
    RenderTarget ping_;
    RenderTarget pong_;
    GLuint blit_program_ = 0;
    GLuint blur_program_ = 0;
};

// Dibuja una textura a pantalla completa con una opacidad (mezcla alfa).
class Presenter {
public:
    bool Init();
    void Shutdown();
    void Draw(GLuint texture, float alpha, const FullscreenQuad& quad) const;

private:
    GLuint program_ = 0;
};

} // namespace render
