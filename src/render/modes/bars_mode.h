#pragma once

#include <vector>
#include <GL/glew.h>

#include "render/modes/visual_mode.h"

// Modo 1: barras de espectro con marcadores de pico. Malla dinámica de dos quads por barra.
namespace render {

class BarsMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Barras"; }

private:
    struct Vertex { float x, y, r, g, b, a; };

    void PushQuad(float x0, float y0, float x1, float y1, const float bottom[3], const float top[3]);

    GLuint program_ = 0;
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    std::vector<Vertex> vertices_;
    std::vector<unsigned int> indices_;
};

} // namespace render
