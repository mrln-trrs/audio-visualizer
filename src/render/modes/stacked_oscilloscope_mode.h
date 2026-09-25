#pragma once

#include <GL/glew.h>

#include "render/modes/visual_mode.h"

// Modo 6 (tecla 5): osciloscopio apilado. Una traza por banda reconstruida y debajo la mezcla.
// La suma numérica de las trazas de banda es la mezcla (docs/10, teorema 4.3; docs/11, sección 8).
namespace render {

class StackedOscilloscopeMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void DrawOverlay(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Osciloscopio apilado"; }
    // Este modo necesita que el análisis reconstruya las ondas por banda.
    bool NeedsBandWaveforms() const override { return true; }

private:
    GLuint program_ = 0;
};

} // namespace render
