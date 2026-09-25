#pragma once

#include <GL/glew.h>

#include "render/modes/visual_mode.h"

// Modo 2: anillo radial procedural con núcleo pulsante. Quad de pantalla completa + fragment shader.
namespace render {

class RadialMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Radial"; }

private:
    GLuint program_ = 0;
};

} // namespace render
