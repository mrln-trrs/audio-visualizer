#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Creación de la ventana GLFW con contexto OpenGL 3.3 Core e inicialización de GLEW.
namespace render {

struct WindowOptions {
    int width = 1024;
    int height = 600;
    const char* title = "Audio Visualizer";
    bool vsync = true;
    // Framebuffer con canal alfa compuesto por el escritorio: necesario para ver los materiales
    // del sistema (Mica, Acrílico) a través de la ventana (docs/12, sección 2).
    bool transparent_framebuffer = false;
    // Escalar la ventana según el DPI del monitor (docs/12, sección 9).
    bool scale_to_monitor = true;
};

// Crea la ventana, hace el contexto actual, inicializa GLEW y registra el callback de tamaño
// que ajusta el viewport. Devuelve nullptr si falla (GLFW queda inicializado y el llamante
// debe llamar a glfwTerminate).
GLFWwindow* CreateMainWindow(const WindowOptions& options);

// Tasa de refresco del monitor que contiene el centro de la ventana. 0 si no se puede saber.
int RefreshRateForWindow(GLFWwindow* window);

} // namespace render
