#include "renderer.h"
#include "audio-capture.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <thread>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace {

constexpr float BAR_GAP_FACTOR = 0.12f;
constexpr float BAR_MAX_HEIGHT = 1.75f;
constexpr float PEAK_CAP_HEIGHT = 0.015f;

// -----------------------------------------------------------------------------
// Shaders Embebidos (Fallbacks garantizados)
// -----------------------------------------------------------------------------
const char* kBarsVertFallback = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;
out vec4 v_color;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
    v_color = a_color;
}
)";

const char* kBarsFragFallback = R"(#version 330 core
in vec4 v_color;
out vec4 FragColor;
void main() {
    FragColor = v_color;
}
)";

const char* kQuadVertFallback = R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
    v_uv = a_uv;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

const char* kRadialFragFallback = R"(#version 330 core
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

const char* kWaveformFragFallback = R"(#version 330 core
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

const char* kWaterfallFragFallback = R"(#version 330 core
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

std::string LoadShaderSource(const std::string& path, const char* fallback) {
    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    return std::string(fallback);
}

GLuint CompileShader(GLenum type, const char* source, const char* name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader [" << name << "] error de compilación: " << infoLog << std::endl;
    }
    return shader;
}

GLuint CreateProgram(const std::string& vertSrc, const std::string& fragSrc, const char* name) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertSrc.c_str(), (std::string(name) + " VS").c_str());
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragSrc.c_str(), (std::string(name) + " FS").c_str());

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Program [" << name << "] error de enlace: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

struct BarVertex {
    float x, y;
    float r, g, b, a;
};

void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ApplyModernObsidianTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 6.0f;

    style.WindowPadding     = ImVec2(14, 14);
    style.FramePadding      = ImVec2(10, 6);
    style.ItemSpacing       = ImVec2(10, 8);
    style.ItemInnerSpacing  = ImVec2(6, 6);
    style.ScrollbarSize     = 14.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.55f, 0.65f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.08f, 0.09f, 0.13f, 0.92f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.06f, 0.07f, 0.10f, 0.70f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.09f, 0.13f, 0.96f);
    colors[ImGuiCol_Border]                = ImVec4(0.20f, 0.25f, 0.35f, 0.50f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.12f, 0.14f, 0.20f, 0.90f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.18f, 0.22f, 0.32f, 0.95f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.15f, 0.19f, 0.28f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.07f, 0.08f, 0.12f, 0.95f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.10f, 0.14f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.05f, 0.06f, 0.08f, 0.75f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.09f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.05f, 0.06f, 0.08f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.28f, 0.38f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.30f, 0.38f, 0.52f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.36f, 0.46f, 0.62f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.25f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.22f, 0.62f, 0.96f, 0.90f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.14f, 0.20f, 0.32f, 0.85f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.32f, 0.50f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.14f, 0.20f, 0.32f, 0.70f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.22f, 0.32f, 0.50f, 0.90f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.16f, 0.24f, 0.38f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.20f, 0.24f, 0.34f, 0.60f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.25f, 0.35f, 0.50f, 0.80f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.30f, 0.45f, 0.65f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.10f, 0.12f, 0.18f, 0.90f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.22f, 0.32f, 0.50f, 1.00f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.16f, 0.25f, 0.40f, 1.00f);
    colors[ImGuiCol_TabDimmed]             = ImVec4(0.08f, 0.09f, 0.14f, 0.90f);
    colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    auto* vis = static_cast<VisualizerData*>(glfwGetWindowUserPointer(window));
    if (vis) {
        vis->atomic_num_bars.store(std::max(1, width));
    }
}

int RefreshRateForWindow(GLFWwindow* window) {
    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);
    const int cx = wx + ww / 2;
    const int cy = wy + wh / 2;

    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i) {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (!mode) continue;
        if (cx >= mx && cx < mx + mode->width && cy >= my && cy < my + mode->height) {
            return mode->refreshRate;
        }
    }
    const GLFWvidmode* primary = glfwGetVideoMode(glfwGetPrimaryMonitor());
    return primary ? primary->refreshRate : 0;
}

} // namespace

void RenderThread(VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData, AudioData& sharedAudioData) {
    std::cout << "Render: hilo iniciado con Modern OpenGL 3.3 Core." << std::endl;

    VisualizerConfig cfg;
    {
        std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
        cfg = sharedConfigData.config;
    }

    if (!glfwInit()) {
        std::cerr << "Render: no se pudo inicializar GLFW." << std::endl;
        return;
    }

    // Configurar contexto Modern OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1024, 600, "Audio Visualizer 2.0", nullptr, nullptr);
    if (!window) {
        std::cerr << "Render: no se pudo crear la ventana OpenGL 3.3." << std::endl;
        glfwTerminate();
        return;
    }

    glfwSetWindowUserPointer(window, &sharedVisualizerData);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(cfg.vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    // Inicializar GLEW tras crear el contexto
    glewExperimental = GL_TRUE;
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "Render: fallo en glewInit: " << glewGetErrorString(glewErr) << std::endl;
    }

    int fbw = 1024, fbh = 600;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    FramebufferSizeCallback(window, fbw, fbh);

    // Inicializar Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyModernObsidianTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // -------------------------------------------------------------------------
    // Compilación de Shaders
    // -------------------------------------------------------------------------
    std::string barsVertSrc = LoadShaderSource("shaders/bars.vert", kBarsVertFallback);
    std::string barsFragSrc = LoadShaderSource("shaders/bars.frag", kBarsFragFallback);
    GLuint progBars = CreateProgram(barsVertSrc, barsFragSrc, "Bars");

    std::string quadVertSrc = LoadShaderSource("shaders/quad.vert", kQuadVertFallback);
    std::string radialFragSrc = LoadShaderSource("shaders/radial.frag", kRadialFragFallback);
    GLuint progRadial = CreateProgram(quadVertSrc, radialFragSrc, "Radial");

    std::string waveFragSrc = LoadShaderSource("shaders/waveform.frag", kWaveformFragFallback);
    GLuint progWaveform = CreateProgram(quadVertSrc, waveFragSrc, "Waveform");

    std::string waterFragSrc = LoadShaderSource("shaders/waterfall.frag", kWaterfallFragFallback);
    GLuint progWaterfall = CreateProgram(quadVertSrc, waterFragSrc, "Waterfall");

    // -------------------------------------------------------------------------
    // Buffers para Modo Barras
    // -------------------------------------------------------------------------
    GLuint vaoBars = 0, vboBars = 0, eboBars = 0;
    glGenVertexArrays(1, &vaoBars);
    glGenBuffers(1, &vboBars);
    glGenBuffers(1, &eboBars);

    glBindVertexArray(vaoBars);
    glBindBuffer(GL_ARRAY_BUFFER, vboBars);
    // a_pos
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BarVertex), (void*)offsetof(BarVertex, x));
    glEnableVertexAttribArray(0);
    // a_color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(BarVertex), (void*)offsetof(BarVertex, r));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // -------------------------------------------------------------------------
    // Quad de Pantalla Completa para Modos Procedurales (Radial, Waveform, Waterfall)
    // -------------------------------------------------------------------------
    float quadVertices[] = {
        // x, y,     u, v
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
    };
    unsigned int quadIndices[] = { 0, 1, 2, 0, 2, 3 };

    GLuint vaoQuad = 0, vboQuad = 0, eboQuad = 0;
    glGenVertexArrays(1, &vaoQuad);
    glGenBuffers(1, &vboQuad);
    glGenBuffers(1, &eboQuad);

    glBindVertexArray(vaoQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboQuad);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    // -------------------------------------------------------------------------
    // Texturas de Datos de Audio (Espectro, Forma de Onda, Cascada)
    // -------------------------------------------------------------------------
    GLuint texSpectrum = 0;
    glGenTextures(1, &texSpectrum);
    glBindTexture(GL_TEXTURE_2D, texSpectrum);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint texWaveform = 0;
    glGenTextures(1, &texWaveform);
    glBindTexture(GL_TEXTURE_2D, texWaveform);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    constexpr int WATERFALL_WIDTH = 512;
    constexpr int WATERFALL_HEIGHT = 256;
    GLuint texWaterfall = 0;
    glGenTextures(1, &texWaterfall);
    glBindTexture(GL_TEXTURE_2D, texWaterfall);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    std::vector<float> waterfall_history(WATERFALL_WIDTH * WATERFALL_HEIGHT, 0.0f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, WATERFALL_WIDTH, WATERFALL_HEIGHT, 0, GL_RED, GL_FLOAT, waterfall_history.data());
    int waterfall_row = 0;

    // -------------------------------------------------------------------------
    // Estado y Bucle Principal
    // -------------------------------------------------------------------------
    std::vector<float> spectrum;
    std::vector<float> waveform;
    std::vector<float> heights;
    std::vector<float> peaks;
    std::vector<float> peak_timers;

    uint64_t seen_generation = 0;
    uint64_t title_generation = 0;
    int title_frames = 0;
    double last_time = glfwGetTime();
    double title_time = last_time;
    char title[256];

    bool show_hud = false;
    bool tab_prev_state = false;
    bool h_prev_state = false;
    std::string toast_message = "";
    double toast_time = 0.0;

    float fps_history[60] = { 0 };
    int fps_hist_offset = 0;
    float last_fps_val = 0.0f;
    float last_ups_val = 0.0f;

    timeBeginPeriod(1);
    int monitor_hz = RefreshRateForWindow(window);
    auto target_period = [&]() -> double {
        const int hz = cfg.max_fps > 0 ? cfg.max_fps : (monitor_hz > 0 ? monitor_hz : 144);
        return 1.0 / hz;
    };
    double frame_period = target_period();
    bool limiter_active = !cfg.vsync;
    double next_frame_deadline = last_time + frame_period;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        const float dt = static_cast<float>(std::clamp(now - last_time, 0.0, 0.1));
        last_time = now;

        // Atajos de teclado para visibilidad del HUD y modos
        if (!io.WantCaptureKeyboard) {
            bool tab_down = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
            bool h_down = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
            if ((tab_down && !tab_prev_state) || (h_down && !h_prev_state)) {
                show_hud = !show_hud;
            }
            tab_prev_state = tab_down;
            h_prev_state = h_down;

            if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) cfg.visual_mode = MODE_BARS;
            if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) cfg.visual_mode = MODE_RADIAL;
            if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) cfg.visual_mode = MODE_WAVEFORM;
            if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) cfg.visual_mode = MODE_WATERFALL;
        }

        // Obtener el nuevo espectro y waveform si ha cambiado la generación
        const uint64_t generation = sharedVisualizerData.generation.load(std::memory_order_acquire);
        bool has_new_spectrum = (generation != seen_generation);
        if (has_new_spectrum) {
            std::lock_guard<std::mutex> lock(sharedVisualizerData.mtx);
            spectrum = sharedVisualizerData.spectrum;
            waveform = sharedVisualizerData.waveform;
            seen_generation = generation;

            // Actualizar fila circular del espectrograma cascada
            if (!spectrum.empty()) {
                std::vector<float> row(WATERFALL_WIDTH, 0.0f);
                for (int x = 0; x < WATERFALL_WIDTH; ++x) {
                    const int s_idx = static_cast<int>(static_cast<float>(x) / WATERFALL_WIDTH * spectrum.size());
                    row[x] = (s_idx < static_cast<int>(spectrum.size())) ? spectrum[s_idx] * cfg.amplitude_factor : 0.0f;
                }
                glBindTexture(GL_TEXTURE_2D, texWaterfall);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, waterfall_row, WATERFALL_WIDTH, 1, GL_RED, GL_FLOAT, row.data());
                waterfall_row = (waterfall_row + 1) % WATERFALL_HEIGHT;
            }
        }

        const int num_bars = std::max(1, sharedVisualizerData.atomic_num_bars.load());
        if (static_cast<int>(heights.size()) != num_bars) {
            heights.assign(num_bars, 0.0f);
            peaks.assign(num_bars, 0.0f);
            peak_timers.assign(num_bars, 0.0f);
        }

        // Coeficientes de respuesta temporal
        const float a_up = 1.0f - std::exp(-dt * 1000.0f / std::max(0.1f, cfg.attack_ms));
        const float a_down = 1.0f - std::exp(-dt * 1000.0f / std::max(0.1f, cfg.release_ms));
        const float peak_hold_sec = cfg.peak_hold_time_ms * 0.001f;

        // Actualizar física de barras y picos
        const int available = static_cast<int>(spectrum.size());
        for (int i = 0; i < num_bars; ++i) {
            const float raw = (i < available) ? spectrum[i] : 0.0f;
            const float target = std::clamp(raw * cfg.amplitude_factor, 0.0f, 1.0f);

            float& h = heights[i];
            h += (target - h) * (target > h ? a_up : a_down);

            float& p = peaks[i];
            float& timer = peak_timers[i];
            if (h >= p) {
                p = h;
                timer = peak_hold_sec;
            }
            else {
                if (timer > 0.0f) {
                    timer -= dt;
                }
                else {
                    p -= cfg.peak_decay_speed * dt;
                    if (p < h) p = h;
                }
            }
        }

        // Actualizar textura 1D del espectro. Se sube cada cuadro porque contiene las alturas
        // ya animadas (ataque/caída), no el espectro crudo: así el modo radial se mueve a la
        // tasa del monitor y no a la de publicación de espectros.
        if (!heights.empty()) {
            glBindTexture(GL_TEXTURE_2D, texSpectrum);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, static_cast<GLsizei>(heights.size()), 1, 0, GL_RED, GL_FLOAT, heights.data());
        }

        // Actualizar textura 1D de la forma de onda
        if (has_new_spectrum && !waveform.empty()) {
            glBindTexture(GL_TEXTURE_2D, texWaveform);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, static_cast<GLsizei>(waveform.size()), 1, 0, GL_RED, GL_FLOAT, waveform.data());
        }

        glfwGetFramebufferSize(window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ---------------------------------------------------------------------
        // Renderizado del Modo Activo
        // ---------------------------------------------------------------------
        if (cfg.visual_mode == MODE_BARS) {
            glUseProgram(progBars);

            const float bar_width = 2.0f / static_cast<float>(num_bars);
            const float draw_width = bar_width * (1.0f - BAR_GAP_FACTOR);
            const float base_r = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[0] : 0.65f;
            const float base_g = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[1] : 0.15f;
            const float base_b = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[2] : 0.15f;

            const float peak_r = cfg.peak_color_rgb.size() == 3 ? cfg.peak_color_rgb[0] : 1.0f;
            const float peak_g = cfg.peak_color_rgb.size() == 3 ? cfg.peak_color_rgb[1] : 0.85f;
            const float peak_b = cfg.peak_color_rgb.size() == 3 ? cfg.peak_color_rgb[2] : 0.2f;

            std::vector<BarVertex> vertices;
            std::vector<unsigned int> indices;
            vertices.reserve(num_bars * 8);
            indices.reserve(num_bars * 12);

            unsigned int v_idx = 0;
            for (int i = 0; i < num_bars; ++i) {
                const float h = heights[i];
                const float x0 = -1.0f + static_cast<float>(i) * bar_width;
                const float x1 = x0 + draw_width;
                const float y0 = -1.0f;
                const float y1 = h * BAR_MAX_HEIGHT - 1.0f;

                const float cr = std::clamp(base_r + h * 0.45f, 0.0f, 1.0f);
                const float cg = std::clamp(base_g - h * 0.10f, 0.0f, 1.0f);
                const float cb = std::clamp(base_b + h * 0.45f, 0.0f, 1.0f);

                // Quad de la barra
                vertices.push_back({ x0, y0, base_r * 0.5f, base_g * 0.5f, base_b * 0.5f, 1.0f });
                vertices.push_back({ x1, y0, base_r * 0.5f, base_g * 0.5f, base_b * 0.5f, 1.0f });
                vertices.push_back({ x1, y1, cr, cg, cb, 1.0f });
                vertices.push_back({ x0, y1, cr, cg, cb, 1.0f });

                indices.push_back(v_idx + 0); indices.push_back(v_idx + 1); indices.push_back(v_idx + 2);
                indices.push_back(v_idx + 0); indices.push_back(v_idx + 2); indices.push_back(v_idx + 3);
                v_idx += 4;

                // Quad del Peak-Hold (si está habilitado)
                if (cfg.peak_hold_enabled) {
                    const float py0 = peaks[i] * BAR_MAX_HEIGHT - 1.0f;
                    const float py1 = std::min(1.0f, py0 + PEAK_CAP_HEIGHT);
                    vertices.push_back({ x0, py0, peak_r, peak_g, peak_b, 1.0f });
                    vertices.push_back({ x1, py0, peak_r, peak_g, peak_b, 1.0f });
                    vertices.push_back({ x1, py1, peak_r * 1.2f, peak_g * 1.2f, peak_b * 1.2f, 1.0f });
                    vertices.push_back({ x0, py1, peak_r * 1.2f, peak_g * 1.2f, peak_b * 1.2f, 1.0f });

                    indices.push_back(v_idx + 0); indices.push_back(v_idx + 1); indices.push_back(v_idx + 2);
                    indices.push_back(v_idx + 0); indices.push_back(v_idx + 2); indices.push_back(v_idx + 3);
                    v_idx += 4;
                }
            }

            glBindVertexArray(vaoBars);
            glBindBuffer(GL_ARRAY_BUFFER, vboBars);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BarVertex), vertices.data(), GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboBars);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_DYNAMIC_DRAW);

            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        else if (cfg.visual_mode == MODE_RADIAL) {
            glUseProgram(progRadial);
            glUniform2f(glGetUniformLocation(progRadial, "u_resolution"), static_cast<float>(fbw), static_cast<float>(fbh));
            glUniform1f(glGetUniformLocation(progRadial, "u_time"), static_cast<float>(now));
            glUniform1f(glGetUniformLocation(progRadial, "u_amplitude"), cfg.amplitude_factor);

            const float base_r = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[0] : 0.65f;
            const float base_g = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[1] : 0.15f;
            const float base_b = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[2] : 0.15f;
            glUniform3f(glGetUniformLocation(progRadial, "u_base_color"), base_r, base_g, base_b);

            const float peak_r = cfg.peak_color_rgb.size() == 3 ? cfg.peak_color_rgb[0] : 1.0f;
            const float peak_g = cfg.peak_color_rgb.size() == 3 ? cfg.peak_color_rgb[1] : 0.85f;
            const float peak_b = cfg.peak_color_rgb.size() == 3 ? cfg.peak_color_rgb[2] : 0.2f;
            glUniform3f(glGetUniformLocation(progRadial, "u_peak_color"), peak_r, peak_g, peak_b);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texSpectrum);
            glUniform1i(glGetUniformLocation(progRadial, "u_spectrum_tex"), 0);

            glBindVertexArray(vaoQuad);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        else if (cfg.visual_mode == MODE_WAVEFORM) {
            glUseProgram(progWaveform);
            glUniform2f(glGetUniformLocation(progWaveform, "u_resolution"), static_cast<float>(fbw), static_cast<float>(fbh));
            glUniform1f(glGetUniformLocation(progWaveform, "u_time"), static_cast<float>(now));
            glUniform1f(glGetUniformLocation(progWaveform, "u_amplitude"), cfg.amplitude_factor);

            const float base_r = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[0] : 0.2f;
            const float base_g = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[1] : 0.85f;
            const float base_b = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[2] : 0.65f;
            glUniform3f(glGetUniformLocation(progWaveform, "u_line_color"), base_r, base_g, base_b);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texWaveform);
            glUniform1i(glGetUniformLocation(progWaveform, "u_waveform_tex"), 0);

            glBindVertexArray(vaoQuad);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        else if (cfg.visual_mode == MODE_WATERFALL) {
            glUseProgram(progWaterfall);
            glUniform2f(glGetUniformLocation(progWaterfall, "u_resolution"), static_cast<float>(fbw), static_cast<float>(fbh));
            glUniform1f(glGetUniformLocation(progWaterfall, "u_scroll_head"), static_cast<float>(waterfall_row) / WATERFALL_HEIGHT);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texWaterfall);
            glUniform1i(glGetUniformLocation(progWaterfall, "u_waterfall_tex"), 0);

            glBindVertexArray(vaoQuad);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        // ---------------------------------------------------------------------
        // Renderizado de la Interfaz Dear ImGui
        // ---------------------------------------------------------------------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_hud) {
            ImGui::SetNextWindowPos(ImVec2(24, 24), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(480, 600), ImGuiCond_FirstUseEver);

            if (ImGui::Begin("Audio Visualizer 2.0  -  Panel de Control", &show_hud, ImGuiWindowFlags_NoCollapse)) {
                // Header superior con indicador de modo y ayuda de atajo
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "ESTADO:");
                ImGui::SameLine();
                const char* modeStr = (cfg.visual_mode == MODE_BARS) ? "Barras con Peak-Hold" :
                                      (cfg.visual_mode == MODE_RADIAL) ? "Radial / Circular" :
                                      (cfg.visual_mode == MODE_WAVEFORM) ? "Osciloscopio CRT" : "Cascada 2D";
                ImGui::Text("%s", modeStr);
                ImGui::SameLine(ImGui::GetWindowWidth() - 150);
                ImGui::TextDisabled("Atajo: Tab / H");

                ImGui::Spacing();

                if (ImGui::BeginTabBar("VisualizerTabBar", ImGuiTabBarFlags_None)) {
                    // -------------------------------------------------------------
                    // Tab 1: Modos y Efectos
                    // -------------------------------------------------------------
                    if (ImGui::BeginTabItem(" Modos y Efectos ")) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Seleccion de Modo Visual:");

                        if (ImGui::RadioButton("1. Barras de Espectro (Ecualizador)", cfg.visual_mode == MODE_BARS)) cfg.visual_mode = MODE_BARS;
                        ImGui::SameLine(); HelpMarker("Muestra las frecuencias en columnas verticales con ecualizacion de alta fidelidad.");

                        if (ImGui::RadioButton("2. Radial / Circular (Anillo Reactivo)", cfg.visual_mode == MODE_RADIAL)) cfg.visual_mode = MODE_RADIAL;
                        ImGui::SameLine(); HelpMarker("Mapeo polar procedural en anillo con nucleo pulsante al ritmo del bombo y subgraves.");

                        if (ImGui::RadioButton("3. Osciloscopio / Forma de Onda", cfg.visual_mode == MODE_WAVEFORM)) cfg.visual_mode = MODE_WAVEFORM;
                        ImGui::SameLine(); HelpMarker("Muestra las muestras crudas de audio en tiempo real con efecto haz CRT de fosforo analogico antialiased.");

                        if (ImGui::RadioButton("4. Espectrograma Cascada 2D (Waterfall)", cfg.visual_mode == MODE_WATERFALL)) cfg.visual_mode = MODE_WATERFALL;
                        ImGui::SameLine(); HelpMarker("Historial continuo de frecuencias desplazandose hacia abajo con un mapa termico continuo estilo Inferno.");

                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Mecanica de Picos (Peak-Hold en Barras):");
                        ImGui::Checkbox("Habilitar Marcadores de Pico", &cfg.peak_hold_enabled);
                        ImGui::SameLine(); HelpMarker("Sostiene una marca horizontal luminosa en el valor mas alto antes de descender por gravedad.");

                        if (!cfg.peak_hold_enabled) ImGui::BeginDisabled();
                        ImGui::SliderFloat("Retardo Pico", &cfg.peak_hold_time_ms, 50.0f, 1000.0f, "%.0f ms");
                        ImGui::SameLine(); HelpMarker("Milisegundos que el marcador se mantiene en la cima antes de comenzar a caer.");

                        ImGui::SliderFloat("Velocidad de Caida", &cfg.peak_decay_speed, 0.5f, 5.0f, "%.1fx");
                        ImGui::SameLine(); HelpMarker("Rapidez de descenso gravitatorio de los picos cuando se agota el retardo.");
                        if (!cfg.peak_hold_enabled) ImGui::EndDisabled();

                        ImGui::EndTabItem();
                    }

                    // -------------------------------------------------------------
                    // Tab 2: DSP y Dinamica
                    // -------------------------------------------------------------
                    if (ImGui::BeginTabItem(" DSP y Dinamica ")) {
                        ImGui::Spacing();

                        // Tarjeta de estado de audio
                        ImGui::BeginChild("AudioInfoCard", ImVec2(0, 68), true);
                        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "DISPOSITIVO DE AUDIO ACTIVO");
                        std::string currentDevName = "";
                        {
                            std::lock_guard<std::mutex> lock(sharedVisualizerData.dev_mtx);
                            currentDevName = sharedVisualizerData.current_device_name;
                        }
                        ImGui::TextUnformatted(currentDevName.empty() ? "Dispositivo por defecto de Windows" : currentDevName.c_str());
                        ImGui::TextDisabled("Frecuencia: %d Hz | Modo: Loopback WASAPI", sharedAudioData.sample_rate.load());
                        ImGui::EndChild();

                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Cambiar Dispositivo de Reproduccion:");

                        std::vector<AudioDeviceInfo> devList;
                        {
                            std::lock_guard<std::mutex> lock(sharedVisualizerData.dev_mtx);
                            devList = sharedVisualizerData.devices;
                        }

                        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 110);
                        if (ImGui::BeginCombo("##DeviceCombo", currentDevName.empty() ? "Predeterminado" : currentDevName.c_str())) {
                            for (const auto& dev : devList) {
                                bool is_selected = (dev.name == currentDevName);
                                std::string label = dev.name + (dev.is_default ? " [Predeterminado]" : "");
                                if (ImGui::Selectable(label.c_str(), is_selected)) {
                                    std::lock_guard<std::mutex> lock(sharedVisualizerData.dev_mtx);
                                    sharedVisualizerData.requested_device_id = dev.id;
                                    sharedVisualizerData.device_change_pending.store(true);
                                    cfg.selected_device_name = dev.name;
                                    toast_message = "Conmutando a: " + dev.name;
                                    toast_time = now;
                                }
                                if (is_selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Refrescar", ImVec2(100, 0))) {
                            std::vector<AudioDeviceInfo> refreshed = EnumerateAudioDevices();
                            std::lock_guard<std::mutex> lock(sharedVisualizerData.dev_mtx);
                            sharedVisualizerData.devices = refreshed;
                            toast_message = "Lista de dispositivos actualizada";
                            toast_time = now;
                        }

                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Parametros de Respuesta Temporal:");
                        ImGui::SliderFloat("Ganancia", &cfg.amplitude_factor, 0.1f, 3.0f, "%.2fx");
                        ImGui::SameLine(); HelpMarker("Multiplicador vertical del espectro. Aumenta este valor si la pista suena baja.");

                        ImGui::SliderFloat("Ataque", &cfg.attack_ms, 1.0f, 80.0f, "%.0f ms");
                        ImGui::SameLine(); HelpMarker("Tiempo de respuesta al subir. Valores bajos reaccionan instantaneamente a bombos y cajas.");

                        ImGui::SliderFloat("Caida (Decay)", &cfg.release_ms, 20.0f, 500.0f, "%.0f ms");
                        ImGui::SameLine(); HelpMarker("Tiempo de descenso inercial. Valores altos suavizan la caida; valores bajos la hacen nerviosa.");

                        ImGui::SliderFloat("Rango Dinamico", &cfg.dynamic_range_db, 20.0f, 100.0f, "%.0f dB");
                        ImGui::SameLine(); HelpMarker("Sensibilidad en decibelios (dBFS). 0 dBFS arriba, -rango abajo.");

                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Distribucion de Frecuencias:");
                        int scale_idx = (cfg.frequency_scale == "log") ? 1 : 0;
                        if (ImGui::RadioButton("Lineal (Hz constantes)", &scale_idx, 0)) cfg.frequency_scale = "linear";
                        ImGui::SameLine();
                        if (ImGui::RadioButton("Logaritmica (Octavas)", &scale_idx, 1)) cfg.frequency_scale = "log";

                        if (cfg.frequency_scale == "log") {
                            ImGui::DragFloatRange2("Rango Hz", &cfg.min_frequency, &cfg.max_frequency, 5.0f, 10.0f, 22000.0f, "Min: %.0f Hz", "Max: %.0f Hz");
                            ImGui::SameLine(); HelpMarker("Limites de frecuencia minima y maxima para la distribucion por octavas.");
                        } else {
                            ImGui::SliderFloat("Hz por Barra", &cfg.bin_grouping_factor, 2.0f, 50.0f, "%.1f Hz");
                            ImGui::SameLine(); HelpMarker("Ancho de banda que abarca cada barra vertical en hercios.");
                        }

                        ImGui::EndTabItem();
                    }

                    // -------------------------------------------------------------
                    // Tab 3: Color y Temas
                    // -------------------------------------------------------------
                    if (ImGui::BeginTabItem(" Color y Temas ")) {
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Colores Personalizados:");

                        if (cfg.base_color_rgb.size() >= 3) {
                            ImGui::ColorEdit3("Color Base", cfg.base_color_rgb.data(), ImGuiColorEditFlags_DisplayRGB);
                            ImGui::SameLine(); HelpMarker("Color de la base de las barras y del anillo central en modo radial.");
                        }
                        if (cfg.peak_color_rgb.size() >= 3) {
                            ImGui::ColorEdit3("Color Picos / Halo", cfg.peak_color_rgb.data(), ImGuiColorEditFlags_DisplayRGB);
                            ImGui::SameLine(); HelpMarker("Color de los marcadores de pico y de los resplandores exteriores.");
                        }

                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Presets Tematicos (1 Clic):");

                        if (ImGui::Button("Cyberpunk Neon", ImVec2(130, 28))) {
                            cfg.base_color_rgb = { 0.05f, 0.75f, 0.95f }; // Cyan
                            cfg.peak_color_rgb = { 1.00f, 0.15f, 0.65f }; // Magenta
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Matrix Emerald", ImVec2(130, 28))) {
                            cfg.base_color_rgb = { 0.10f, 0.85f, 0.35f }; // Verde
                            cfg.peak_color_rgb = { 0.85f, 1.00f, 0.20f }; // Lima
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Solar Amber", ImVec2(130, 28))) {
                            cfg.base_color_rgb = { 0.85f, 0.25f, 0.10f }; // Lava
                            cfg.peak_color_rgb = { 1.00f, 0.85f, 0.20f }; // Oro
                        }

                        if (ImGui::Button("Deep Amethyst", ImVec2(130, 28))) {
                            cfg.base_color_rgb = { 0.60f, 0.15f, 0.90f }; // Purpura
                            cfg.peak_color_rgb = { 0.10f, 0.80f, 1.00f }; // Cyan
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Arctic Ice", ImVec2(130, 28))) {
                            cfg.base_color_rgb = { 0.75f, 0.90f, 1.00f }; // Hielo
                            cfg.peak_color_rgb = { 0.00f, 0.65f, 0.95f }; // Azul glaciar
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Classic Crimson", ImVec2(130, 28))) {
                            cfg.base_color_rgb = { 0.65f, 0.15f, 0.15f }; // Carmesi
                            cfg.peak_color_rgb = { 1.00f, 0.85f, 0.20f }; // Oro
                        }

                        ImGui::EndTabItem();
                    }

                    // -------------------------------------------------------------
                    // Tab 4: Sistema y Telemetria
                    // -------------------------------------------------------------
                    if (ImGui::BeginTabItem(" Telemetria y Sistema ")) {
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Rendimiento en Tiempo Real:");
                        ImGui::PlotLines("##fps_plot", fps_history, 60, fps_hist_offset, "Cuadros por Segundo (ultimos 60s)", 0.0f, 240.0f, ImVec2(0, 65));

                        ImGui::Columns(2, "TelemetriaCols", false);
                        ImGui::Text("FPS Actuales:"); ImGui::NextColumn();
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.5f, 1.0f), "%.1f FPS", last_fps_val); ImGui::NextColumn();

                        ImGui::Text("Refresco Monitor:"); ImGui::NextColumn();
                        ImGui::Text("%d Hz (%s)", monitor_hz, limiter_active ? "Limitador activo" : "VSync Driver"); ImGui::NextColumn();

                        ImGui::Text("Espectros / seg:"); ImGui::NextColumn();
                        ImGui::Text("%.1f esp/s", last_ups_val); ImGui::NextColumn();

                        ImGui::Text("Resolucion Barras:"); ImGui::NextColumn();
                        ImGui::Text("%d px", num_bars); ImGui::NextColumn();
                        ImGui::Columns(1);

                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Sincronizacion de Cuadros:");
                        ImGui::Checkbox("Sincronia Vertical (VSync)", &cfg.vsync);
                        ImGui::SameLine(); HelpMarker("Sincroniza el dibujo con el refresco de pantalla para evitar tearing.");

                        ImGui::SliderInt("Limite Max FPS", &cfg.max_fps, 0, 360, cfg.max_fps == 0 ? "Auto (Monitor Hz)" : "%d FPS");
                        ImGui::SameLine(); HelpMarker("0 = Detecta automaticamente la tasa de refresco del monitor.");

                        ImGui::Separator();
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Persistencia de Ajustes:");
                        if (ImGui::Button("Guardar en config.json", ImVec2(180, 32))) {
                            {
                                std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
                                sharedConfigData.config = cfg;
                                sharedConfigData.version.fetch_add(1);
                            }
                            if (SaveConfig(sharedConfigData, "config.json")) {
                                toast_message = "Configuracion guardada exitosamente!";
                                toast_time = now;
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Restaurar por Defecto", ImVec2(160, 32))) {
                            VisualizerConfig defaultCfg;
                            cfg = defaultCfg;
                            toast_message = "Valores restaurados por defecto";
                            toast_time = now;
                        }

                        if (now - toast_time < 3.0 && !toast_message.empty()) {
                            ImGui::Spacing();
                            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "OK: %s", toast_message.c_str());
                        }

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
            }
            ImGui::End();

            // Sincronizar cambios en vivo con sharedConfigData
            {
                std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
                sharedConfigData.config = cfg;
                sharedConfigData.version.fetch_add(1);
            }
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        // Limitador adaptativo
        if (limiter_active) {
            double t = glfwGetTime();
            while (t < next_frame_deadline - 0.002) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                t = glfwGetTime();
            }
            while (t < next_frame_deadline) {
                std::this_thread::yield();
                t = glfwGetTime();
            }
            next_frame_deadline += frame_period;
            if (t > next_frame_deadline + frame_period) next_frame_deadline = t + frame_period;
        }

        // Métricas en el título de la ventana
        ++title_frames;
        if (now - title_time >= 1.0) {
            const double elapsed = now - title_time;
            const double fps = title_frames / elapsed;
            const double ups = static_cast<double>(seen_generation - title_generation) / elapsed;

            last_fps_val = static_cast<float>(fps);
            last_ups_val = static_cast<float>(ups);
            fps_history[fps_hist_offset] = last_fps_val;
            fps_hist_offset = (fps_hist_offset + 1) % 60;

            monitor_hz = RefreshRateForWindow(window);
            frame_period = target_period();
            if (!limiter_active && monitor_hz > 0 && fps > monitor_hz * 1.5) {
                limiter_active = true;
                next_frame_deadline = glfwGetTime() + frame_period;
            }

            const char* mode_name = "Barras";
            if (cfg.visual_mode == MODE_RADIAL) mode_name = "Radial";
            else if (cfg.visual_mode == MODE_WAVEFORM) mode_name = "Osciloscopio";
            else if (cfg.visual_mode == MODE_WATERFALL) mode_name = "Cascada";

            std::snprintf(title, sizeof(title),
                "Audio Visualizer 2.0  |  [%s]  |  %.0f fps (%d Hz%s)  |  %.0f esp/s  |  audio %d Hz",
                mode_name, fps, monitor_hz, limiter_active ? ", limitador" : ", vsync",
                ups, sharedAudioData.sample_rate.load());
            glfwSetWindowTitle(window, title);
            title_frames = 0;
            title_time = now;
            title_generation = seen_generation;
        }
    }

    // -------------------------------------------------------------------------
    // Limpieza de Recursos y Cierre
    // -------------------------------------------------------------------------
    timeEndPeriod(1);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &vaoBars);
    glDeleteBuffers(1, &vboBars);
    glDeleteBuffers(1, &eboBars);

    glDeleteVertexArrays(1, &vaoQuad);
    glDeleteBuffers(1, &vboQuad);
    glDeleteBuffers(1, &eboQuad);

    glDeleteTextures(1, &texSpectrum);
    glDeleteTextures(1, &texWaveform);
    glDeleteTextures(1, &texWaterfall);

    glDeleteProgram(progBars);
    glDeleteProgram(progRadial);
    glDeleteProgram(progWaveform);
    glDeleteProgram(progWaterfall);

    glfwDestroyWindow(window);
    glfwTerminate();
}
