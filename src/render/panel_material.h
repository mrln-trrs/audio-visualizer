#pragma once

#include <array>
#include <GL/glew.h>

#include "render/fullscreen_quad.h"
#include "render/post_process.h"
#include "ui/panel_material.h"

struct ImDrawList;
struct ImDrawCmd;

// Material acrílico propio para los paneles de Dear ImGui: fondo de la escena desenfocado, mezcla
// por exclusión, tinte, ruido, esquinas redondeadas y sombra. Receta y fundamento en docs/12,
// secciones 3 a 6 y 8. Se dibuja desde un callback de la lista de dibujo de ImGui, debajo de los
// controles de cada panel, y sigue al panel cuando se arrastra.
namespace render {

struct MaterialParams {
    float tint[3] = { 0.08f, 0.09f, 0.13f };
    float opacity = 0.75f;      // opacidad del tinte; por debajo de 0,7 el contraste puede bajar de 4,5:1
    float exclusion = 0.15f;    // mezcla por exclusión con gris medio
    float noise = 0.03f;        // amplitud del ruido, 0,02 a 0,04
    float shadow = 0.35f;       // opacidad de la sombra
};

class PanelMaterial : public ui::IPanelMaterial {
public:
    bool Init();
    void Shutdown();

    // Desenfoca la escena para el cuadro actual. Llamar antes de ImGui::Render si hay paneles.
    void Prepare(GLuint scene_texture, int fb_width, int fb_height, const FullscreenQuad& quad);
    // Sin material este cuadro (paneles opacos). Debe llamarse una vez por cuadro si no se prepara.
    void Skip() { prepared_ = false; entry_count_ = 0; }
    void set_params(const MaterialParams& p) { params_ = p; }
    bool ready() const { return prepared_; }

    // ui::IPanelMaterial
    void AddPanelBackground(ImDrawList* draw_list, float x, float y, float w, float h, float rounding, float alpha) override;

private:
    struct Entry { PanelMaterial* owner; float x, y, w, h, rounding, alpha; };
    static void Callback(const ImDrawList* parent_list, const ImDrawCmd* cmd);
    void Draw(const Entry& e) const;

    static constexpr int kMaxPanels = 8;
    std::array<Entry, kMaxPanels> entries_{};
    int entry_count_ = 0;

    MaterialParams params_;
    BlurChain blur_;
    FullscreenQuad quad_;
    GLuint program_ = 0;
    int fb_width_ = 1;
    int fb_height_ = 1;
    bool prepared_ = false;
};

} // namespace render
