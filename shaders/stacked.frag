#version 330 core
// Osciloscopio apilado: una traza por banda reconstruida y, abajo, la mezcla original.
// Cada fila lee su onda de una textura de historial circular (columnas = tiempo, filas = bandas).
// La suma de las trazas de banda es exactamente la traza de mezcla (docs/10, teorema 4.3).
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_wave_tex;   // BAND_WAVEFORM_HISTORY x filas
uniform int u_tex_rows;         // filas de la textura: bandas + resto + mezcla
uniform int u_bands;            // bandas visibles (K); la fila K es el resto (no se dibuja) y K+1 la mezcla
uniform float u_head;           // posicion de la muestra mas antigua, en [0, 1)
uniform vec2 u_resolution;
uniform float u_amplitude;
uniform float u_mix_height;     // altura de la fila de mezcla en unidades de fila de banda
uniform vec3 u_colors[40];      // color por banda; u_colors[u_bands] es el de la mezcla
uniform float u_gains[40];      // ganancia por banda; la mezcla usa 1.0

const int SAMPLES = 6;

void main() {
    float total_units = float(u_bands) + u_mix_height;
    float y_units = v_uv.y * total_units;

    int visible;     // indice de fila visible: 0..u_bands-1 bandas (0 arriba), u_bands = mezcla
    int tex_row;     // fila en la textura
    float v;         // posicion vertical dentro de la fila, 0 abajo 1 arriba
    float row_units;
    if (y_units < u_mix_height) {
        visible = u_bands;
        tex_row = u_bands + 1;
        v = y_units / u_mix_height;
        row_units = u_mix_height;
    } else {
        float from_top = total_units - y_units;      // 0 en el borde superior
        visible = int(floor(from_top));
        visible = clamp(visible, 0, u_bands - 1);
        tex_row = visible;
        v = 1.0 - fract(from_top);
        row_units = 1.0;
    }

    float gain = (visible == u_bands) ? 1.0 : u_gains[visible];
    vec3 col = u_colors[visible];
    float row_px = u_resolution.y * (row_units / total_units);
    float tex_v = (float(tex_row) + 0.5) / float(u_tex_rows);

    // Reduccion min/max sobre el ancho del pixel para no perder picos (docs/11, seccion 6).
    float ymin = 1.0, ymax = 0.0;
    for (int s = 0; s < SAMPLES; ++s) {
        float xs = v_uv.x + ((float(s) + 0.5) / float(SAMPLES) - 0.5) / u_resolution.x;
        float idx = fract(u_head + xs);
        float val = texture(u_wave_tex, vec2(idx, tex_v)).r * gain * u_amplitude;
        float y = 0.5 + clamp(val, -1.0, 1.0) * 0.44;
        ymin = min(ymin, y);
        ymax = max(ymax, y);
    }

    float dist = max(0.0, max(ymin - v, v - ymax));
    float dist_px = dist * row_px;
    float beam = exp(-dist_px * dist_px * 0.6);
    float halo = exp(-dist_px * 0.35) * 0.35;
    float ambient = exp(-dist_px * 0.08) * 0.06;

    // Fondo: linea central tenue y separador entre filas.
    vec3 bg = vec3(0.045, 0.05, 0.065);
    float center = smoothstep(1.5, 0.0, abs(v - 0.5) * row_px) * 0.10;
    float edge = smoothstep(1.5, 0.0, min(v, 1.0 - v) * row_px) * 0.22;
    bg += vec3(0.10, 0.14, 0.20) * center + vec3(0.16, 0.18, 0.24) * edge;

    vec3 beam_col = mix(col, vec3(1.0), beam * 0.55);
    FragColor = vec4(bg + beam_col * (beam * 1.4 + halo + ambient), 1.0);
}
