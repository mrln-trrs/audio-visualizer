#pragma once

#include <GL/glew.h>

// Quad de pantalla completa (-1..1) con UV (0..1), compartido por los modos procedurales.
namespace render {

class FullscreenQuad {
public:
    bool Init();
    void Draw() const;
    void Shutdown();

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
};

} // namespace render
