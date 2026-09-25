#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Creación de la ventana GLFW con contexto OpenGL 3.3 Core e inicialización de GLEW.
namespace render {

// Crea la ventana, hace el contexto actual, inicializa GLEW y registra el callback de tamaño
// que ajusta el viewport. Devuelve nullptr si falla (GLFW queda inicializado y el llamante
// debe llamar a glfwTerminate).
GLFWwindow* CreateMainWindow(int width, int height, const char* title, bool vsync);

// Tasa de refresco del monitor que contiene el centro de la ventana. 0 si no se puede saber.
int RefreshRateForWindow(GLFWwindow* window);

} // namespace render
