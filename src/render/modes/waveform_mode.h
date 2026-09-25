#pragma once

#include <GL/glew.h>

#include "render/modes/visual_mode.h"

// Modo 3: osciloscopio de la mezcla con haz tipo CRT. Quad de pantalla completa + fragment shader.
namespace render {

class WaveformMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Osciloscopio"; }

private:
    GLuint program_ = 0;
};

} // namespace render
