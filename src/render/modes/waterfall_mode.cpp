#include "render/modes/waterfall_mode.h"

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {

bool WaterfallMode::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert),
                             LoadShaderSource("shaders/waterfall.frag", embedded::kWaterfallFrag), "Waterfall");
    return program_ != 0;
}

void WaterfallMode::Render(const RenderContext& ctx) {
    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_resolution"), static_cast<float>(ctx.fb_width), static_cast<float>(ctx.fb_height));
    glUniform1f(glGetUniformLocation(program_, "u_scroll_head"), ctx.textures.waterfall_head());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx.textures.waterfall());
    glUniform1i(glGetUniformLocation(program_, "u_waterfall_tex"), 0);

    ctx.quad.Draw();
}

void WaterfallMode::Shutdown() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

} // namespace render
