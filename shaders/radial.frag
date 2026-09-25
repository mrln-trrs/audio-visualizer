#version 330 core
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
    // Normalizar coordenadas centradas con corrección de aspecto
    vec2 st = (gl_FragCoord.xy - 0.5 * u_resolution) / min(u_resolution.x, u_resolution.y);
    float r = length(st);
    float theta = atan(st.y, st.x); // [-PI, PI]

    // Simetría espejada en 2 mitades para un anillo armónico
    float norm_angle = abs(theta) / PI; // [0, 1]

    // Muestreo del espectro
    float mag = texture(u_spectrum_tex, vec2(norm_angle, 0.5)).r * u_amplitude;
    mag = clamp(mag, 0.0, 1.0);

    // Parámetros del anillo radial
    float inner_r = 0.22;
    float outer_r = inner_r + mag * 0.35;

    // Fondo oscuro con sutil viñeteado
    vec3 col = vec3(0.06, 0.06, 0.08) * (1.0 - smoothstep(0.3, 0.8, r));

    // Núcleo central pulsante reactivo al bombo/graves (bajas frecuencias)
    float bass = texture(u_spectrum_tex, vec2(0.03, 0.5)).r * u_amplitude;
    float core_radius = 0.10 + bass * 0.06;
    float core_glow = exp(-r * (7.0 - bass * 3.0)) * bass;
    col += mix(u_base_color, u_peak_color, 0.5) * core_glow * 1.5;

    // Anillo base interno
    float ring_line = smoothstep(0.008, 0.0, abs(r - inner_r));
    col += u_base_color * ring_line * 1.2;

    // Barras radiales / picos de frecuencia
    if (r >= inner_r && r <= outer_r) {
        float f = (r - inner_r) / max(0.001, (outer_r - inner_r));
        vec3 bar_col = mix(u_base_color, u_peak_color, f);
        col += bar_col * (0.8 + 0.5 * f);
    }

    // Resplandor exterior (Glow)
    if (r > outer_r) {
        float glow = exp(-(r - outer_r) * 22.0) * mag;
        col += u_peak_color * glow * 1.0;
    }

    FragColor = vec4(col, 1.0);
}
