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

} // namespace render::embedded
