#include "render/modes/band_meters_mode.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <imgui.h>

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {
namespace {

// Fracción de la altura reservada abajo para las etiquetas.
constexpr float LABEL_AREA = 0.14f;
// Separación entre columnas como fracción del ancho de columna.
constexpr float GAP_FACTOR = 0.18f;
constexpr float PEAK_CAP_HEIGHT = 0.012f;
// Máximo de columnas etiquetadas; por encima (modo per_bin) se dibujan sin texto.
constexpr int MAX_LABELED = 48;

} // namespace

bool BandMetersMode::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/bars.vert", embedded::kBarsVert),
                             LoadShaderSource("shaders/bars.frag", embedded::kBarsFrag), "BandMeters");
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return program_ != 0;
}

void BandMetersMode::PushQuad(float x0, float y0, float x1, float y1, const float bottom[3], const float top[3]) {
    const unsigned int base = static_cast<unsigned int>(vertices_.size());
    vertices_.push_back({ x0, y0, bottom[0], bottom[1], bottom[2], 1.0f });
    vertices_.push_back({ x1, y0, bottom[0], bottom[1], bottom[2], 1.0f });
    vertices_.push_back({ x1, y1, top[0], top[1], top[2], 1.0f });
    vertices_.push_back({ x0, y1, top[0], top[1], top[2], 1.0f });
    indices_.insert(indices_.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
}

std::vector<BandMetersMode::Column> BandMetersMode::Columns(int count) const {
    std::vector<Column> cols(count);
    const float margin = 0.04f;                       // margen lateral en NDC
    const float width = (2.0f - 2.0f * margin) / count;
    const float gap = count > MAX_LABELED ? 0.0f : width * GAP_FACTOR;
    for (int i = 0; i < count; ++i) {
        cols[i].x0 = -1.0f + margin + i * width + gap * 0.5f;
        cols[i].x1 = cols[i].x0 + width - gap;
    }
    return cols;
}

void BandMetersMode::Render(const RenderContext& ctx) {
    const int count = std::min<int>(ctx.bands.count(), static_cast<int>(ctx.band_dynamics.heights().size()));
    if (count <= 0) return;

    const auto& levels = ctx.band_dynamics.heights();
    const auto& peaks = ctx.band_dynamics.peaks();
    const float bottom_y = -1.0f + 2.0f * LABEL_AREA;
    const float max_height = 2.0f - 2.0f * LABEL_AREA - 0.06f;
    const std::vector<Column> cols = Columns(count);

    float peak_color[3];
    ColorOr(ctx.cfg.peak_color_rgb, 1.0f, 0.85f, 0.2f, peak_color);
    const float peak_bright[3] = { peak_color[0] * 1.2f, peak_color[1] * 1.2f, peak_color[2] * 1.2f };
    const float track[3] = { 0.10f, 0.11f, 0.15f };
    const float track_top[3] = { 0.13f, 0.14f, 0.19f };

    vertices_.clear();
    indices_.clear();
    for (int i = 0; i < count; ++i) {
        const auto& band = ctx.bands.bands[i];
        // Carril de fondo.
        PushQuad(cols[i].x0, bottom_y, cols[i].x1, bottom_y + max_height, track, track_top);
        // Nivel.
        const float h = std::clamp(levels[i], 0.0f, 1.0f);
        const float base[3] = { band.color[0] * 0.45f, band.color[1] * 0.45f, band.color[2] * 0.45f };
        const float top[3] = { std::min(1.0f, band.color[0] * (0.7f + 0.5f * h)),
                               std::min(1.0f, band.color[1] * (0.7f + 0.5f * h)),
                               std::min(1.0f, band.color[2] * (0.7f + 0.5f * h)) };
        PushQuad(cols[i].x0, bottom_y, cols[i].x1, bottom_y + h * max_height, base, top);
        // Marcador de pico.
        if (ctx.cfg.peak_hold_enabled) {
            const float py0 = bottom_y + std::clamp(peaks[i], 0.0f, 1.0f) * max_height;
            PushQuad(cols[i].x0, py0, cols[i].x1, std::min(bottom_y + max_height, py0 + PEAK_CAP_HEIGHT), peak_color, peak_bright);
        }
    }

    glUseProgram(program_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(Vertex), vertices_.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int), indices_.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices_.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void BandMetersMode::DrawOverlay(const RenderContext& ctx) {
    const int count = ctx.bands.count();
    if (count <= 0 || count > MAX_LABELED) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const std::vector<Column> cols = Columns(count);
    const float w = static_cast<float>(ctx.fb_width);
    const float hpx = static_cast<float>(ctx.fb_height);
    const float label_top = hpx * (1.0f - LABEL_AREA) + 8.0f;
    const ImU32 text_color = IM_COL32(230, 235, 245, 230);
    const ImU32 dim_color = IM_COL32(150, 160, 180, 200);
    const ImU32 warn_color = IM_COL32(255, 170, 80, 230);
    const auto& levels = ctx.band_dynamics.heights();

    for (int i = 0; i < count; ++i) {
        const auto& band = ctx.bands.bands[i];
        const float cx = (0.5f * (cols[i].x0 + cols[i].x1) * 0.5f + 0.5f) * w;
        const float col_w = (cols[i].x1 - cols[i].x0) * 0.5f * w;

        char range[48];
        if (band.f_high_hz >= 1000.0f) std::snprintf(range, sizeof(range), "%.0f-%.1fk", band.f_low_hz, band.f_high_hz / 1000.0f);
        else std::snprintf(range, sizeof(range), "%.0f-%.0f Hz", band.f_low_hz, band.f_high_hz);

        const ImVec2 name_size = ImGui::CalcTextSize(band.name.c_str());
        const ImVec2 range_size = ImGui::CalcTextSize(range);
        const bool fits = std::max(name_size.x, range_size.x) <= col_w + 4.0f;
        if (fits) {
            dl->AddText(ImVec2(cx - name_size.x * 0.5f, label_top), text_color, band.name.c_str());
            dl->AddText(ImVec2(cx - range_size.x * 0.5f, label_top + name_size.y + 2.0f), band.too_narrow ? warn_color : dim_color, range);
        }
        // Valor en dB sobre la columna.
        if (i < static_cast<int>(levels.size())) {
            const float range_db = std::max(1.0f, ctx.cfg.dynamic_range_db);
            const float db = levels[i] * range_db - range_db;
            char value[16];
            std::snprintf(value, sizeof(value), "%.0f", db);
            const ImVec2 vs = ImGui::CalcTextSize(value);
            if (vs.x <= col_w) {
                const float level_px = hpx * (1.0f - LABEL_AREA) - levels[i] * (hpx * (1.0f - LABEL_AREA) - 0.03f * hpx);
                dl->AddText(ImVec2(cx - vs.x * 0.5f, level_px - vs.y - 3.0f), text_color, value);
            }
        }
    }
}

void BandMetersMode::Shutdown() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (program_) glDeleteProgram(program_);
    vao_ = vbo_ = ebo_ = program_ = 0;
}

} // namespace render
