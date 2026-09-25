#include "render/modes/stacked_oscilloscope_mode.h"

#include <algorithm>
#include <vector>
#include <cstdio>
#include <imgui.h>

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {
namespace {

constexpr int MAX_ROWS = 40;
constexpr float MIX_HEIGHT = 1.5f; // la fila de mezcla es 1,5 veces más alta que una de banda

} // namespace

bool StackedOscilloscopeMode::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert),
                             LoadShaderSource("shaders/stacked.frag", embedded::kStackedFrag), "Stacked");
    return program_ != 0;
}

void StackedOscilloscopeMode::Render(const RenderContext& ctx) {
    const BandWaveformTexture& waves = ctx.band_waves;
    const int bands = std::min(waves.band_rows(), ctx.bands.count());
    if (!waves.has_data() || bands <= 0 || bands + 1 > MAX_ROWS) return;

    float colors[MAX_ROWS * 3] = {};
    float gains[MAX_ROWS] = {};
    for (int b = 0; b < bands; ++b) {
        colors[b * 3 + 0] = ctx.bands.bands[b].color[0];
        colors[b * 3 + 1] = ctx.bands.bands[b].color[1];
        colors[b * 3 + 2] = ctx.bands.bands[b].color[2];
        gains[b] = ctx.bands.bands[b].gain;
    }
    float peak[3];
    ColorOr(ctx.cfg.peak_color_rgb, 1.0f, 0.85f, 0.2f, peak);
    colors[bands * 3 + 0] = peak[0];
    colors[bands * 3 + 1] = peak[1];
    colors[bands * 3 + 2] = peak[2];
    gains[bands] = 1.0f;

    glUseProgram(program_);
    glUniform2f(glGetUniformLocation(program_, "u_resolution"), static_cast<float>(ctx.fb_width), static_cast<float>(ctx.fb_height));
    glUniform1i(glGetUniformLocation(program_, "u_tex_rows"), waves.rows());
    glUniform1i(glGetUniformLocation(program_, "u_bands"), bands);
    glUniform1f(glGetUniformLocation(program_, "u_head"), waves.head());
    glUniform1f(glGetUniformLocation(program_, "u_amplitude"), ctx.cfg.amplitude_factor);
    glUniform1f(glGetUniformLocation(program_, "u_mix_height"), MIX_HEIGHT);
    glUniform3fv(glGetUniformLocation(program_, "u_colors"), bands + 1, colors);
    glUniform1fv(glGetUniformLocation(program_, "u_gains"), bands + 1, gains);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, waves.texture());
    glUniform1i(glGetUniformLocation(program_, "u_wave_tex"), 0);

    ctx.quad.Draw();
}

void StackedOscilloscopeMode::DrawOverlay(const RenderContext& ctx) {
    const BandWaveformTexture& waves = ctx.band_waves;
    const int bands = std::min(waves.band_rows(), ctx.bands.count());
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float h = static_cast<float>(ctx.fb_height);

    if (!waves.has_data() || bands <= 0) {
        const char* msg = ctx.bands.count() > core::MAX_WAVEFORM_BANDS
            ? "Demasiadas bandas para reconstruir ondas (maximo 32). Cambie la particion en el HUD."
            : "Esperando ondas por banda...";
        const ImVec2 sz = ImGui::CalcTextSize(msg);
        dl->AddText(ImVec2((ctx.fb_width - sz.x) * 0.5f, (h - sz.y) * 0.5f), IM_COL32(200, 205, 215, 220), msg);
        return;
    }

    const float total_units = bands + MIX_HEIGHT;
    const float unit_px = h / total_units;
    const ImU32 text = IM_COL32(225, 230, 240, 220);
    const ImU32 dim = IM_COL32(150, 160, 180, 190);
    for (int b = 0; b < bands; ++b) {
        const auto& band = ctx.bands.bands[b];
        const float top = b * unit_px;
        dl->AddText(ImVec2(10.0f, top + 6.0f), text, band.name.c_str());
        char range[40];
        if (band.f_high_hz >= 1000.0f) std::snprintf(range, sizeof(range), "%.0f - %.1fk Hz", band.f_low_hz, band.f_high_hz / 1000.0f);
        else std::snprintf(range, sizeof(range), "%.0f - %.0f Hz", band.f_low_hz, band.f_high_hz);
        dl->AddText(ImVec2(10.0f, top + 6.0f + ImGui::GetFontSize() + 2.0f), dim, range);
    }
    const float mix_top = bands * unit_px;
    dl->AddText(ImVec2(10.0f, mix_top + 6.0f), text, "Mezcla (suma de las bandas y del resto)");
    char window_ms[48];
    const float ms = ctx.bands.sample_rate > 0 ? 1000.0f * core::BAND_WAVEFORM_HISTORY / ctx.bands.sample_rate : 0.0f;
    std::snprintf(window_ms, sizeof(window_ms), "ventana %.0f ms", ms);
    const ImVec2 wsz = ImGui::CalcTextSize(window_ms);
    dl->AddText(ImVec2(ctx.fb_width - wsz.x - 10.0f, mix_top + 6.0f), dim, window_ms);
}

void StackedOscilloscopeMode::Shutdown() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
}

} // namespace render
