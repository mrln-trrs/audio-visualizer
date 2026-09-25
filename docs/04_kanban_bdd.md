# Plan de Trabajo Ágil, Kanban y Criterios BDD - Audio Visualizer 2.0

## 1. Definición de Hecho (Definition of Done - DoD)
Una tarea o historia de usuario se considera terminada únicamente si:
1. **Compilación Limpia**: Compila en Debug y Release en x64 sin advertencias ni errores (`/W3` o superior sin avisos).
2. **Criterios BDD Verificados**: Los escenarios *Given-When-Then* asociados se ejecutan y superan satisfactoriamente.
3. **Métricas de Rendimiento**: Mantiene la tasa de refresco nativa (144+ FPS o vsync) sin caídas de frames ni fugas de memoria.
4. **Documentación Sincronizada**: Código debidamente comentado y `README.md` actualizado si cambian comandos de compilación o configuraciones.

---

## 2. Definición de Preparado (Definition of Ready - DoR)
El sprint de desarrollo queda listo para comenzar cuando:
- [x] PRD Lean revisado y aprobado por el usuario.
- [x] Tech Spec con arquitectura de hilos, shaders e interfaces COM definida.
- [x] UX Flows y 7 estados del sistema especificados.
- [x] Historias de usuario BDD estructuradas con alcance acotado.

---

## 3. Épicas y Tablero Kanban BDD

### Épica 1: Build System Universal con CMake (Fase 1)
**Objetivo**: Permitir compilar el proyecto de forma independiente al IDE mediante CMake 3.20+, gestionando dependencias locales e integrando Dear ImGui.

#### Historia 1.1: Configuración de CMakeLists.txt con dependencias locales
- **Como**: Desarrollador C++.
- **Quiero**: Un archivo `CMakeLists.txt` que detecte GLFW, GLEW, FFTW y nlohmann/json desde las carpetas locales del repositorio.
- **Para**: Compilar el proyecto con cualquier generador (Visual Studio, Ninja, Clang) sin instalar dependencias globales.
- **Criterios BDD**:
  - **Scenario**: Generación y compilación con CMake
    - **Given** una copia limpia del repositorio en Windows
    - **When** se ejecuta `cmake -B build -S .` y `cmake --build build --config Release`
    - **Then** el proceso termina exitosamente con código 0 y genera el ejecutable `audio-visualizer.exe`
  - **Scenario**: Copia automática de binarios en post-build
    - **Given** la compilación finalizada del ejecutable
    - **When** se examina la carpeta de salida (`build/Release` o `build`)
    - **Then** las DLLs `libfftw3f-3.dll`, `glew32.dll`, `glfw3.dll` y `config.json` se encuentran presentes automáticamente.

#### Historia 1.2: Integración de Dear ImGui vía CMake
- **Como**: Desarrollador C++.
- **Quiero**: Descargar o compilar Dear ImGui (con backends GLFW y OpenGL3) como parte del proceso de CMake.
- **Para**: Disponer de la API de ImGui lista para ser consumida en el hilo de renderizado.
- **Criterios BDD**:
  - **Scenario**: Inclusión de cabeceras de ImGui
    - **Given** el proyecto configurado con CMake
    - **When** se incluye `#include <imgui.h>` y `#include <backends/imgui_impl_glfw.h>`
    - **Then** no hay errores de compilación ni símbolos indefinidos enlazando `imgui`.

---

### Épica 2: Modernización Gráfica OpenGL 3.3+ y Shaders (Fase 2)
**Objetivo**: Sustituir el renderizado inmediato OpenGL 1.1 por un pipeline de shaders GLSL modernos y renderizado híbrido.

#### Historia 2.1: Contexto Core Profile y Shader Loader
- **Como**: Usuario de la aplicación.
- **Quiero**: Que la ventana utilice OpenGL 3.3 Core Profile con shaders compilados en GPU.
- **Para**: Aprovechar la aceleración de hardware moderna y eliminar llamadas obsoletas.
- **Criterios BDD**:
  - **Scenario**: Inicialización exitosa de OpenGL 3.3
    - **Given** la aplicación arrancando en Windows
    - **When** se crea la ventana GLFW con `GLFW_CONTEXT_VERSION_MAJOR 3` y `MINOR 3`
    - **Then** `glGetString(GL_VERSION)` retorna `>= 3.3.0` y no hay llamadas a `glBegin`/`glEnd`.

#### Historia 2.2: Modo Barras con Peak-Hold
- **Como**: Usuario melómano o streamer.
- **Quiero**: Ver las barras de frecuencia con pequeños marcadores de pico que sostienen el valor máximo y caen suavemente con gravedad.
- **Para**: Apreciar la dinámica y los transitorios de la música con mayor precisión visual.
- **Criterios BDD**:
  - **Scenario**: Respuesta de las marcas de pico
    - **Given** un golpe de bombo o transitorio fuerte de volumen
    - **When** la barra sube instantáneamente y luego desciende
    - **Then** el marcador de pico permanece en la posición más alta durante `hold_time_ms` y desciende progresivamente sin desaparecer de golpe.

#### Historia 2.3: Modos Visuales Procedurales (Radial, Waveform, Waterfall)
- **Como**: Usuario.
- **Quiero**: Poder alternar entre modo Barras, Modo Radial (circular), Osciloscopio y Espectrograma Cascada pulsando teclas numéricas (1, 2, 3, 4).
- **Para**: Disfrutar de diferentes estilos estéticos adaptados al tipo de música.
- **Criterios BDD**:
  - **Scenario**: Cambio instantáneo de modo visual
    - **Given** la aplicación ejecutándose en modo Barras
    - **When** el usuario presiona la tecla `2`
    - **Then** la pantalla pasa inmediatamente a visualizar el espectro radial circular sin parpadeos ni tirones en el audio.

---

### Épica 3: Panel de Control Interactivo con Dear ImGui (Fase 3)
**Objetivo**: Proporcionar una interfaz flotante translúcida para modificar todos los parámetros de renderizado en vivo y guardarlos en `config.json`.

#### Historia 3.1: Overlay Flotante y Control de Visibilidad
- **Como**: Usuario.
- **Quiero**: Abrir y ocultar el panel de control pulsando la tecla `H` o `Tab`.
- **Para**: Configurar los parámetros cuando lo necesite y mantener la pantalla despejada durante la visualización.
- **Criterios BDD**:
  - **Scenario**: Alternancia de visibilidad
    - **Given** la aplicación en ejecución con el HUD oculto
    - **When** el usuario pulsa la tecla `H`
    - **Then** aparece el panel de control translúcido con los controles interactivos
    - **When** vuelve a pulsar `H`
    - **Then** el panel se oculta y el ratón deja de interactuar con la UI.

#### Historia 3.2: Ajuste en Tiempo Real y Persistencia
- **Como**: Usuario.
- **Quiero**: Mover los controles deslizantes de ataque, caída, ganancia y colores y ver el efecto inmediato, con la opción de guardar en `config.json`.
- **Para**: Calibrar visualmente la respuesta sin tener que editar archivos de texto a mano.
- **Criterios BDD**:
  - **Scenario**: Modificación y persistencia
    - **Given** el HUD abierto
    - **When** el usuario cambia `release_ms` a 250 y hace clic en "Guardar Configuración"
    - **Then** el archivo `config.json` se actualiza en disco con el nuevo valor y la animación responde inmediatamente a los 250 ms.

---

### Épica 4: Selector y Gestor de Dispositivos WASAPI (Fase 4)
**Objetivo**: Permitir seleccionar cualquier interfaz o salida de audio activa desde la interfaz gráfica sin reiniciar el proceso.

#### Historia 4.1: Enumeración de Endpoints WASAPI
- **Como**: Usuario con múltiples tarjetas de sonido, auriculares USB o altavoces.
- **Quiero**: Que el combo de dispositivos en ImGui liste todos mis dispositivos de audio con sus nombres amigables.
- **Para**: Seleccionar exactamente qué fuente de audio deseo monitorizar.
- **Criterios BDD**:
  - **Scenario**: Listado de dispositivos
    - **Given** un sistema con altavoces Realtek y auriculares USB conectados
    - **When** se despliega el ComboBox de audio en el HUD
    - **Then** ambos dispositivos aparecen con sus nombres correspondientes y el actual está seleccionado.

#### Historia 4.2: Conmutación Atómica de Captura en Caliente
- **Como**: Usuario.
- **Quiero**: Cambiar de dispositivo en el menú desplegable y que el audio comience a capturarse desde la nueva salida de inmediato.
- **Para**: No tener que reiniciar la aplicación cuando cambio de auriculares a altavoces.
- **Criterios BDD**:
  - **Scenario**: Cambio de endpoint
    - **Given** el visualizador capturando del dispositivo por defecto
    - **When** el usuario selecciona los auriculares USB en el ComboBox
    - **Then** el hilo de captura cierra el cliente anterior, conecta el nuevo endpoint, ajusta la frecuencia de muestreo y reanuda la visualización en menos de 500 ms sin bloquear la interfaz.
