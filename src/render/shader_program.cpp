#include "render/shader_program.h"

#include <iostream>
#include <fstream>
#include <sstream>

namespace render {
namespace {

GLuint CompileShader(GLenum type, const char* source, const std::string& name) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader [" << name << "] error de compilacion: " << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

std::string LoadShaderSource(const std::string& path, const char* fallback) {
    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
    return std::string(fallback);
}

GLuint CreateProgram(const std::string& vertex_source, const std::string& fragment_source, const char* name) {
    const GLuint vs = CompileShader(GL_VERTEX_SHADER, vertex_source.c_str(), std::string(name) + " VS");
    const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragment_source.c_str(), std::string(name) + " FS");
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program [" << name << "] error de enlace: " << log << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace render
