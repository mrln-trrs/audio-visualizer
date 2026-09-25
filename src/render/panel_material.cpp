#include "render/panel_material.h"

#include <algorithm>
#include <imgui.h>

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {

bool PanelMaterial::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert),
                             LoadShaderSource("shaders/material.frag", embedded::kMaterialFrag), "Material");
    return quad_.Init() && blur_.Init() && program_ != 0;
}

void PanelMaterial::Shutdown() {
    blur_.Shutdown();
    quad_.Shutdown();
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

void PanelMaterial::Prepare(GLuint scene_texture, int fb_width, int fb_height, const FullscreenQuad& quad) {
    fb_width_ = std::max(1, fb_width);
    fb_height_ = std::max(1, fb_height);
    // Núcleo de ~1,6 texels por pasada, tres iteraciones, a un cuarto de resolución: equivale a
    // una sigma de unos 24 px a resolución completa (docs/12, sección 4.3).
    blur_.Run(scene_texture, fb_width_, fb_height_, 1.6f, 3, quad);
    glViewport(0, 0, fb_width_, fb_height_);
    prepared_ = true;
    entry_count_ = 0;
}

void PanelMaterial::AddPanelBackground(ImDrawList* draw_list, float x, float y, float w, float h, float rounding, float alpha) {
    if (!prepared_ || entry_count_ >= kMaxPanels) return;
    Entry& e = entries_[entry_count_++];
    e = { this, x, y, w, h, rounding, alpha };
    // El puntero a la entrada viaja en UserCallbackData; el array no se realoja durante el cuadro.
    draw_list->AddCallback(&PanelMaterial::Callback, &e);
    // Tras un callback de usuario el backend de ImGui debe restaurar su estado de render.
    draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

void PanelMaterial::Callback(const ImDrawList*, const ImDrawCmd* cmd) {
    const Entry* e = static_cast<const Entry*>(cmd->UserCallbackData);
    if (e && e->owner) e->owner->Draw(*e);
}

void PanelMaterial::Draw(const Entry& e) const {
    // Coordenadas de ImGui (origen arriba) a coordenadas de gl_FragCoord (origen abajo).
    const float x0 = e.x, x1 = e.x + e.w;
    const float y1 = static_cast<float>(fb_height_) - e.y;
    const float y0 = y1 - e.h;

    glDisable(GL_SCISSOR_TEST); // el rectángulo del panel y su sombra se recortan en el shader
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, fb_width_, fb_height_);

    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_resolution"), static_cast<float>(fb_width_), static_cast<float>(fb_height_));
    glUniform4f(glGetUniformLocation(program_, "u_rect"), x0, y0, x1, y1);
    glUniform1f(glGetUniformLocation(program_, "u_radius"), e.rounding);
    glUniform4f(glGetUniformLocation(program_, "u_tint"), params_.tint[0], params_.tint[1], params_.tint[2], params_.opacity);
    glUniform1f(glGetUniformLocation(program_, "u_exclusion"), params_.exclusion);
    glUniform1f(glGetUniformLocation(program_, "u_noise"), params_.noise);
    glUniform1f(glGetUniformLocation(program_, "u_shadow"), params_.shadow * e.alpha);
    glUniform1f(glGetUniformLocation(program_, "u_alpha"), e.alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blur_.blurred());
    glUniform1i(glGetUniformLocation(program_, "u_blurred"), 0);
    quad_.Draw();
}

} // namespace render
