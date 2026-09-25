# PRD Lean - Audio Visualizer 2.0

> **Estado:** Mixto. Las secciones 1 a 4 describen la versión 2.0, implementada y verificada. La sección 5 es la hoja de ruta propuesta para la 3.0.  
> **Alcance:** Visión de producto, alcance, requerimientos funcionales y no funcionales.  
> **Documentos relacionados:** [02_tech_spec.md](02_tech_spec.md), [04_kanban_bdd.md](04_kanban_bdd.md), [10_signal_decomposition_theory.md](10_signal_decomposition_theory.md), [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md), [12_fluent_design_ui.md](12_fluent_design_ui.md)  
> **Convención:** este documento distingue entre lo *implementado* (verificable en el código de `master`) y lo *propuesto* (diseño para la versión 3.0). Toda cifra cuantitativa se deriva o se referencia; no hay estimaciones sin base.

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
- **Separación de fuentes (voz, batería, bajo)**: Separar por frecuencia no es separar por instrumento. La separación de fuentes exige redes neuronales con latencias de segundos, incompatibles con 144 fps. Justificación en [10_signal_decomposition_theory.md, sección 10](10_signal_decomposition_theory.md#10-lo-que-esta-técnica-no-hace).
- **Reconstrucción de una onda por cada bin de la FFT**: Multiplica por mil el volumen de datos sin añadir información (teorema de conservación, documento 10, sección 6.2). Las ondas se reconstruyen solo por banda visible.
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

---

## 5. Hoja de Ruta Propuesta: Versión 3.0

Estado: propuesta. Ninguno de estos requerimientos está implementado. Su viabilidad se demuestra en el documento 10; su diseño, en los documentos 11 y 12.

### 5.1 Motivación

La versión 2.0 reduce el análisis a un vector de alturas de barra y descarta la fase, la magnitud lineal y toda estructura por bandas. La 3.0 recupera la información completa que la transformada de Fourier extrae del audio y la expone como datos que cualquier renderizador puede consumir. Sobre esa base, el usuario decide en cuántas bandas divide el sonido, cómo responde cada una y cómo se visualiza. En paralelo, la interfaz adopta el lenguaje visual de Windows 11.

### 5.2 Requerimientos funcionales adicionales

| ID | Requerimiento | Descripción | Criterio de Aceptación |
|---|---|---|---|
| **RF-08** | Trama de análisis | El hilo de procesado publica magnitud, fase, dB, energía por banda, RMS, pico y flujo espectral en una estructura única (`AnalysisFrame`). | Los cuatro modos de la 2.0 producen capturas equivalentes leyendo de la trama, con el procesado por debajo del 1 % de CPU. |
| **RF-09** | Bandas configurables | Partición del espectro en octavas, lineal, manual o por bin, con nombres, colores y validación. | Con una senoidal de 100 Hz a -6 dBFS solo la banda "Bajo" muestra energía, con error inferior a 0,5 dB; la suma de energías por banda iguala la total con error relativo menor que $10^{-4}$. |
| **RF-10** | Ondas por banda | Reconstrucción de la onda de cada banda por IFFT enmascarada con solapamiento y máscaras de coseno alzado. | La suma numérica de las $K$ ondas coincide con la mezcla con error máximo inferior a $10^{-5}$ en escala completa. |
| **RF-11** | Osciloscopio apilado | Modo con $K$ trazas, una por banda, y la traza de mezcla debajo, con reducción por mínimo y máximo. | Un golpe de bombo aparece en la traza grave y en la de mezcla en el mismo cuadro. |
| **RF-12** | Dinámica por banda | Ataque y caída editables por banda, aplicados a energía, pico y ganancia de onda. | Cambiar el ataque de la banda grave no altera la respuesta de las demás. |
| **RF-13** | Material propio (acrílico) | El panel de control desenfoca la visualización que tiene detrás, con tinte, exclusión y ruido. | El fondo desenfocado se desplaza con el panel sin discontinuidad; el contraste texto-fondo es al menos 4,5:1 sobre la escena más brillante. |
| **RF-14** | Materiales del sistema | Mica o Acrílico de Windows 11 opcionales, esquinas redondeadas, marco oscuro. | En Windows 11 22H2 se ve el contenido de otras ventanas desenfocado detrás; en Windows 10 la aplicación arranca opaca sin error. |
| **RF-15** | Movimiento | Transiciones con curvas de aceleración de Fluent, de 150 a 300 ms, independientes de los fps. | El cambio de modo dura 150 ms con más o menos 10 ms medido a 144 fps. |

### 5.3 Requerimientos no funcionales adicionales

5. **Conservación de la latencia**: Con `fft_size` 2048 la latencia total no debe aumentar respecto a la 2.0. Si se activa la resolución variable para graves, la interfaz muestra el retardo añadido por banda (documento 10, sección 7.3).
6. **Presupuesto de análisis**: Procesado por debajo del 3 % de un núcleo con 32 bandas reconstruidas (documento 10, sección 6.4).
7. **Presupuesto gráfico**: El post-procesado de materiales añade menos de 0,3 ms por cuadro a 1920 por 1080 en GPU integrada (documento 12, sección 12).
8. **Accesibilidad**: Contraste mínimo 4,5:1 (WCAG 2.1, criterio 1.4.3) y respeto a la preferencia del sistema de desactivar transparencias y animaciones.
9. **Honestidad física**: La interfaz no promete resolución que el principio de incertidumbre prohíbe. Todo parámetro que afecte a la resolución en frecuencia muestra su latencia asociada.

### 5.4 Métricas de éxito de la 3.0

- Un usuario configura siete bandas desde el HUD y ve sus siete ondas apiladas en menos de un minuto sin editar archivos.
- El error de reconstrucción medido cumple RF-10 en Debug y Release.
- Los fps se mantienen en el refresco del monitor con el HUD abierto y el material activo.
