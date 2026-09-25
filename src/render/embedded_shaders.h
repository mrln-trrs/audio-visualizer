#pragma once

// Copias embebidas de los shaders de shaders/. Se usan si el archivo externo no existe, para
// que el ejecutable arranque aunque falte la carpeta. Mantener sincronizadas con shaders/.
namespace render::embedded {

inline const char* const kBarsVert = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;
out vec4 v_color;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_color = a_color;
}
)";

inline const char* const kBarsFrag = R"(#version 330 core
in vec4 v_color;
out vec4 FragColor;
void main() {
    FragColor = v_color;
}
)";

inline const char* const kQuadVert = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

inline const char* const kRadialFrag = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_spectrum_tex;
uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_base_color;
uniform vec3 u_peak_color;
uniform float u_amplitude;
const float PI = 3.14159265359;

void main() {
    vec2 st = (gl_FragCoord.xy - 0.5 * u_resolution) / min(u_resolution.x, u_resolution.y);
    float r = length(st);
    float theta = atan(st.y, st.x);
    float norm_angle = abs(theta) / PI;

    float mag = texture(u_spectrum_tex, vec2(norm_angle, 0.5)).r * u_amplitude;
    mag = clamp(mag, 0.0, 1.0);

    float inner_r = 0.22;
    float outer_r = inner_r + mag * 0.35;

    vec3 col = vec3(0.06, 0.06, 0.08) * (1.0 - smoothstep(0.3, 0.8, r));

    float bass = texture(u_spectrum_tex, vec2(0.03, 0.5)).r * u_amplitude;
    float core_glow = exp(-r * (7.0 - bass * 3.0)) * bass;
    col += mix(u_base_color, u_peak_color, 0.5) * core_glow * 1.5;

    float ring_line = smoothstep(0.008, 0.0, abs(r - inner_r));
    col += u_base_color * ring_line * 1.2;

    if (r >= inner_r && r <= outer_r) {
        float f = (r - inner_r) / max(0.001, (outer_r - inner_r));
        vec3 bar_col = mix(u_base_color, u_peak_color, f);
        col += bar_col * (0.8 + 0.5 * f);
    }

    if (r > outer_r) {
        float glow = exp(-(r - outer_r) * 22.0) * mag;
        col += u_peak_color * glow * 1.0;
    }

    FragColor = vec4(col, 1.0);
}
)";

inline const char* const kWaveformFrag = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_waveform_tex;
uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_line_color;
uniform float u_amplitude;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    float wave_sample = texture(u_waveform_tex, vec2(uv.x, 0.5)).r * u_amplitude;
    float target_y = 0.5 + clamp(wave_sample, -1.0, 1.0) * 0.42;

    float dist = abs(uv.y - target_y);
    float beam = exp(-pow(dist * 180.0, 2.0));
    float halo = exp(-dist * 28.0) * 0.45;
    float ambient = exp(-dist * 8.0) * 0.15;

    vec2 grid_uv = fract(gl_FragCoord.xy / 40.0);
    float grid = (step(grid_uv.x, 0.03) + step(grid_uv.y, 0.03)) * 0.06;
    float center_line = (abs(uv.y - 0.5) < 0.002) ? 0.15 : 0.0;

    vec3 bg_color = vec3(0.04, 0.05, 0.06) + vec3(0.1, 0.2, 0.15) * (grid + center_line);
    vec3 beam_color = mix(u_line_color, vec3(1.0, 1.0, 1.0), beam * 0.7);

    vec3 final_col = bg_color + beam_color * (beam * 1.5 + halo + ambient);
    FragColor = vec4(final_col, 1.0);
}
)";

inline const char* const kWaterfallFrag = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_waterfall_tex;
uniform vec2 u_resolution;
uniform float u_scroll_head;

vec3 Colormap(float t) {
    t = clamp(t, 0.0, 1.0);
    vec3 c0 = vec3(0.05, 0.05, 0.08);
    vec3 c1 = vec3(0.35, 0.10, 0.45);
    vec3 c2 = vec3(0.85, 0.25, 0.15);
    vec3 c3 = vec3(1.00, 0.75, 0.10);
    vec3 c4 = vec3(1.00, 1.00, 0.95);

    if (t < 0.25) return mix(c0, c1, t / 0.25);
    if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
    if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
    return mix(c3, c4, (t - 0.75) / 0.25);
}

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    float y_history = fract(u_scroll_head - (1.0 - uv.y));
    float val = texture(u_waterfall_tex, vec2(uv.x, y_history)).r;
    vec3 col = Colormap(val);
    if (uv.y > 0.99) {
        col += vec3(0.2, 0.3, 0.4);
    }
    FragColor = vec4(col, 1.0);
}
)";

inline const char* const kStackedFrag = R"(#version 330 core
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_wave_tex;
uniform int u_tex_rows;
uniform int u_bands;
uniform float u_head;
uniform vec2 u_resolution;
uniform float u_amplitude;
uniform float u_mix_height;
uniform vec3 u_colors[40];
uniform float u_gains[40];
const int SAMPLES = 6;

void main() {
    float total_units = float(u_bands) + u_mix_height;
    float y_units = v_uv.y * total_units;
    int visible; int tex_row; float v; float row_units;
    if (y_units < u_mix_height) {
        visible = u_bands; tex_row = u_bands + 1; v = y_units / u_mix_height; row_units = u_mix_height;
    } else {
        float from_top = total_units - y_units;
        visible = clamp(int(floor(from_top)), 0, u_bands - 1);
        tex_row = visible; v = 1.0 - fract(from_top); row_units = 1.0;
    }
    float gain = (visible == u_bands) ? 1.0 : u_gains[visible];
    vec3 col = u_colors[visible];
    float row_px = u_resolution.y * (row_units / total_units);
    float tex_v = (float(tex_row) + 0.5) / float(u_tex_rows);
    float ymin = 1.0, ymax = 0.0;
    for (int s = 0; s < SAMPLES; ++s) {
        float xs = v_uv.x + ((float(s) + 0.5) / float(SAMPLES) - 0.5) / u_resolution.x;
        float idx = fract(u_head + xs);
        float val = texture(u_wave_tex, vec2(idx, tex_v)).r * gain * u_amplitude;
        float y = 0.5 + clamp(val, -1.0, 1.0) * 0.44;
        ymin = min(ymin, y); ymax = max(ymax, y);
    }
    float dist = max(0.0, max(ymin - v, v - ymax));
    float dist_px = dist * row_px;
    float beam = exp(-dist_px * dist_px * 0.6);
    float halo = exp(-dist_px * 0.35) * 0.35;
    float ambient = exp(-dist_px * 0.08) * 0.06;
    vec3 bg = vec3(0.045, 0.05, 0.065);
    float center = smoothstep(1.5, 0.0, abs(v - 0.5) * row_px) * 0.10;
    float edge = smoothstep(1.5, 0.0, min(v, 1.0 - v) * row_px) * 0.22;
    bg += vec3(0.10, 0.14, 0.20) * center + vec3(0.16, 0.18, 0.24) * edge;
    vec3 beam_col = mix(col, vec3(1.0), beam * 0.55);
    FragColor = vec4(bg + beam_col * (beam * 1.4 + halo + ambient), 1.0);
}
)";

} // namespace render::embedded
