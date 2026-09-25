# Guía de Contribución y Estándares de Código - Audio Visualizer 2.0

Esta guía define las directrices de ingeniería de software, arquitectura de código en C++17, estándares de concurrencia y el protocolo paso a paso para extender **Audio Visualizer 2.0** con nuevas funcionalidades o modos de visualización.

---

## 1. Principios de Diseño y Estándares C++17

1. **Simplicidad y Eficiencia (KISS & Data-Oriented)**:
   - Evitar jerarquías de herencia complejas o sobreingeniería de patrones cuando estructuras planas (`POD / Structs`) y funciones puras libres bastan.
   - En rutas críticas de audio y gráficos (bucle de cuadros a 144+ FPS), evitar asignaciones dinámicas en el heap (`new`, `std::vector::push_back` con redimensionamiento). Realizar reservas previas con `reserve()` o usar buffers de tamaño fijo.
2. **Gestión de Recursos (RAII)**:
   - Los objetos del sistema operativo (interfaces COM de WASAPI, contextos GLFW, recursos OpenGL VAO/VBO/Texturas y planes FFTW) deben crearse y destruirse de forma explícita y ordenada sin fugas de memoria (*zero memory leaks*).
3. **Concurrencia Segura y Predictible**:
   - Todo dato compartido entre hilos debe estar protegido por un `std::mutex` o ser un tipo atómico (`std::atomic<T>`).
   - Los bloqueos con mutex deben ser de duración mínima (operaciones de copia rápida o `swap()`). Nunca realizar operaciones lentas de I/O, llamadas pesadas de GPU o cálculos de FFT con un mutex tomado.
   - Utilizar siempre `std::condition_variable::wait` con un **predicado** explícito para evitar despertares espurios o notificaciones perdidas.
4. **Convenciones de Nomenclatura**:
   - **Clases y Estructuras**: `PascalCase` (ej. `VisualizerConfig`, `AudioData`, `AudioDeviceInfo`).
   - **Funciones y Métodos**: `PascalCase` (ej. `RenderThread`, `LoadConfig`, `EnumerateAudioDevices`).
   - **Variables y Miembros**: `snake_case` (ej. `peak_hold_time_ms`, `selected_device_id`).
   - **Constantes de Compilación**: `UPPER_SNAKE_CASE` o prefijo `k` (ej. `FFT_SIZE`, `BAR_MAX_HEIGHT`, `kBarsVertFallback`).

---

## 2. Guía Paso a Paso: Cómo Agregar un Nuevo Modo Visual

Supongamos que deseas agregar un nuevo modo llamado **"Partículas Reactivas" (Particle Orbit)**:

### Paso 1: Registrar el Modo en `common.h`
Abre [`common.h`](../common.h) y añade el identificador en `enum VisualizerMode`:
```cpp
enum VisualizerMode {
    MODE_BARS = 0,
    MODE_RADIAL = 1,
    MODE_WAVEFORM = 2,
    MODE_WATERFALL = 3,
    MODE_PARTICLES = 4 // <-- Nuevo modo
};
```

### Paso 2: Crear el Fragment Shader en `shaders/`
Crea el archivo `shaders/particles.frag`:
```glsl
#version 330 core
in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_spectrum_tex;
uniform vec2 u_resolution;
uniform float u_time;
uniform vec3 u_base_color;
uniform float u_amplitude;

void main() {
    vec2 uv = (gl_FragCoord.xy - 0.5 * u_resolution) / min(u_resolution.x, u_resolution.y);
    // Tu lógica matemática y visual aquí...
    FragColor = vec4(u_base_color, 1.0);
}
```

### Paso 3: Cargar y Compilar en `renderer.cpp`
En [`renderer.cpp`](../renderer.cpp):
1. Añade un fallback en string para emergencias:
   ```cpp
   const char* kParticlesFragFallback = R"(#version 330 core ... )";
   ```
2. Carga y compila el programa durante la inicialización:
   ```cpp
   std::string partFragSrc = LoadShaderSource("shaders/particles.frag", kParticlesFragFallback);
   GLuint progParticles = CreateProgram(quadVertSrc, partFragSrc, "Particles");
   ```
3. En el bloque de limpieza al final de `RenderThread`, añade:
   ```cpp
   glDeleteProgram(progParticles);
   ```

### Paso 4: Implementar la Rama de Renderizado en `RenderThread`
En el bucle de cuadros de [`renderer.cpp`](../renderer.cpp):
```cpp
else if (cfg.visual_mode == MODE_PARTICLES) {
    glUseProgram(progParticles);
    glUniform2f(glGetUniformLocation(progParticles, "u_resolution"), static_cast<float>(fbw), static_cast<float>(fbh));
    glUniform1f(glGetUniformLocation(progParticles, "u_time"), static_cast<float>(now));
    glUniform1f(glGetUniformLocation(progParticles, "u_amplitude"), cfg.amplitude_factor);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texSpectrum);
    glUniform1i(glGetUniformLocation(progParticles, "u_spectrum_tex"), 0);

    glBindVertexArray(vaoQuad);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
```

### Paso 5: Añadir el Control a la Interfaz Dear ImGui y Atajo de Teclado
1. En el atajo de teclado:
   ```cpp
   if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) cfg.visual_mode = MODE_PARTICLES;
   ```
2. En la ventana de Dear ImGui:
   ```cpp
   ImGui::RadioButton("Particulas (5)", &cfg.visual_mode, MODE_PARTICLES);
   ```

### Paso 6: Compilar y Validar
```powershell
cmake --build build --config Release
.\build\Release\audio-visualizer.exe
```

---

## 3. Manejo de Dependencias Externas

- **Librerías C/C++ Header-Only o con Fuentes**: Vendorizar una release fija dentro del repositorio, en una carpeta con nombre y versión (como `imgui-1.91.5/`), incluyendo su archivo de licencia, y registrarla tanto en `CMakeLists.txt` como en el `.vcxproj`. No usar `FetchContent`: requiere red y git en tiempo de configuración y rompe el principio de clonar y compilar.
- **Librerías con Binarios Versionados**: Las dependencias precompiladas (`fftw-3.3.5-dll64`, `glew-2.1.0`, `glfw-3.4.bin.WIN64`) deben residir en la raíz y ser enlazadas como librerías dinámicas (`.dll` + `.lib` de importación) para x64.
- **Paso Post-Build**: Cualquier nueva DLL debe registrarse en el bloque `add_custom_command(TARGET audio-visualizer POST_BUILD ...)` para asegurar que se copie automáticamente a la carpeta de salida.

---

## 4. Checklist para Pull Requests y Definition of Done (DoD)

Antes de fusionar cualquier cambio a `master`:
- [ ] El proyecto compila limpiamente en Release x64 con CMake y MSVC sin advertencias (`/W4`).
- [ ] No hay llamadas directas u obsoletas a OpenGL 1.x (`glBegin`, `glEnd`, `glMatrixMode`).
- [ ] La aplicación arranca sin fugas de memoria ni punteros colgantes en el Administrador de Tareas.
- [ ] El conmutador de dispositivos WASAPI sigue funcionando sin crashear.
- [ ] La sincronización de cuadros alcanza los 144+ FPS esperados del monitor sin quemar núcleos de CPU de forma innecesaria.
- [ ] `README.md` y la documentación técnica en `docs/` reflejan cualquier nueva clave de configuración o atajo de teclado.
