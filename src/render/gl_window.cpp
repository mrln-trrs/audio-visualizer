#include "render/gl_window.h"

#include <iostream>
#include <algorithm>

namespace render {
namespace {

void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    auto* vis = static_cast<core::VisualizerData*>(glfwGetWindowUserPointer(window));
    if (vis) {
        // Una barra por píxel de ancho. El hilo de análisis se adapta en su siguiente ciclo.
        vis->atomic_num_bars.store(std::max(1, width));
    }
}

} // namespace

GLFWwindow* CreateMainWindow(int width, int height, const char* title, bool vsync, core::VisualizerData* vis) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "Render: no se pudo crear la ventana OpenGL 3.3." << std::endl;
        return nullptr;
    }

    glfwSetWindowUserPointer(window, vis);
    glfwMakeContextCurrent(window);
    // 1 = un cuadro por refresco del monitor. Si el driver lo ignora, FrameLimiter lo detecta.
    glfwSwapInterval(vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    glewExperimental = GL_TRUE;
    const GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Render: fallo en glewInit: " << glewGetErrorString(err) << std::endl;
    }
    // glewInit puede dejar un GL_INVALID_ENUM espurio en perfiles Core; se descarta.
    glGetError();

    int fbw = width, fbh = height;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    FramebufferSizeCallback(window, fbw, fbh);
    return window;
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

} // namespace render
