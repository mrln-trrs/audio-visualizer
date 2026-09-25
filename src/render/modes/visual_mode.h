#pragma once

#include <vector>

#include "core/config.h"
#include "core/band_layout.h"
#include "render/data_textures.h"
#include "render/spectrum_dynamics.h"
#include "render/fullscreen_quad.h"

// Interfaz común de los modos de visualización. Cada modo posee sus programas y buffers y
// lee los datos de audio a través de las texturas y la dinámica del contexto.
namespace render {

struct RenderContext {
    int fb_width;
    int fb_height;
    double time;                          // glfwGetTime()
    int num_bars;
    const core::VisualizerConfig& cfg;
    const DataTextures& textures;
    const SpectrumDynamics& dynamics;     // alturas y picos por barra (ancho en píxeles)
    const FullscreenQuad& quad;
    const core::BandLayout& bands;        // partición actual del espectro
    const SpectrumDynamics& band_dynamics;// niveles y picos por banda, animados
};

class IVisualMode {
public:
    virtual ~IVisualMode() = default;
    // Compila shaders y crea recursos GL. Devuelve false si el modo no puede dibujar.
    virtual bool Init() = 0;
    // Dibujo OpenGL de la escena.
    virtual void Render(const RenderContext& ctx) = 0;
    // Dibujo opcional de texto y marcas con Dear ImGui. Se llama entre ImGui::NewFrame y
    // ImGui::Render, después de Render.
    virtual void DrawOverlay(const RenderContext&) {}
    virtual void Shutdown() = 0;
    virtual const char* Name() const = 0;
};

// Lee un color RGB de un vector de configuración, con valores por defecto si no tiene 3 componentes.
inline void ColorOr(const std::vector<float>& v, float r, float g, float b, float out[3]) {
    const bool ok = v.size() == 3;
    out[0] = ok ? v[0] : r;
    out[1] = ok ? v[1] : g;
    out[2] = ok ? v[2] : b;
}

} // namespace render
