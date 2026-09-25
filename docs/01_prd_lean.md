# PRD Lean - Audio Visualizer 2.0

## 1. Visión del Producto y Objetivos

**Audio Visualizer 2.0** es una evolución de alto rendimiento para el visualizador de espectro en tiempo real en Windows (C++17). La versión 2.0 traslada el renderizado anticuado en modo inmediato (OpenGL 1.1) a una arquitectura gráfica moderna basada en shaders GLSL (OpenGL 3.3+ Core Profile), incorpora un panel de control interactivo en tiempo real con Dear ImGui, permite conmutar dispositivos de reproducción WASAPI en caliente y unifica la compilación mediante un sistema CMake moderno con cero fricción de dependencias.

### Objetivos Clave (OKRs)
1. **Calidad Visual y Modos**: Proporcionar 4 modos de visualización (Barras con Peak-Hold, Radial/Circular, Osciloscopio/Waveform y Espectrograma Cascada 2D) acelerados 100% por GPU mediante shaders GLSL.
2. **Interactividad Zero-Restart**: Permitir el ajuste en vivo de todos los parámetros de renderizado (ganancia, ataque, decaimiento, paleta de colores RGB, escala lineal/logarítmica) y persistirlos en `config.json` desde la GUI sin reiniciar la app.
3. **Control Total de Audio**: Permitir listar y conmutar dinámicamente cualquier dispositivo de audio activo en Windows (auriculares, altavoces, tarjetas externas, Voicemeeter) mediante enumeración nativa WASAPI.
4. **Build System Estándar**: Permitir compilar el proyecto en Visual Studio 2022/2026, VS Code o terminal con un único `CMakeLists.txt` autosuficiente.

---

## 2. In-Scope vs. Out-of-Scope (Filtro Anti-Sobreingeniería)

Para respetar el principio KISS & YAGNI y evitar dispersión de esfuerzo:

### In-Scope (Versión 2.0)
- **Pipeline Modern OpenGL 3.3 Core Profile**: Inicialización de contexto moderno con GLFW y carga de extensiones con GLEW 2.1.0.
- **Renderizado Híbrido**:
  - Malla VBO/VAO instanciada para Barras de Espectro con indicadores de pico (*Peak-Hold*).
  - Shaders de pantalla completa (Full-screen quad) alimentados con textura 1D de magnitudes espectrales para modos Radial/Circular, Osciloscopio y Espectrograma.
- **Panel de Control Dear ImGui**: Overlay flotante y translúcido con toggle de visibilidad (tecla `H` o `Tab`), paleta de colores, sliders de filtro temporal, selectores de escala y selector de dispositivos de audio.
- **Persistencia de Configuración**: Botón en UI para guardar los cambios en caliente directamente en `config.json`.
- **Enumeración y Cambio de Dispositivos WASAPI**: Lista desplegable con nombres descriptivos de los endpoints de reproducción y reconexión atómica del cliente de captura sin congelar el render.
- **CMakeLists.txt Cero-Fricción**: Configuración CMake que enlaza las librerías versionadas en el repositorio (`fftw-3.3.5-dll64`, `glew-2.1.0`, `glfw-3.4.bin.WIN64`, `json-develop`) e integra Dear ImGui desde la carpeta versionada `imgui-1.91.5`, sin descargas en tiempo de configuración.

### Out-of-Scope (Descartado para evitar sobreingeniería)
- **Soporte de Audio multiplataforma nativo en Linux/macOS**: Mantener el Core de captura enfocado en Windows WASAPI. La abstracción a PulseAudio/CoreAudio se difiere a una v3.0 si fuera demandada.
- **Procesamiento VST/Efectos de Audio**: El visualizador es de solo lectura (análisis y monitorización pasiva), no un DAW o procesador de efectos.
- **Plugins de terceros o scripts Lua**: Todo el renderizado y shaders estarán integrados de forma nativa en C++.
- **Grabación de video/exportación a MP4**: El software es un visualizador interactivo en tiempo real, no un renderizador de video offline.
- **Skins complejas o temas pesados**: La estética de la UI se apoya en el tema oscuro nativo pulido de Dear ImGui.

---

## 3. Requerimientos Funcionales (RF)

| ID | Requerimiento | Descripción | Criterio de Aceptación |
|---|---|---|---|
| **RF-01** | Modern OpenGL Context | Inicializar ventana GLFW con perfil Core OpenGL 3.3+ y cargar funciones con GLEW. | No se emplean llamadas obsoletas (`glBegin`, `glEnd`, matrix stack de GL 1.x). `glGetError()` retorna `GL_NO_ERROR`. |
| **RF-02** | Shaders y Modos Visuales | Renderizar 4 modos: 1) Barras con picos, 2) Radial, 3) Osciloscopio temporal, 4) Cascada. | Conmutación instantánea mediante atajo de teclado numérico (1, 2, 3, 4) o selector en ImGui. |
| **RF-03** | Peak-Hold Dinámico | En modo Barras, dibujar marcadores de pico que caen con gravedad física tras un retardo configurable. | Las marcas se mantienen en los máximos locales y descienden fluidamente a 60-144+ FPS. |
| **RF-04** | HUD de Configuración ImGui | Menú flotante interactivo con widgets para `attack_ms`, `release_ms`, `amplitude_factor`, `dynamic_range_db`, `frequency_scale`, colores y selector de modo. | Se activa/desactiva pulsando `H` o `Tab`. No bloquea los eventos de la ventana cuando está oculto. |
| **RF-05** | Enumeración WASAPI | Enumerate endpoints activos vía `IMMDeviceEnumerator::EnumAudioEndpoints`. | ImGui muestra un ComboBox con los nombres reales de los altavoces/auriculares. Al seleccionar uno, el hilo de captura se reinicia en el nuevo dispositivo. |
| **RF-06** | Guardado de Configuración | Serializar el estado actual a `config.json` con formato legible. | Los cambios sobreviven al cierre y reapertura de la aplicación. |
| **RF-07** | Build con CMake | Generar proyecto funcional con `cmake -B build` y compilar con Ninja o MSBuild. | Compila limpiamente en x64 sin requerir configuración manual de rutas absolutas. |

---

## 4. Requerimientos No Funcionales (RNF)

1. **Latencia Ultra-Baja**: La latencia total del pipeline de señal (captura WASAPI -> FFT -> Renderizado en pantalla) debe mantenerse entre 40 ms y 65 ms a 48 kHz / 144 Hz.
2. **Eficiencia de CPU/GPU**: El hilo de render no debe superar el 3-5% de uso de CPU en resoluciones 1080p/1440p, y el hilo de procesado FFT debe consumir < 1% gracias a FFTW single precision.
3. **Cero Caídas de Paquetes de Audio**: La sincronización de intercambio mediante `AudioData` y `VisualizerData` debe ser atómica y libre de contención excesiva de mutex.
4. **Tolerancia a Fallos y Reconexión**: Si el dispositivo de audio seleccionado se desconecta (ej. desenchufar auriculares USB), la aplicación debe volver automáticamente al dispositivo por defecto sin crashear.
