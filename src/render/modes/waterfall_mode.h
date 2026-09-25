#pragma once

#include <GL/glew.h>

#include "render/modes/visual_mode.h"

// Modo 4: espectrograma en cascada con paleta térmica. Lee la textura circular de DataTextures.
namespace render {

class WaterfallMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Cascada"; }

private:
    GLuint program_ = 0;
};

} // namespace render
