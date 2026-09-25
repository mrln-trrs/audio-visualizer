#include "render/modes/waveform_mode.h"

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {

bool WaveformMode::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert),
                             LoadShaderSource("shaders/waveform.frag", embedded::kWaveformFrag), "Waveform");
    return program_ != 0;
}

void WaveformMode::Render(const RenderContext& ctx) {
    float line[3];
    ColorOr(ctx.cfg.base_color_rgb, 0.2f, 0.85f, 0.65f, line);

    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_resolution"), static_cast<float>(ctx.fb_width), static_cast<float>(ctx.fb_height));
    glUniform1f(glGetUniformLocation(program_, "u_time"), static_cast<float>(ctx.time));
    glUniform1f(glGetUniformLocation(program_, "u_amplitude"), ctx.cfg.amplitude_factor);
    glUniform3f(glGetUniformLocation(program_, "u_line_color"), line[0], line[1], line[2]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx.textures.waveform());
    glUniform1i(glGetUniformLocation(program_, "u_waveform_tex"), 0);

    ctx.quad.Draw();
}

void WaveformMode::Shutdown() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

} // namespace render
