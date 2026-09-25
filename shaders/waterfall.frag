#version 330 core
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_waterfall_tex;
uniform vec2 u_resolution;
uniform float u_scroll_head; // Fila actual de escritura [0.0, 1.0]

// Colormap estilo Turbo / Inferno para espectrograma térmico
vec3 Colormap(float t) {
    t = clamp(t, 0.0, 1.0);
    // Gradiente térmico: negro -> violeta oscuro -> naranja ardiente -> amarillo brillante -> blanco
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

    // Desplazar verticalmente según el cabezal circular de escritura
    // La fila más reciente aparece arriba (uv.y = 1.0) y desciende hacia abajo
    float y_history = fract(u_scroll_head - (1.0 - uv.y));
    float val = texture(u_waterfall_tex, vec2(uv.x, y_history)).r;

    // Aplicar paleta térmica
    vec3 col = Colormap(val);

    // Divisor sutil en la parte superior donde entra el nuevo audio
    if (uv.y > 0.99) {
        col += vec3(0.2, 0.3, 0.4);
    }

    FragColor = vec4(col, 1.0);
}
