#pragma once

#include <string>
#include <GL/glew.h>

// Carga y compilación de programas GLSL.
namespace render {

// Lee el archivo `path` relativo al directorio de trabajo; si no existe devuelve `fallback`
// (los shaders embebidos en embedded_shaders.h garantizan que la aplicación arranque siempre).
std::string LoadShaderSource(const std::string& path, const char* fallback);

// Compila y enlaza. Los errores se escriben en cerr con `name` como etiqueta. Devuelve 0 si falla.
GLuint CreateProgram(const std::string& vertex_source, const std::string& fragment_source, const char* name);

} // namespace render
