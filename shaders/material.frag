#version 330 core
// Material acrilico propio (docs/12, seccion 3): escena desenfocada, mezcla por exclusion con
// gris medio, tinte con opacidad, ruido determinista por pixel, esquinas redondeadas por
// distancia con signo y sombra suave fuera del rectangulo.
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_blurred;   // escena desenfocada, alineada con la pantalla
uniform vec2 u_resolution;
uniform vec4 u_rect;           // x0, y0, x1, y1 en pixeles de gl_FragCoord (origen abajo)
uniform float u_radius;        // radio de las esquinas en pixeles
uniform vec4 u_tint;           // rgb del tinte y su opacidad
uniform float u_exclusion;     // 0.1 a 0.2
uniform float u_noise;         // 0.02 a 0.04
uniform float u_shadow;        // opacidad de la sombra
uniform float u_alpha;         // atenuacion global durante transiciones

float sdRoundRect(vec2 p, vec2 half_size, float r) {
    vec2 q = abs(p) - half_size + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec2 p = gl_FragCoord.xy;
    vec2 center = 0.5 * (u_rect.xy + u_rect.zw);
    vec2 half_size = 0.5 * (u_rect.zw - u_rect.xy);
    float d = sdRoundRect(p - center, half_size, u_radius);

    if (d > 0.0) {
        // Sombra desplazada 6 px hacia abajo con caida exponencial (docs/12, seccion 8).
        float ds = sdRoundRect(p - center + vec2(0.0, 6.0), half_size, u_radius);
        float a = u_shadow * exp(-max(ds, 0.0) / 14.0);
        if (ds > 40.0) discard;
        FragColor = vec4(0.0, 0.0, 0.0, a);
        return;
    }

    vec3 bg = texture(u_blurred, p / u_resolution).rgb;
    vec3 gray = vec3(0.5);
    vec3 excl = bg + gray - 2.0 * bg * gray;            // exclusion: empuja hacia el gris medio
    bg = mix(bg, excl, u_exclusion);
    vec3 col = mix(bg, u_tint.rgb, u_tint.a);
    float n = (hash(p) - 0.5) * 2.0 * u_noise;
    col += n;

    // Antialias del borde: un pixel de transicion.
    float aa = 1.0 - smoothstep(-1.0, 0.0, d);
    FragColor = vec4(col, aa * u_alpha);
}
