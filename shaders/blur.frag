#version 330 core
// Una pasada del desenfoque gaussiano separable (docs/12, seccion 4.2): nueve muestras
// efectivas con cinco lecturas bilineales. u_step lleva la direccion (horizontal o vertical)
// escalada por el radio en texels.
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_tex;
uniform vec2 u_step;

void main() {
    // Pesos y desplazamientos del nucleo de 9 taps aprovechando el filtrado bilineal.
    const float w0 = 0.2270270270;
    const float w1 = 0.3162162162;
    const float w2 = 0.0702702703;
    const float o1 = 1.3846153846;
    const float o2 = 3.2307692308;
    vec4 c = texture(u_tex, v_uv) * w0;
    c += texture(u_tex, v_uv + u_step * o1) * w1;
    c += texture(u_tex, v_uv - u_step * o1) * w1;
    c += texture(u_tex, v_uv + u_step * o2) * w2;
    c += texture(u_tex, v_uv - u_step * o2) * w2;
    FragColor = c;
}
