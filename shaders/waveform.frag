#version 330 core
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_waveform_tex;
uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_line_color;
uniform float u_amplitude;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;

    // Muestreo de las muestras en el dominio del tiempo [-1.0, 1.0]
    float wave_sample = texture(u_waveform_tex, vec2(uv.x, 0.5)).r * u_amplitude;
    float target_y = 0.5 + clamp(wave_sample, -1.0, 1.0) * 0.42;

    float dist = abs(uv.y - target_y);

    // Haz primario tipo rayo catódico / osciloscopio CRT
    float beam = exp(-pow(dist * 180.0, 2.0));
    // Halo difuso fosforescente
    float halo = exp(-dist * 28.0) * 0.45;
    // Resplandor ambiental suave
    float ambient = exp(-dist * 8.0) * 0.15;

    // Cuadrícula sutil de fondo (estilo retícula de osciloscopio)
    vec2 grid_uv = fract(gl_FragCoord.xy / 40.0);
    float grid = (step(grid_uv.x, 0.03) + step(grid_uv.y, 0.03)) * 0.06;
    float center_line = (abs(uv.y - 0.5) < 0.002) ? 0.15 : 0.0;

    vec3 bg_color = vec3(0.04, 0.05, 0.06) + vec3(0.1, 0.2, 0.15) * (grid + center_line);
    vec3 beam_color = mix(u_line_color, vec3(1.0, 1.0, 1.0), beam * 0.7);

    vec3 final_col = bg_color + beam_color * (beam * 1.5 + halo + ambient);
    FragColor = vec4(final_col, 1.0);
}
