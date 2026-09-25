#pragma once

#include <vector>
#include <GL/glew.h>

#include "render/modes/visual_mode.h"

// Modo 5: medidores por banda. Una columna por banda con su color, nivel animado (pico de la
// banda en dB sobre el rango dinámico), marcador de pico y etiquetas con nombre y rango en Hz.
namespace render {

class BandMetersMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void DrawOverlay(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Medidores"; }

private:
    struct Vertex { float x, y, r, g, b, a; };
    struct Column { float x0, x1; };

    void PushQuad(float x0, float y0, float x1, float y1, const float bottom[3], const float top[3]);
    std::vector<Column> Columns(int count) const;

    GLuint program_ = 0;
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    std::vector<Vertex> vertices_;
    std::vector<unsigned int> indices_;
};

} // namespace render
