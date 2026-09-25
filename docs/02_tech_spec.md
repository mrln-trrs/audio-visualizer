# Especificación Técnica (Tech Spec) - Audio Visualizer 2.0

> **Estado:** Mixto. Las secciones 1 a 6 describen la versión 2.0 implementada. Las secciones 7 y 8 resumen el diseño propuesto para la 3.0 y remiten a los documentos 11 y 12.  
> **Alcance:** Decisiones de ingeniería: stack, hilos, pipeline gráfico, GUI, WASAPI y sistema de construcción.  
> **Documentos relacionados:** [01_prd_lean.md](01_prd_lean.md), [05_architecture_and_pipeline.md](05_architecture_and_pipeline.md), [10_signal_decomposition_theory.md](10_signal_decomposition_theory.md), [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md), [12_fluent_design_ui.md](12_fluent_design_ui.md)  
> **Convención:** este documento distingue entre lo *implementado* (verificable en el código de `master`) y lo *propuesto* (diseño para la versión 3.0). Toda cifra cuantitativa se deriva o se referencia; no hay estimaciones sin base.

## 1. Stack Tecnológico y Matriz "Build vs. Adopt"

| Dominio | Tecnología / Estándar | Justificación (Build vs. Adopt) |
|---|---|---|
| **Lenguaje Base** | C++17 | Estándar moderno, concurrencia nativa (`std::thread`, `std::atomic`, `std::mutex`), compatibilidad con toolset v143/v145 de MSVC y compiladores GCC/Clang. |
| **Captura de Audio** | Windows WASAPI Loopback (COM) | **Construir lógica propia sobre API nativa**: Máximo rendimiento y mínima latencia en Windows. No requiere drivers virtuales de terceros. |
| **Análisis Espectral** | FFTW 3.3.5 (Float / `fftwf_*`) | **Adoptar**: La biblioteca más rápida del mundo para FFT en CPU con vectorización AVX/SSE. |
| **Ventana y Contexto** | GLFW 3.4 | **Adoptar**: Ligero, maneja DPI, eventos de ventana, multimonitor e intervalos de vsync fiables. |
| **Carga de Extensiones GL**| GLEW 2.1.0 | **Adoptar**: Ya integrado en el repositorio. Carga transparente de símbolos OpenGL 3.3+. |
| **Interfaz de Usuario (GUI)**| Dear ImGui 1.91.5, rama estándar, vendorizado en `imgui-1.91.5/` | **Adoptar**: Estándar de facto en herramientas interactivas C++. Sin dependencias, renderizado inmediato sobre el contexto OpenGL existente. Se compila desde el repositorio, sin descargas. |
| **Composición del escritorio (3.0)** | `dwmapi.dll` (`DwmSetWindowAttribute`) | **Adoptar**: Única vía documentada para Mica y Acrílico del sistema en Windows 11. Opcional y con degradación a fondo opaco. Ver documento 12. |
| **Serialización de Config** | nlohmann/json 3.12.0 | **Adoptar**: Header-only, sintaxis intuitiva y tolerante a claves faltantes. |
| **Build System** | CMake 3.20+ y solución `.sln` en paralelo | **Adoptar**: CMake como ruta independiente del IDE; el `.sln` se mantiene sincronizado para el flujo de Visual Studio. Ambas compilan las mismas fuentes, incluido ImGui, sin red. |

---

## 2. Arquitectura de Hilos y Flujo de Datos

Se mantiene el modelo de 3 hilos concurrentes de alto rendimiento desacoplados mediante búferes circulares y sincronización con predicados de condición:

```mermaid
flowchart TD
    subgraph AudioEngine[Sistema Operativo]
        WASAPI[WASAPI Loopback Client<br/>Endpoint de Audio Seleccionado]
    end

    subgraph ThreadCapture[Hilo 1: AudioCaptureThread]
        CapLoop[Captura paquetes 10ms<br/>Detección de formato PCM/Float<br/>Mezcla a mono]
        DevEnum[IMMDeviceEnumerator<br/>Listado de dispositivos activos]
    end

    subgraph MemorySync1[Estructura AudioData]
        RingBuf[(Ring Buffer Circular: 8192 floats<br/>total_samples atómico<br/>sample_rate atómico)]
        CV1[std::condition_variable<br/>Predicado: total_samples >= consumido + 256]
    end

    subgraph ThreadProcessing[Hilo 2: AudioProcessingThread]
        Windowing[Ventana de Hann periódica]
        FFTExec[FFTW r2c: 1025 bins complejos]
        FreqBands[Mapeo a bandas: lineal / logarítmico<br/>Interpolación de bins estrechos]
        DBScale[Cálculo dBFS: 20 log10<br/>Normalización a rango 0..1]
    end

    subgraph MemorySync2[Estructura VisualizerData]
        SpecBuf[(Espectro normalizado: vector float<br/>Buffer de forma de onda temporal: vector float<br/>generation atómico)]
        DeviceCmd[(std::atomic<bool> reload_device<br/>std::string selected_device_id)]
    end

    subgraph ThreadRender[Hilo Principal: Render & UI Thread]
        InputEvents[Eventos GLFW: Teclado y Mouse]
        ImGuiLayer[Dear ImGui: Panel de Control Flotante<br/>Presets, Colores, Modos, Dispositivos]
        GL3Pipeline[OpenGL 3.3 Core Pipeline<br/>Barras con Peak-Hold / Shaders Procedurales]
        Limiter[Limitador adaptativo de cuadros<br/>Sincronizado a Hz del monitor]
    end

    WASAPI --> CapLoop
    CapLoop --> RingBuf
    CapLoop --> CV1
    CV1 --> ThreadProcessing
    RingBuf --> ThreadProcessing
    ThreadProcessing --> SpecBuf
    SpecBuf --> GL3Pipeline
    ImGuiLayer --> DeviceCmd
    DeviceCmd --> ThreadCapture
    GL3Pipeline --> Limiter
```

---

## 3. Pipeline de Renderizado Modern OpenGL 3.3+

### 3.1 Inicialización de Contexto Core
```cpp
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
```
Tras la creación de ventana GLFW, se inicializa GLEW con `glewExperimental = GL_TRUE; glewInit();`.

### 3.2 Modos de Visualización Implementados

1. **Modo 1: Barras con Peak-Hold (VBO + Shaders)**:
   - Se emplea un VBO con un rectángulo unitario básico y un VBO con atributos instanciados (posición X, ancho, altura interpolada, altura de pico).
   - El Vertex Shader posiciona cada barra y su correspondiente marcador de pico horizontal.
   - El Fragment Shader aplica un degradado suave basado en la altura y un brillo especial para el indicador de pico.
   - La física del *Peak-Hold* almacena un valor de retardo (`hold_time_ms`) y una velocidad de caída por gravedad (`peak_decay_rate`).

2. **Modo 2: Radial / Circular (Procedural Quad + Fragment Shader)**:
   - Se dibuja un Quad de pantalla completa (-1..1).
   - El espectro se sube a una textura 1D (`GL_R32F`).
   - El Fragment Shader calcula coordenadas polares `(r, theta)` desde el centro de la pantalla, muestrea la textura 1D y dibuja un anillo de frecuencias reactivo con simetría circular o espiral.

3. **Modo 3: Osciloscopio / Waveform (Líneas suavizadas)**:
   - Muestra las muestras crudas en el dominio del tiempo desde el anillo de captura.
   - Renderizado con Shader de línea con antialiasing o textura 1D muestreada en pantalla completa.

4. **Modo 4: Espectrograma Cascada 2D (Waterfall)**:
   - Textura 2D flotante de tamaño `(num_barras, 256 historial)`.
   - Cada nuevo espectro desplaza o escribe en una fila circular mediante `glTexSubImage2D`.
   - Fragment shader mapea la amplitud a un mapa de calor térmico (*Inferno* o *Cyberpunk*).

---

## 4. Integración de Dear ImGui

- **Backends**: `imgui_impl_glfw.cpp` y `imgui_impl_opengl3.cpp` (con `#version 330 core`).
- **Ciclo de Cuadro**:
  1. `ImGui_ImplOpenGL3_NewFrame();`
  2. `ImGui_ImplGlfw_NewFrame();`
  3. `ImGui::NewFrame();`
  4. Si `show_hud == true`, renderizar ventana `ImGui::Begin("Audio Visualizer Settings", ...)` con sliders y combos.
  5. `ImGui::Render();`
  6. `ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());`
- **Control de Teclado**: La tecla `H` o `Tab` alterna `show_hud = !show_hud`. Cuando está oculto, ImGui no captura eventos del ratón.

---

## 5. Enumeración y Gestión de Dispositivos WASAPI

### 5.1 Estructura de Dispositivo
```cpp
struct AudioDeviceInfo {
    std::wstring id;
    std::string name;
    bool is_default;
};
```

### 5.2 Algoritmo de Enumeración
1. Instanciar `IMMDeviceEnumerator`.
2. Llamar a `EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection)`.
3. Iterar cada `IMMDevice`, abrir su `IPropertyStore` y leer `PKEY_Device_FriendlyName`.
4. Obtener su ID único con `pDevice->GetId(&pStrId)`.
5. Si el usuario selecciona un nuevo dispositivo en la GUI:
   - Se escribe el `selected_device_id` en una variable protegida.
   - Se activa la bandera atómica `reload_device = true`.
   - El bucle de `AudioCaptureThread` detecta la bandera, libera los objetos COM actuales y reabre el nuevo endpoint sin reiniciar la aplicación ni el renderizador.

---

## 6. Arquitectura del Build System (CMakeLists.txt)

Estructura diseñada para funcionar inmediatamente tras clonar el repositorio:

- **Detección de librerías locales**:
  - `GLFW`: Rutas a `glfw-3.4.bin.WIN64/include` y `glfw-3.4.bin.WIN64/lib-vc2022/glfw3dll.lib`.
  - `GLEW`: Rutas a `glew-2.1.0/include` y `glew-2.1.0/lib/Release/x64/glew32.lib`.
  - `FFTW`: Rutas a `fftw-3.3.5-dll64` y `libfftw3f-3.lib`.
  - `nlohmann_json`: Ruta a `json-develop/include`.
- **Dear ImGui**: Vendorizado en `imgui-1.91.5/` (núcleo `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp` y backends `imgui_impl_glfw.cpp`, `imgui_impl_opengl3.cpp`) y compilado como biblioteca estática `imgui_lib`. Se descartó `FetchContent` porque exige red y git en tiempo de configuración y el clon fallaba de forma intermitente. La solución `.sln` compila los mismos archivos directamente.
- **Comandos Post-Build**: Copia automática de `libfftw3f-3.dll`, `glew32.dll`, `glfw3.dll`, `config.json` y la carpeta `shaders/` al directorio del binario objetivo (`$<TARGET_FILE_DIR:audio-visualizer>`).

---

## 6.5 Organización del Código por Capas

El código fuente vive en `src/` con una carpeta por capa y un archivo por responsabilidad. Las dependencias entre capas van en un solo sentido: `core` no depende de nadie; `audio`, `analysis` y `render` dependen de `core`; `ui` depende de `core` y de `audio` (para refrescar dispositivos); `app` orquesta a todas. Ningún archivo de `audio` o `analysis` incluye OpenGL, y ningún archivo de `render` o `ui` incluye WASAPI.

```mermaid
flowchart TD
    APP[app<br/>application] --> AUD[audio<br/>capture_thread, capture_session<br/>sample_format, device_enumerator]
    APP --> ANA[analysis<br/>analysis_thread, window_function<br/>band_mapper]
    APP --> REN[render<br/>renderer, gl_window, shader_program<br/>data_textures, spectrum_dynamics<br/>frame_limiter, modes/*]
    REN --> UI[ui<br/>hud, theme, telemetry]
    AUD --> CORE[core<br/>constants, types<br/>shared_state, config]
    ANA --> CORE
    REN --> CORE
    UI --> CORE
    UI -. refrescar dispositivos .-> AUD
```

| Capa | Responsabilidad | Dependencias externas |
|---|---|---|
| `core` | Constantes, tipos, estado compartido, configuración | nlohmann/json (solo `config.cpp`) |
| `audio` | Loopback WASAPI, formato de muestra, enumeración, sesión y reintentos | Windows COM, WASAPI, avrt, winmm |
| `analysis` | Ventana, FFT, magnitud, dB, mapeo a barras, publicación | FFTW (`fftwf_*`) |
| `render` | Ventana y contexto, shaders, texturas, dinámica, limitador, post-procesado y material, modos | GLFW, GLEW, OpenGL 3.3 |
| `platform` | Efectos de ventana de Windows (DWM) y preferencias de accesibilidad del sistema | dwmapi, advapi32 |
| `ui` | Panel, tema, telemetría | Dear ImGui |
| `app` | Arranque de hilos y cierre ordenado | Windows (`FreeConsole`) |

Cada modo visual implementa `render::IVisualMode` (`Init`, `Render`, `Shutdown`, `Name`) y recibe un `RenderContext` con el framebuffer, la configuración, las texturas de datos y la dinámica. Añadir un modo no toca el análisis ni el bucle de render más allá de registrar la clase (procedimiento en el documento 08, sección 2).

---

## 7. Capa de Análisis y Descomposición en Bandas (Propuesta 3.0)

Resumen ejecutivo del diseño detallado en [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md), cuya base matemática se demuestra en [10_signal_decomposition_theory.md](10_signal_decomposition_theory.md).

| Aspecto | Versión 2.0 | Versión 3.0 |
|---|---|---|
| Salida del procesado | `vector<float>` de alturas por píxel | `AnalysisFrame`: magnitud, fase, dB, RMS, pico, flujo espectral, centroide y mezcla (fase A); energía, RMS y pico por banda (fase B); ondas por banda y mezcla alineada (fase C). Todo implementado |
| Intercambio | `swap` bajo mutex más copia en el render | Triple búfer sin copia, índice atómico (`core::TripleBuffer`, implementado) |
| Bandas | Implícitas, una por píxel | Configurables: octavas, lineal, manual, por bin; con nombre, color, rampas |
| Ondas | Solo mezcla | Mezcla y una por banda, reconstruidas por IFFT enmascarada con solapamiento (reconstrucción exacta, teorema 4.3 del documento 10) |
| Dinámica | Dos escalares globales | Vectores de ataque y caída por banda |
| Ventana | Hann periódica (ya implementada en `src/analysis/window_function.cpp`) | Sin cambios; es la condición necesaria para la reconstrucción exacta |
| Texturas | Espectro y forma de onda 1D, cascada 2D | Historial de espectro y fase, ondas por banda, estado por banda, UBO de bandas |

Límites que la especificación asume explícitamente:

- Resolución y latencia están ligadas por $\sigma_t \sigma_f \geq 1/(4\pi)$ (documento 10, sección 5). No existe "ver cada frecuencia" en tiempo real; se ofrece resolución variable por rango como compromiso (sección 7 del mismo documento).
- El volumen de datos se acota conservando la STFT compleja (1,5 MB/s) y reconstruyendo solo las bandas visibles (documento 10, sección 6.3).

## 8. Pipeline de Post-procesado y Materiales (Implementado en la 3.0)

Resumen del diseño de [12_fluent_design_ui.md](12_fluent_design_ui.md).

```mermaid
flowchart LR
    S[Escena a FBO<br/>resolucion completa] --> D[Reduccion x4]
    D --> BH[Desenfoque horizontal<br/>9 lecturas bilineales]
    BH --> BV[Desenfoque vertical]
    BV --> M[Material acrilico<br/>exclusion, tinte, ruido]
    S --> P[Presentacion de la escena]
    P --> I[Dear ImGui<br/>fondo alfa 0 + callback del material]
    M --> I
```

- El desenfoque es separable (demostración en el documento 12, sección 4.2): dos pasadas de $2r+1$ lecturas en lugar de $(2r+1)^2$.
- Coste añadido inferior a 0,3 ms por cuadro a 1920 por 1080 en GPU integrada.
- Materiales del sistema (Mica, Acrílico) mediante `DwmSetWindowAttribute` con atributo 38, solo Windows 11 22H2 o superior, con degradación a opaco.
- Contraste mínimo 4,5:1 garantizado por la mezcla por exclusión y la opacidad mínima del tinte.
