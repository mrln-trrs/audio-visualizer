#version 330 core
// Copia una textura a pantalla completa con una opacidad.
in vec2 v_uv;
out vec4 FragColor;
uniform sampler2D u_tex;
uniform float u_alpha;

void main() {
    vec4 c = texture(u_tex, v_uv);
    FragColor = vec4(c.rgb, c.a * u_alpha);
}
