#include "renderer.h"
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <algorithm>
#include <thread>
#include <chrono>

// windows.h antes que GLFW (evita redefinir APIENTRY) y sin los macros min/max.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include <GLFW/glfw3.h>
#include <GL/gl.h>

namespace {

// Espacio entre barras como fracción del ancho de barra.
constexpr float BAR_GAP_FACTOR = 0.1f;
// Altura máxima de una barra en coordenadas normalizadas (2.0 = toda la ventana).
constexpr float BAR_MAX_HEIGHT = 1.5f;

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    auto* vis = static_cast<VisualizerData*>(glfwGetWindowUserPointer(window));
    if (vis) {
        // Una barra por píxel de ancho. El hilo de procesado se adapta en su siguiente ciclo.
        vis->atomic_num_bars.store(std::max(1, width));
    }
}

// Tasa de refresco del monitor que contiene el centro de la ventana.
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
    std::cout << "Render: hilo iniciado." << std::endl;

    VisualizerConfig cfg;
    {
        std::lock_guard<std::mutex> lock(sharedConfigData.mtx);
        cfg = sharedConfigData.config;
    }
    const float base_r = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[0] : 0.65f;
    const float base_g = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[1] : 0.15f;
    const float base_b = cfg.base_color_rgb.size() == 3 ? cfg.base_color_rgb[2] : 0.15f;
    const float attack_ms = std::max(0.1f, cfg.attack_ms);
    const float release_ms = std::max(0.1f, cfg.release_ms);

    if (!glfwInit()) {
        std::cerr << "Render: no se pudo inicializar GLFW." << std::endl;
        return;
    }

    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    GLFWwindow* window = glfwCreateWindow(1024, 600, "Audio Visualizer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Render: no se pudo crear la ventana." << std::endl;
        glfwTerminate();
        return;
    }

    glfwSetWindowUserPointer(window, &sharedVisualizerData);
    glfwMakeContextCurrent(window);
    // 1 = un cuadro por refresco del monitor (144 fps en un panel de 144 Hz). 0 = sin límite.
    glfwSwapInterval(cfg.vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
    {
        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        FramebufferSizeCallback(window, fbw, fbh);
    }

    std::vector<float> spectrum;   // copia local del último espectro publicado
    std::vector<float> heights;    // altura animada de cada barra
    uint64_t seen_generation = 0;
    uint64_t title_generation = 0;
    int title_frames = 0;
    double last_time = glfwGetTime();
    double title_time = last_time;
    char title[256];

    // Limitador de cuadros. Algunos drivers ignoran glfwSwapInterval(1) y el bucle corre a miles
    // de fps quemando un núcleo. Se activa solo si se detecta que la sincronía vertical no actúa
    // (fps medidos muy por encima del refresco del monitor) y entonces clava el bucle al refresco.
    timeBeginPeriod(1);
    int monitor_hz = RefreshRateForWindow(window);
    auto target_period = [&]() -> double {
        const int hz = cfg.max_fps > 0 ? cfg.max_fps : (monitor_hz > 0 ? monitor_hz : 144);
        return 1.0 / hz;
    };
    double frame_period = target_period();
    bool limiter_active = !cfg.vsync; // sin vsync el limitador actúa desde el principio
    double next_frame_deadline = last_time + frame_period;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double now = glfwGetTime();
        // dt real del cuadro, acotado para que un tirón no dispare la animación.
        const float dt = static_cast<float>(std::clamp(now - last_time, 0.0, 0.1));
        last_time = now;

        // Recoger el espectro nuevo, si lo hay. El bloqueo dura una copia de ~1000 floats.
        const uint64_t generation = sharedVisualizerData.generation.load(std::memory_order_acquire);
        if (generation != seen_generation) {
            std::lock_guard<std::mutex> lock(sharedVisualizerData.mtx);
            spectrum = sharedVisualizerData.spectrum;
            seen_generation = generation;
        }

        const int num_bars = std::max(1, sharedVisualizerData.atomic_num_bars.load());
        if (static_cast<int>(heights.size()) != num_bars) {
            heights.assign(num_bars, 0.0f);
        }

        // Coeficientes de un filtro de primer orden dependientes del tiempo real transcurrido,
        // así la respuesta es la misma a 60, 144 o 240 fps.
        const float a_up = 1.0f - std::exp(-dt * 1000.0f / attack_ms);
        const float a_down = 1.0f - std::exp(-dt * 1000.0f / release_ms);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const float bar_width = 2.0f / static_cast<float>(num_bars);
        const float draw_width = bar_width * (1.0f - BAR_GAP_FACTOR);
        const int available = static_cast<int>(spectrum.size());

        glBegin(GL_QUADS);
        for (int i = 0; i < num_bars; ++i) {
            const float raw = (i < available) ? spectrum[i] : 0.0f;
            const float target = std::clamp(raw * cfg.amplitude_factor, 0.0f, 1.0f);

            float& h = heights[i];
            h += (target - h) * (target > h ? a_up : a_down);

            const float x0 = -1.0f + static_cast<float>(i) * bar_width;
            const float x1 = x0 + draw_width;
            const float y1 = h * BAR_MAX_HEIGHT - 1.0f;

            glColor3f(base_r + h * 0.5f, base_g - h * 0.5f, base_b + h * 0.5f);
            glVertex2f(x0, -1.0f);
            glVertex2f(x1, -1.0f);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
        }
        glEnd();

        glfwSwapBuffers(window);

        if (limiter_active) {
            // Dormir en pasos de 1 ms hasta ~2 ms antes del plazo y afinar el resto en espera activa.
            double t = glfwGetTime();
            while (t < next_frame_deadline - 0.002) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                t = glfwGetTime();
            }
            while (t < next_frame_deadline) {
                std::this_thread::yield();
                t = glfwGetTime();
            }
            // Plazo del siguiente cuadro anclado al anterior para no acumular deriva; si vamos
            // muy tarde (ventana arrastrada, tirón), se resincroniza en vez de correr para alcanzar.
            next_frame_deadline += frame_period;
            if (t > next_frame_deadline + frame_period) next_frame_deadline = t + frame_period;
        }

        // Métricas en el título una vez por segundo: fps reales, refresco del monitor,
        // espectros por segundo y frecuencia de muestreo del audio.
        ++title_frames;
        if (now - title_time >= 1.0) {
            const double elapsed = now - title_time;
            const double fps = title_frames / elapsed;
            const double ups = static_cast<double>(seen_generation - title_generation) / elapsed;

            monitor_hz = RefreshRateForWindow(window);
            frame_period = target_period();
            if (!limiter_active && monitor_hz > 0 && fps > monitor_hz * 1.5) {
                // El driver no está aplicando la sincronía vertical: tomar el control del ritmo.
                limiter_active = true;
                next_frame_deadline = glfwGetTime() + frame_period;
                std::cout << "Render: vsync ignorado por el driver (" << static_cast<int>(fps)
                    << " fps), limitador activado a " << static_cast<int>(1.0 / frame_period) << " fps." << std::endl;
            }

            std::snprintf(title, sizeof(title),
                "Audio Visualizer  |  %.0f fps (monitor %d Hz%s)  |  %.0f espectros/s  |  audio %d Hz  |  %d barras",
                fps, monitor_hz, limiter_active ? ", limitador" : ", vsync", ups, sharedAudioData.sample_rate.load(), num_bars);
            glfwSetWindowTitle(window, title);
            title_frames = 0;
            title_time = now;
            title_generation = seen_generation;
        }
    }

    timeEndPeriod(1);
    glfwDestroyWindow(window);
    glfwTerminate();
}
