#include "render/modes/bars_mode.h"

#include <algorithm>
#include <cstddef>

#include "render/shader_program.h"
#include "render/embedded_shaders.h"

namespace render {
namespace {

// Espacio entre barras como fracción del ancho de barra.
constexpr float BAR_GAP_FACTOR = 0.12f;
// Altura máxima de una barra en coordenadas normalizadas (2.0 = toda la ventana).
constexpr float BAR_MAX_HEIGHT = 1.75f;
// Grosor del marcador de pico en coordenadas normalizadas.
constexpr float PEAK_CAP_HEIGHT = 0.015f;

} // namespace

bool BarsMode::Init() {
    program_ = CreateProgram(LoadShaderSource("shaders/bars.vert", embedded::kBarsVert),
                             LoadShaderSource("shaders/bars.frag", embedded::kBarsFrag), "Bars");
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

void BarsMode::PushQuad(float x0, float y0, float x1, float y1, const float bottom[3], const float top[3]) {
    const unsigned int base = static_cast<unsigned int>(vertices_.size());
    vertices_.push_back({ x0, y0, bottom[0], bottom[1], bottom[2], 1.0f });
    vertices_.push_back({ x1, y0, bottom[0], bottom[1], bottom[2], 1.0f });
    vertices_.push_back({ x1, y1, top[0], top[1], top[2], 1.0f });
    vertices_.push_back({ x0, y1, top[0], top[1], top[2], 1.0f });
    indices_.insert(indices_.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
}

void BarsMode::Render(const RenderContext& ctx) {
    const auto& heights = ctx.dynamics.heights();
    const auto& peaks = ctx.dynamics.peaks();
    const int num_bars = std::min<int>(ctx.num_bars, static_cast<int>(heights.size()));
    if (num_bars <= 0) return;

    float base[3], peak[3];
    ColorOr(ctx.cfg.base_color_rgb, 0.65f, 0.15f, 0.15f, base);
    ColorOr(ctx.cfg.peak_color_rgb, 1.0f, 0.85f, 0.2f, peak);
    const float base_dark[3] = { base[0] * 0.5f, base[1] * 0.5f, base[2] * 0.5f };
    const float peak_bright[3] = { peak[0] * 1.2f, peak[1] * 1.2f, peak[2] * 1.2f };

    const float bar_width = 2.0f / static_cast<float>(num_bars);
    const float draw_width = bar_width * (1.0f - BAR_GAP_FACTOR);

    vertices_.clear();
    indices_.clear();
    vertices_.reserve(static_cast<size_t>(num_bars) * 8);
    indices_.reserve(static_cast<size_t>(num_bars) * 12);

    for (int i = 0; i < num_bars; ++i) {
        const float h = heights[i];
        const float x0 = -1.0f + static_cast<float>(i) * bar_width;
        const float x1 = x0 + draw_width;
        const float y1 = h * BAR_MAX_HEIGHT - 1.0f;

        // Degradado: base oscura abajo, color desplazado por la altura arriba.
        const float top[3] = {
            std::clamp(base[0] + h * 0.45f, 0.0f, 1.0f),
            std::clamp(base[1] - h * 0.10f, 0.0f, 1.0f),
            std::clamp(base[2] + h * 0.45f, 0.0f, 1.0f),
        };
        PushQuad(x0, -1.0f, x1, y1, base_dark, top);

        if (ctx.cfg.peak_hold_enabled) {
            const float py0 = peaks[i] * BAR_MAX_HEIGHT - 1.0f;
            const float py1 = std::min(1.0f, py0 + PEAK_CAP_HEIGHT);
            PushQuad(x0, py0, x1, py1, peak, peak_bright);
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

void BarsMode::Shutdown() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (program_) glDeleteProgram(program_);
    vao_ = vbo_ = ebo_ = program_ = 0;
}

} // namespace render
