# Guía de Contribución y Estándares de Código - Audio Visualizer 2.0

> **Estado:** Implementado. Normas vigentes para contribuir a `master`.  
> **Alcance:** Estándares de código, concurrencia, dependencias, documentación y checklist de pull request.  
> **Documentos relacionados:** [04_kanban_bdd.md](04_kanban_bdd.md), [06_build_and_toolchain.md](06_build_and_toolchain.md), [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md)  
> **Convención:** este documento distingue entre lo *implementado* (verificable en el código de `master`) y lo *propuesto* (diseño para la versión 3.0). Toda cifra cuantitativa se deriva o se referencia; no hay estimaciones sin base.

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

## 1.5 Estándares de Documentación

1. **Cabecera de estado**: Todo documento de `docs/` empieza con un bloque que declara *Estado* (Implementado, Propuesta o Mixto), *Alcance* y *Documentos relacionados*.
2. **Rigor cuantitativo**: Ninguna cifra (latencia, coste, volumen de datos, contraste) se afirma sin derivarla en el documento o remitir a la derivación. Las demostraciones matemáticas terminan con $\blacksquare$.
3. **Sin emojis ni iconografía decorativa**. Los diagramas son Mermaid; las fórmulas, LaTeX entre `$$`.
4. **Enlaces relativos**: Nunca rutas absolutas de una máquina concreta (`file:///D:/...`).
5. **Separación entre lo implementado y lo propuesto**: Una función que no existe en `master` se describe siempre en tiempo condicional o bajo un encabezado que la marque como propuesta.
6. **Sincronía con el código**: Un cambio de clave de configuración, atajo o comando de compilación no se fusiona sin actualizar los documentos 06 y 07 y el `README.md`.

---

## 2. Guía Paso a Paso: Cómo Agregar un Nuevo Modo Visual

Cada modo es una clase que implementa `render::IVisualMode` en `src/render/modes/`. Supongamos un modo nuevo, "Partículas".

### Paso 1: Registrar el identificador en `src/core/types.h`
```cpp
enum VisualizerMode {
    MODE_BARS = 0,
    MODE_RADIAL = 1,
    MODE_WAVEFORM = 2,
    MODE_WATERFALL = 3,
    MODE_PARTICLES = 4,   // nuevo
    MODE_COUNT
};
```
Y su nombre en `VisualizerModeName` (`src/core/types.cpp`). `MODE_COUNT` dimensiona la tabla de modos del render y valida `visual_mode` al cargar la configuración.

### Paso 2: Escribir el shader en `shaders/particles.frag` y su copia embebida
El fragment shader recibe los mismos uniformes que los modos procedurales existentes (`u_spectrum_tex`, `u_resolution`, `u_time`, `u_base_color`, `u_peak_color`, `u_amplitude`). Añadir la copia literal en `src/render/embedded_shaders.h` como `kParticlesFrag`, para que el ejecutable arranque aunque falte la carpeta `shaders/`.

### Paso 3: Crear la clase del modo
`src/render/modes/particles_mode.h`:
```cpp
#pragma once
#include <GL/glew.h>
#include "render/modes/visual_mode.h"

namespace render {
class ParticlesMode : public IVisualMode {
public:
    bool Init() override;
    void Render(const RenderContext& ctx) override;
    void Shutdown() override;
    const char* Name() const override { return "Particulas"; }
private:
    GLuint program_ = 0;
};
}
```
`src/render/modes/particles_mode.cpp` sigue el patrón de `radial_mode.cpp`: `Init` compila con `CreateProgram(LoadShaderSource("shaders/quad.vert", embedded::kQuadVert), LoadShaderSource("shaders/particles.frag", embedded::kParticlesFrag), "Particles")`; `Render` fija uniformes, enlaza `ctx.textures.spectrum()` y llama a `ctx.quad.Draw()`; `Shutdown` borra el programa.

### Paso 4: Registrar el modo en `src/render/renderer.cpp`
En la tabla `modes`:
```cpp
modes[core::MODE_PARTICLES] = std::make_unique<ParticlesMode>();
```
Y en `KeyboardShortcuts::Poll`, la tecla: `{ GLFW_KEY_5, core::MODE_PARTICLES }`.

### Paso 5: Exponerlo en el HUD (`src/ui/hud.cpp`, función `TabModes`)
```cpp
if (ImGui::RadioButton("5. Particulas", cfg.visual_mode == core::MODE_PARTICLES)) cfg.visual_mode = core::MODE_PARTICLES;
```

### Paso 6: Añadir los archivos al build
Los tres sistemas leen la lista de fuentes de forma explícita. En lugar de editarlos a mano, ejecutar `python tools\update_build_lists.py`, que regenera `PROJECT_SOURCES` en `CMakeLists.txt`, los grupos `ClCompile`, `ClInclude` y `None` de `audio-visualizer.vcxproj` y los filtros por carpeta de `audio-visualizer.vcxproj.filters` a partir del árbol `src/`, `shaders/` e `imgui-1.91.5/`.

### Paso 7: Compilar, verificar y documentar
```powershell
cmake --build build --config Release
.\build\Release\audio-visualizer.exe
```
Actualizar el documento 07 (atajos y modos) y el `README.md`.

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

---

## 5. Guía Prevista: Cómo Añadir una Métrica a la Trama de Análisis (3.0)

Cuando exista `AnalysisFrame` (documento 11, sección 3), añadir una métrica nueva, por ejemplo la planitud espectral, seguirá estos pasos:

1. Declarar el campo en `AnalysisFrame` con su unidad en el comentario y, si es vectorial, su longitud ($N/2+1$ o $K$).
2. Calcularlo en `AudioProcessingThread` tras la magnitud, sin bloqueos tomados, y documentar su coste en operaciones por trama en el documento 11, sección 10.
3. Si algún renderizador lo consume, subirlo a `tex_band_state` o a una textura nueva de un canal, y registrar la textura en la tabla del documento 11, sección 6.
4. Exponerlo en la pestaña "Telemetría" del HUD si tiene interés diagnóstico.
5. Añadir un escenario Given-When-Then al documento 04 con una señal de prueba sintética cuyo valor teórico se conozca (por ejemplo, ruido blanco para la planitud, que debe dar 1,0).
6. Documentar la fórmula y su derivación o referencia en el documento 10 o en un documento nuevo si el fundamento es extenso.
