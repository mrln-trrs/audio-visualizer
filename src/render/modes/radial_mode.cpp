#include "render/modes/radial_mode.h"

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {

bool RadialMode::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert),
                             LoadShaderSource("shaders/radial.frag", embedded::kRadialFrag), "Radial");
    return program_ != 0;
}

void RadialMode::Render(const RenderContext& ctx) {
    float base[3], peak[3];
    ColorOr(ctx.cfg.base_color_rgb, 0.65f, 0.15f, 0.15f, base);
    ColorOr(ctx.cfg.peak_color_rgb, 1.0f, 0.85f, 0.2f, peak);

    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_resolution"), static_cast<float>(ctx.fb_width), static_cast<float>(ctx.fb_height));
    glUniform1f(glGetUniformLocation(program_, "u_time"), static_cast<float>(ctx.time));
    glUniform1f(glGetUniformLocation(program_, "u_amplitude"), ctx.cfg.amplitude_factor);
    glUniform3f(glGetUniformLocation(program_, "u_base_color"), base[0], base[1], base[2]);
    glUniform3f(glGetUniformLocation(program_, "u_peak_color"), peak[0], peak[1], peak[2]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx.textures.spectrum());
    glUniform1i(glGetUniformLocation(program_, "u_spectrum_tex"), 0);

    ctx.quad.Draw();
}

void RadialMode::Shutdown() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

} // namespace render
