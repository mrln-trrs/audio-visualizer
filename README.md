# Audio Visualizer 2.0

Visualizador de audio de alto rendimiento en tiempo real para Windows (C++17). Captura el sonido del sistema mediante loopback WASAPI, calcula su espectro con FFTW en precisión simple y lo renderiza con **Modern OpenGL 3.3 Core Profile** mediante shaders GLSL acelerados por GPU, sincronizado a la tasa de refresco del monitor (60, 144, 240+ Hz).

Incluye un panel de control flotante interactivo en vivo con **Dear ImGui**, efecto físico de **Peak-Hold**, 4 modos de visualización procedurales, selector de dispositivos de salida WASAPI con cambio en caliente y compilación estándar con **CMake**.

- **Lenguaje**: C++17
- **Plataforma**: Windows 10/11, x64
- **Gráficos**: OpenGL 3.3 Core Profile + Shaders GLSL + GLEW 2.1.0 + GLFW 3.4
- **Interfaz**: Dear ImGui (v1.91.5)
- **Audio**: Windows WASAPI Loopback (COM)
- **FFT**: FFTW 3.3.5 float (`fftwf_*`)
- **Build System**: CMake 3.20+ (Visual Studio, Ninja, Clang) y solución `.sln`

---

## Índice

1. [Novedades en la Versión 2.0](#novedades-en-la-versión-20)
2. [Compilar y Ejecutar](#compilar-y-ejecutar)
3. [Documentación Completa (docs/)](#documentación-completa-docs)
4. [Controles y Atajos de Teclado](#controles-y-atajos-de-teclado)
5. [Estructura del Repositorio](#estructura-del-repositorio)
6. [Arquitectura y Sincronización](#arquitectura-y-sincronización)
7. [Modos de Visualización](#modos-de-visualización)
8. [Configuración (config.json)](#configuración-configjson)
9. [Dependencias de Terceros](#dependencias-de-terceros)

---

## Novedades en la Versión 2.0

- **Modern OpenGL 3.3 Core Profile**: Adiós a `glBegin`/`glEnd` (OpenGL 1.1). Ahora todo corre en la GPU con VBO/VAO y shaders GLSL.
- **Panel de Control Dear ImGui en Vivo**: Pulsa `H` o `Tab` para abrir un HUD translúcido y ajustar en tiempo real ataque, caída, ganancia, colores, escalas y presets ("Cyberpunk Neon", "Matrix Emerald", "Solar Amber") sin editar archivos de texto.
- **Marcadores de Pico (Peak-Hold)**: Barras con agujas de pico que flotan en el valor máximo y descienden suavemente con física de gravedad.
- **4 Modos Visuales Conmutables**:
  1. **Barras con Peak-Hold** (Ecualizador clásico de alta fidelidad).
  2. **Radial / Circular** (Anillo reactivo procedural con núcleo pulsante para graves).
  3. **Osciloscopio / Waveform** (Haz analógico tipo tubo CRT de fósforo verde con antialiasing).
  4. **Espectrograma Cascada 2D (Waterfall)** (Mapa térmico histórico con desplazamiento continuo).
- **Selector de Dispositivos WASAPI**: Lista desplegable en la UI para conmutar entre auriculares, altavoces o interfaces de audio sin reiniciar la app.
- **Persistencia Zero-Restart**: Botón en la UI para guardar la configuración calibrada directamente en `config.json`.
- **Build System con CMake**: Compilación moderna e independiente de IDE con un único `CMakeLists.txt`.

---

## Compilar y Ejecutar

### Opción 1: Con CMake (Recomendada)
Requisitos: CMake 3.20+ y Visual Studio 2022/2026 con C++ desktop (o Ninja).

```powershell
# Generar archivos de construcción
cmake -B build -S . -A x64

# Compilar en modo Release
cmake --build build --config Release

# Ejecutar
.\build\Release\audio-visualizer.exe
```

El paso post-build de CMake copia automáticamente todas las DLLs (`libfftw3f-3.dll`, `glew32.dll`, `glfw3.dll`), los shaders (`shaders/`) y `config.json` a la carpeta del ejecutable.

### Opción 2: Desde Visual Studio (.sln)
Abrir `audio-visualizer.sln`, seleccionar la configuración `Release|x64` y pulsar **F5**.

---

## Documentación Completa (docs/)

El repositorio cuenta con una suite documental estructurada en la carpeta [`docs/`](docs/):

| Documento | Descripción |
|---|---|
| [01. PRD Lean](docs/01_prd_lean.md) | Visión, objetivos, matriz In/Out Scope y requerimientos funcionales/no funcionales. |
| [02. Especificación Técnica](docs/02_tech_spec.md) | Arquitectura de hilos, shaders GLSL, Dear ImGui, WASAPI y CMake. |
| [03. UX Flows y 7 Estados](docs/03_ux_flows.md) | Ciclo de vida del sistema, ergonomía y mapa de estados visuales. |
| [04. Kanban y Criterios BDD](docs/04_kanban_bdd.md) | Épicas, DoD, DoR y escenarios de prueba formales *Given-When-Then*. |
| [05. Arquitectura y Pipeline Matemático](docs/05_architecture_and_pipeline.md) | Fórmulas de Hann, FFTW r2c, mapeo de octavas, dBFS, filtros IIR independientes de FPS y shaders. |
| [06. Guía de Compilación y Toolchain](docs/06_build_and_toolchain.md) | Instrucciones para CMake, Ninja, Visual Studio, VS Code y resolución de errores. |
| [07. Manual de Usuario y Configuración](docs/07_user_manual_and_config.md) | Guía del panel interactivo, atajos, telemetría y referencia de `config.json`. |
| [08. Guía de Contribución y Estándares](docs/08_contributing_and_standards.md) | Estándares C++17, concurrencia, RAII y tutorial para añadir nuevos modos visuales. |
| [09. Recomendaciones y Mejoras](docs/09_recommendations_and_future_improvements.md) | Cola SPSC Lock-Free, Bloom/Glow, Compute Shaders, iconografía y CI/CD con GitHub Actions. |
| [10. Teoría de Descomposición en Bandas](docs/10_signal_decomposition_theory.md) | Demostraciones: inversión de la DFT, Parseval, reconstrucción perfecta por solapamiento, teorema de la suma de bandas, incertidumbre de Gabor, conservación de la información, volúmenes de datos, CQT. Propuesta 3.0. |
| [11. Arquitectura de la Trama de Análisis](docs/11_analysis_frame_architecture.md) | `AnalysisFrame`, bandas configurables, triple búfer, texturas, dinámica por banda, osciloscopio apilado, configuración, presupuesto y fases. Propuesta 3.0. |
| [12. Diseño Fluent y Post-procesado](docs/12_fluent_design_ui.md) | Mica y Acrílico del sistema, material acrílico propio, desenfoque separable demostrado, ruido, integración con ImGui, movimiento, DPI, contraste WCAG. Propuesta 3.0. |
| [Índice Maestro de Docs](docs/README.md) | Mapa de navegación completo de la documentación técnica. |

---

## Controles y Atajos de Teclado

| Tecla | Acción |
|---|---|
| `Tab` o `H` | Mostrar / Ocultar el panel de control interactivo de Dear ImGui |
| `1` | Activar Modo 1: **Barras con Peak-Hold** |
| `2` | Activar Modo 2: **Radial / Circular** |
| `3` | Activar Modo 3: **Osciloscopio / Waveform** |
| `4` | Activar Modo 4: **Espectrograma Cascada (Waterfall)** |

---

## Estructura del Repositorio

```
audio-visualizer/
|-- CMakeLists.txt                  Configuración de construcción CMake moderna
|-- audio-visualizer.sln            Solución de Visual Studio
|-- config.json                     Configuración inicial en tiempo de ejecución
|-- docs/                           Documentación formal (PRD, Tech Spec, UX Flows, Kanban)
|-- shaders/                        Shaders GLSL (Modern OpenGL 3.3 Core)
|   |-- bars.vert / bars.frag       Shader para barras y marcadores de pico
|   |-- quad.vert                   Vertex shader común para modos de pantalla completa
|   |-- radial.frag                 Fragment shader para espectro circular
|   |-- waveform.frag               Fragment shader para osciloscopio analógico CRT
|   `-- waterfall.frag              Fragment shader para espectrograma térmico cascada
|
|-- src/                            Codigo fuente, una carpeta por capa
|   |-- main.cpp                    Punto de entrada: app::Run()
|   |-- app/application.*           Ciclo de vida: configuracion, hilos y cierre ordenado
|   |-- core/                       Sin dependencias de Windows ni OpenGL
|   |   |-- constants.h             FFT_SIZE, HOP_SIZE, RING_SIZE
|   |   |-- types.*                 VisualizerMode, AudioDeviceInfo
|   |   |-- shared_state.h          AudioData, VisualizerData (puntos de intercambio)
|   |   `-- config.*                VisualizerConfig, LoadConfig, SaveConfig
|   |-- audio/                      Captura WASAPI
|   |   |-- sample_format.*         Deteccion de formato y mezcla a mono
|   |   |-- device_enumerator.*     Endpoints y nombres amigables
|   |   |-- capture_session.*       Una sesion de loopback sobre un endpoint
|   |   `-- capture_thread.*        COM, MMCSS, reintentos y cambio de dispositivo
|   |-- analysis/                   Procesado de senal
|   |   |-- window_function.*       Hann periodica y normalizacion
|   |   |-- band_mapper.*           Bins a barras, lineal o logaritmico, interpolacion
|   |   `-- analysis_thread.*       Ventana deslizante, FFT, dB, publicacion
|   |-- render/                     OpenGL 3.3 Core
|   |   |-- gl_window.*             Ventana GLFW, GLEW, refresco del monitor
|   |   |-- shader_program.*        Carga y compilacion GLSL
|   |   |-- embedded_shaders.h      Copias de shaders/ para arrancar sin la carpeta
|   |   |-- fullscreen_quad.*       Quad compartido por los modos procedurales
|   |   |-- data_textures.*         Texturas de espectro, onda y cascada
|   |   |-- spectrum_dynamics.*     Ataque, caida y peak-hold por tiempo real
|   |   |-- frame_limiter.*         Limitador adaptativo cuando el driver ignora vsync
|   |   |-- renderer.*              Bucle principal de render
|   |   `-- modes/                  Un modulo por modo visual (IVisualMode)
|   |       |-- visual_mode.h       Interfaz y RenderContext
|   |       |-- bars_mode.*         1. Barras con peak-hold
|   |       |-- radial_mode.*       2. Radial
|   |       |-- waveform_mode.*     3. Osciloscopio
|   |       `-- waterfall_mode.*    4. Cascada
|   `-- ui/                         Dear ImGui
|       |-- theme.*                 Estilo y HelpMarker
|       |-- telemetry.*             Metricas y titulo de la ventana
|       `-- hud.*                   Panel de control por pestanas
|
|-- fftw-3.3.5-dll64/               FFTW 3.3.5 (DLL, headers y .lib x64)
|-- glew-2.1.0/                     GLEW 2.1.0 (Extension loader para OpenGL 3.3+)
|-- glfw-3.4.bin.WIN64/             GLFW 3.4 (Ventana y contexto)
|-- imgui-1.91.5/                   Dear ImGui 1.91.5 (núcleo y backends GLFW + OpenGL3)
`-- json-develop/                   nlohmann/json 3.12.0 (Serialización JSON)
```

---

## Arquitectura y Sincronización

Tres hilos concurrentes desacoplados sin contención de bloqueo en el hilo de render:

```mermaid
flowchart LR
    WASAPI[WASAPI Loopback] -- 10ms packets --> CAP[AudioCaptureThread]
    CAP -- Lock & Notify --> RING[(AudioData: Ring Buffer)]
    RING -- Predicate Wait --> FFT[AudioProcessingThread]
    FFT -- Lock & Swap --> SPEC[(VisualizerData: Spectrum + Waveform)]
    SPEC -- Read if gen++ --> REN[Main Render Thread (144+ FPS)]
    REN -- Dear ImGui & Shaders --> DISP[Monitor / Pantalla]
```

- **Captura (WASAPI)**: Extrae paquetes de audio de la salida activa o seleccionada, convierte a flotante mono y escribe en el anillo circular de 8192 muestras.
- **Procesado (FFT)**: Toma las 2048 muestras más recientes cada 256 muestras nuevas, aplica ventana de Hann periódica, ejecuta FFTW r2c (1025 bins), interpola bandas en escala lineal/logarítmica y normaliza en dB.
- **Renderizado (OpenGL 3.3 + ImGui)**: Dibuja las barras o texturas procedurales con shaders GLSL a la frecuencia de actualización del monitor y procesa la interfaz interactiva.

---

## Modos de Visualización

1. **Barras con Peak-Hold**: Cada barra representa una banda de frecuencia. Un marcador horizontal superior conserva el pico más alto durante el retardo configurado (`peak_hold_time_ms`) y desciende por gravedad (`peak_decay_speed`).
2. **Radial / Circular**: Proyecta las frecuencias en coordenadas polares en forma de anillo reactivo. Los graves pulsan el núcleo central en sintonía con el bombo.
3. **Osciloscopio / Waveform**: Dibuja la forma de onda temporal en tiempo real con un haz antialiased que simula el fósforo de un osciloscopio CRT clásico.
4. **Espectrograma Cascada (Waterfall)**: Mapea la evolución temporal del espectro hacia abajo en una textura 2D mediante una paleta de calor térmico estilo *Inferno*.

---

## Configuración (config.json)

Los valores se pueden modificar desde el panel de control Dear ImGui (`H` / `Tab`) y guardarse con el botón **"Guardar en config.json"**:

```json
{
  "estilos": {
    "amplitude_factor": 1.0,
    "attack_ms": 12.0,
    "base_color_rgb": [0.65, 0.15, 0.15],
    "bin_grouping_factor": 10.0,
    "dynamic_range_db": 60.0,
    "frequency_scale": "linear",
    "max_fps": 0,
    "max_frequency": 16000.0,
    "min_frequency": 30.0,
    "peak_color_rgb": [1.0, 0.85, 0.2],
    "peak_decay_speed": 1.8,
    "peak_hold_enabled": true,
    "peak_hold_time_ms": 350.0,
    "release_ms": 160.0,
    "selected_device_name": "",
    "visual_mode": 0,
    "vsync": true
  }
}
```

---

## Hoja de Ruta: Versión 3.0 (Propuesta)

La versión 2.0 reduce el análisis a un vector de alturas de barra. La 3.0 propuesta recupera toda la información que la transformada de Fourier extrae del audio y la expone a cualquier renderizador:

- **Descomposición en bandas con reconstrucción exacta.** La pista se separa en las ondas de cada rango de frecuencias y la suma de esas ondas es idéntica a la señal original. Está demostrado en el documento 10 y diseñado en el 11.
- **Límite físico explícito.** No existe "ver cada frecuencia" en tiempo real: resolución en frecuencia y retraso están ligados por el principio de incertidumbre. La interfaz mostrará siempre la latencia asociada a la resolución elegida.
- **Osciloscopio apilado**, medidores por banda, dinámica de ataque y caída por banda, y detección de golpes por flujo espectral.
- **Diseño Fluent.** Panel con material acrílico propio (desenfoque, tinte, exclusión, ruido), materiales Mica y Acrílico del sistema en Windows 11, transiciones con curvas de aceleración y contraste mínimo 4,5:1. Documento 12.

Estado: solo documentación. El orden de ejecución y los criterios de aceptación están en el documento 04, épicas 5 a 7.

---

## Dependencias de Terceros

Todas las bibliotecas están versionadas dentro del repositorio. No se descarga nada al configurar ni al compilar:

| Biblioteca | Versión | Tipo | Función |
|---|---|---|---|
| **FFTW** | 3.3.5 | Binarios versionados | Transformada rápida de Fourier r2c en precisión simple |
| **GLFW** | 3.4 | Binarios versionados | Ventana, contexto OpenGL, temporizador y eventos |
| **GLEW** | 2.1.0 | Binarios versionados | Carga de extensiones OpenGL 3.3 Core Profile |
| **Dear ImGui** | 1.91.5 | Fuentes versionadas (`imgui-1.91.5/`) | Panel de control interactivo en tiempo real |
| **nlohmann/json** | 3.12.0 | Header-only versionado | Lectura y escritura de `config.json` |
