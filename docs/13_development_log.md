# Bitácora de Desarrollo de la Versión 3.0: Pasos, Fallos, Resultados y Estado Actual

> **Estado:** Registro histórico y estado verificado a fecha 2026-09-25.  
> **Alcance:** Qué se hizo, en qué orden, qué falló, cómo se corrigió, qué se midió y qué queda sin verificar. Todo lo que aquí se afirma procede de los mensajes de commit del repositorio, de las salidas de compilación y de pruebas, y de capturas de pantalla tomadas durante la sesión. Donde algo no se midió, se dice.  
> **Documentos relacionados:** [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md), [12_fluent_design_ui.md](12_fluent_design_ui.md), [04_kanban_bdd.md](04_kanban_bdd.md), [06_build_and_toolchain.md](06_build_and_toolchain.md)  
> **Convención:** los errores se clasifican por origen (defecto heredado del código previo, error de implementación en esta sesión, error en una prueba o en un arnés de prueba, error en la documentación, limitación de herramienta). Los errores propios se registran con el mismo detalle que los ajenos.

## Índice

1. [Método](#1-método)
2. [Punto de partida](#2-punto-de-partida)
3. [Cronología por commit](#3-cronología-por-commit)
4. [Errores y fallos consolidados](#4-errores-y-fallos-consolidados)
5. [Resultados medidos](#5-resultados-medidos)
6. [Estado actual](#6-estado-actual)
7. [Deuda técnica y riesgos](#7-deuda-técnica-y-riesgos)
8. [Cómo reproducir las verificaciones](#8-cómo-reproducir-las-verificaciones)

## 1. Método

Toda la sesión se desarrolló sobre `master` con commits atómicos por fase, cada uno compilado en Debug y Release con MSBuild (solución de Visual Studio 2026) y en Release con CMake, con la política de cero avisos. Las funciones con criterios numéricos tienen una prueba ejecutable sin audio ni ventana. El comportamiento en ejecución se verificó con un arnés de PowerShell que arranca el ejecutable, reproduce un sonido del sistema, envía teclas, captura la ventana con `PrintWindow` y mide la CPU por hilo. Las cifras de esta bitácora son las de la máquina de referencia: Windows 11 build 26200, monitor de 144 Hz, salida de audio a 48 kHz en float32 estéreo, driver gráfico que ignora `glfwSwapInterval(1)`.

Los scripts del arnés están en `tools/` para que las mediciones sean reproducibles (sección 8).

## 2. Punto de partida

Estado del repositorio en el commit `2865065` (21 de agosto de 2025), antes de la sesión:

- Diez archivos planos en la raíz. Un solo vector de alturas como salida del análisis.
- El proyecto pedía el toolset v143 (Visual Studio 2022), no instalado; el aviso "The build tools for v143 cannot be found" impedía compilar.
- Rutas de las bibliotecas externas absolutas a `C:\proyects\audio-visualizer\audio-visualizer\`, carpeta inexistente en la máquina. Las bibliotecas no estaban en el repositorio ni en el disco.
- Defectos funcionales que se diagnosticaron después (sección 3, commit `7d25bce`): estéreo leído como mono, frecuencia de muestreo fija en 44100, sin solapamiento, sin ventana, lectura del búfer equivocado, congelación en silencio, suavizado dependiente de los fps.
- Existía un archivo `signal-processor.cpp` muerto que redefinía `FFT_SIZE` y las estructuras compartidas.

## 3. Cronología por commit

Cada entrada indica objetivo, qué se hizo, qué falló y cómo se verificó.

### `c4f746e` Retarget a v145 e inclusión de bibliotecas

- **Objetivo.** Que el proyecto compile en la máquina.
- **Hecho.** `PlatformToolset` de v143 a v145 en las cuatro configuraciones. Descarga de GLFW 3.4, GLEW 2.1.0, FFTW 3.3.5 y nlohmann/json 3.12.0 al repositorio (por decisión del usuario, para no volver a buscarlas). Generación de los `.lib` de importación de FFTW con `lib.exe /def`, porque el paquete oficial no los trae. Rutas del proyecto relativas a `$(ProjectDir)`. Post-build que copia las DLL a la salida.
- **Fallos.** Ninguno propio. El primer intento de compilar tras el retarget falló por las bibliotecas ausentes, que era el problema a resolver.
- **Verificación.** Compilación Debug y Release; el ejecutable arranca y abre la ventana.

### `63b4311` Saneado para Visual Studio 2026

- **Hecho.** Eliminación de las configuraciones Win32, que no podían compilar con bibliotecas de 64 bits. GLFW enlazado como DLL para eliminar el aviso LNK4098 (CRT de Release en `glfw3.lib` estática). Copia de `config.json` y `glfw3.dll` en el post-build. C++17 unificado. Nueve avisos C4244 en `renderer.cpp` corregidos con conversiones explícitas. `.vcxproj.user` con el directorio de trabajo del depurador.
- **Verificación.** Cero avisos en ambas configuraciones; el ejecutable encuentra `config.json` y sus DLL desde la carpeta de salida.

### `7d25bce` Pipeline de audio en tiempo real

- **Diagnóstico.** Once defectos, documentados en el mensaje de commit y en el documento 05. Los de mayor efecto: el espectro se recalculaba de 12 a 23 veces por segundo (búfer de 4096 sin solapamiento), el suavizado por cuadro añadía de 150 a 300 ms, el estéreo se leía como mono, la frecuencia de muestreo era 44100 fija cuando el dispositivo va a 48000.
- **Hecho.** Anillo circular, ventana deslizante de 2048 con salto de 256, ventana de Hann, mezcla a mono con el formato real del dispositivo, publicación por intercambio bajo mutex con contador de generación, ataque y caída en milisegundos con el tiempo real del cuadro, `condition_variable` con predicado, `timeBeginPeriod(1)`, prioridad MMCSS, reintento ante dispositivo invalidado.
- **Hallazgo inesperado.** Al medir, el bucle de render corría a unos 7000 fps: el driver gráfico ignora `glfwSwapInterval(1)`. El código original nunca sincronizó con el monitor. Se añadió un limitador adaptativo que se activa al detectar fps muy por encima del refresco y clava el bucle a la tasa del monitor.
- **Fallos propios.** Un heredoc de Bash falló por el escape de `<<` dentro de código C++; se reescribió con la herramienta de escritura de archivos. `NOMINMAX` y `windows.h` antes que GLFW para evitar la redefinición de `APIENTRY` y el conflicto con `std::max`.
- **Verificación.** 144 fps clavados al monitor, 100 espectros por segundo (límite del periodo de 10 ms del motor de audio), 48000 Hz reales, captura con audio real mostrando picos armónicos.

### `915ede1` y `a7a3c9d` Documentación y recorte de dependencias

- **Hecho.** README con arquitectura, pipeline, sincronización, latencia, configuración y dependencias. Recorte de `json-develop` de 18 MB y 1447 archivos a 1,2 MB y 48, porque GitHub avisó de dos vulnerabilidades en el `requirements.txt` de la documentación Python de nlohmann/json (`mkdocs-material` y `wheel`). Ambas alertas figuran como resueltas tras el recorte.
- **Fallo propio.** Un `CppProperties.json` generado por Visual Studio en modo "Abrir carpeta" se coló en el commit `915ede1`; se retiró y se añadió al `.gitignore` en `a7a3c9d`.
- **Pendiente externo.** La rama remota `Marlon`, con un único commit superado, no se pudo borrar desde esta sesión por política de permisos; el usuario puede hacerlo con `git push origin --delete Marlon`.

### `4f4c6f7` Audio Visualizer 2.0

- **Punto de partida.** El usuario aportó un borrador extenso de la 2.0 (OpenGL 3.3 Core, cuatro modos con shaders, Dear ImGui, selector de dispositivos WASAPI, CMake) con nueve documentos en `docs/`, y ninguna de las dos rutas de compilación funcionaba.
- **Causas.** El `.vcxproj` no tenía las fuentes ni la ruta de ImGui (`imgui.h` no encontrado). El `CMakeLists.txt` usaba `FetchContent` para clonar ImGui en cada configuración y el clon fallaba de forma intermitente ("remote did not send all necessary objects"), además de contradecir el principio de clonar y compilar sin red. Faltaban `<atomic>` en `config.h` y `<cstddef>` en `renderer.cpp`.
- **Hecho.** Dear ImGui 1.91.5 vendorizado en `imgui-1.91.5/` (2,8 MB). CMake con rutas locales y comprobaciones. `.vcxproj` con ImGui, copia de `shaders/` y filtros. Arranque con el dispositivo guardado en `selected_device_name`, que estaba en la configuración pero nadie leía. Textura del espectro subida cada cuadro para que el modo radial se mueva a la tasa del monitor.
- **Fallos del arnés de prueba.** La primera captura de pantalla devolvió el contenido del navegador del usuario, porque `CopyFromScreen` copia la región de pantalla y la ventana estaba tapada; se pasó a `PrintWindow` con `PW_RENDERFULLCONTENT`, que captura el contenido de la ventana aunque esté oculta.
- **Verificación.** Cero avisos en MSBuild y CMake a `/W4`; los cuatro modos y el HUD capturados; 144 fps; cierre en 108 ms.

### `c693cc8` Documentación de la 3.0

- **Hecho.** Documentos 10 (teoría con demostraciones), 11 (arquitectura de la trama de análisis) y 12 (Fluent), y formalización de los nueve existentes con cabecera de estado, alcance y documentos relacionados. Eliminación de emojis y de enlaces `file:///` a la máquina del autor.
- **Fallo propio.** Un comando de edición demasiado largo superó el límite de la consola (`ENAMETOOLONG`); se movió a un script.

### `f2a032f` Reestructuración en capas

- **Hecho.** De 10 archivos planos a 53 en `src/` en seis capas con dependencias en un solo sentido hacia `core`. Interfaz `IVisualMode` con una clase por modo. Listas de fuentes del `.vcxproj`, filtros y CMake regeneradas por script a partir del árbol. Ventana de Hann periódica (denominador $N$) en lugar de la simétrica, condición necesaria para la reconstrucción exacta posterior.
- **Fallos propios.** `COINIT_MULTITHREADED` y `CoInitializeEx` no declarados: `WIN32_LEAN_AND_MEAN` excluye COM de `windows.h`; se añadió `<objbase.h>`. `MODE_COUNT` no declarado en `config.cpp`: faltaba incluir `core/types.h`.
- **Verificación.** Cero avisos; prueba de humo con los cuatro modos, HUD, 144 fps y cierre en 130 ms.

### `302ea88` Fase A: trama de análisis y triple búfer

- **Hecho.** `AnalysisFrame` (magnitud, dB, fase, RMS, pico, flujo espectral, centroide, mezcla), `TripleBuffer` sin mutex ni copias, mapeo de bins a barras trasladado al render (`BarSpectrum`), telemetría de la trama en el HUD.
- **Medición.** CPU por hilo durante 10 s: análisis 0,5 %, captura 0,6 %, render 26 % de un núcleo. El 26 % era la espera activa del limitador, que giraba los últimos 2 ms de cada periodo de 6,9 ms.
- **Verificación.** Capturas de barras y radial equivalentes a las de la 2.0; 100 tramas por segundo; criterio de la fase A cumplido (análisis por debajo del 1 %).

### `6320f82` Fase B: bandas, métricas, dinámica por banda y medidores

- **Hecho.** `BandConfig` con cuatro particiones y validación, `BandLayout` contiguo sin solapes, métricas por banda en la trama, `SpectrumDynamics` con constantes por elemento, herencia de dinámica por barra, modo de medidores con etiquetas por `DrawOverlay`, pestaña "Bandas", prueba `band_metrics_test`. Limitador con temporizador de alta resolución de Windows en lugar de espera activa.
- **Fallo en la prueba, primera ejecución.** La comprobación "banda Sub al menos 40 dB por debajo" falló con -37,9 dB. No era un defecto del código: el bin más alto de "Sub" (46,9 Hz) está a 2,27 bins del tono de 100 Hz, dentro del lóbulo principal de Hann. El criterio se corrigió a -31 dB para bandas contiguas (primer lóbulo secundario de Hann) y se mantuvo -46 dB para las demás.
- **Error de documentación detectado por la prueba.** El criterio de 0,5 dB para el pico de la banda "Bajo" era físicamente incorrecto: el tono a 0,27 bins del centro sufre 0,4 dB de pérdida de festoneado, hasta 1,42 dB en el peor caso. Se corrigió en los documentos 04 y 11 y se añadió la confirmación empírica al documento 10.
- **Fallo del arnés.** Tras pulsar la tecla 6, las teclas 2 y 4 parecían no funcionar. GLFW decide pulsación o liberación por el bit `KF_UP` de `lParam`, no por el tipo de mensaje; el arnés enviaba liberaciones sin ese bit, así que la tecla quedaba pulsada y pisaba a las demás cada cuadro. No era un defecto de la aplicación. Corregido el arnés, los cinco modos conmutan.
- **Falsa alarma.** Una lectura de 112 fps con el HUD abierto coincidió con la ventana maximizada a 1920 por 1025 por el usuario durante la prueba; con la ventana en 1024 por 600 el HUD mantiene 144 fps. No se investigó más.
- **Fallo transitorio de compilación.** Un `LNK1104: cannot open file audio-visualizer.exe` en Debug, sin ninguna instancia en ejecución; desapareció al reintentar. Se atribuye al análisis del antivirus sobre el ejecutable recién creado. Está en la tabla de solución de problemas del documento 07.
- **Medición.** Render del 26 % al 16 % de un núcleo con el temporizador de alta resolución; análisis 0,3 %; captura 0,2 %.
- **Resultados de la prueba.** Pico de "Bajo" -6,40 dB (criterio 1,42), 99,95 % de la energía en "Bajo", fuga contigua -37,9 dB (criterio -31), demás bandas de -68 a -156 dB, Parseval por bandas con error relativo menor que $3 \cdot 10^{-8}$ en las cuatro particiones.

### `868ea43` Fase C: ondas por banda y osciloscopio apilado

- **Hecho.** `BandSynthesizer` (máscaras de coseno alzado complementarias, banda "resto", IFFT `c2r` por fila, solapamiento y suma normalizado), `BandWaveformTexture`, modo apilado con reducción por mínimo y máximo, cálculo bajo demanda con `NeedsBandWaveforms` y máximo de 32 bandas, prueba `band_synthesis_test`.
- **Resultados de la prueba.** Suma de máscaras igual a 1 con error cero; suma de las ondas de banda igual a la mezcla con error máximo $2{,}6 \cdot 10^{-7}$ sobre 44 032 muestras (criterio $10^{-5}$); RMS por banda dentro del 3 % de cada tono; fila resto 0,012.
- **Medición.** Análisis del 0,3 % al 2,2 % con el modo apilado activo (ocho IFFT de 2048 por trama); 144 fps.
- **Observación.** Una lectura de 117 fps en el segundo en que se reactivó el modo, atribuida a la recreación de la textura de 4096 por 9 y al arranque del solapamiento. No medida con más detalle.

### `440d2da` Diseño Fluent

- **Hecho.** Escena a framebuffer, cadena de desenfoque separable a un cuarto de resolución, material acrílico propio desde un callback de ImGui con sombra y esquinas, fundido entre modos de 150 ms y apertura del panel de 200 ms con curvas de deceleración, Mica y Acrílico del sistema por DWM con degradación, escala por DPI inicial, lectura de las preferencias de transparencia y animaciones de Windows, pestaña "Color y Apariencia", prueba `motion_test`.
- **Fallos propios de implementación.** Faltaba la declaración adelantada `struct ImDrawCmd;` en `panel_material.h`. El material se dibujaba en la lista de la propia ventana de ImGui y tapaba la barra de título y su texto; se pasó a la lista de fondo, que se dibuja antes que cualquier ventana. Un primer diseño del callback usaba un puntero global; se sustituyó por un puntero al propietario dentro de cada entrada.
- **Fallos propios en la prueba.** `motion_test` declaraba "terminada" la transición al superar 0,999, valor que la curva cúbica alcanza unos 10 ms antes del final por construcción; el criterio correcto es valor exactamente 1, que solo ocurre al cumplirse la duración. Además, a 60 fps ningún cuadro cae en 140 ms; la comprobación de "no terminada a los 140 ms" pasó a evaluar la función de valor en ese instante exacto.
- **Limitación de herramienta.** No fue posible capturar un cuadro intermedio del fundido entre modos: `PrintWindow` tarda más que la transición y la medida de brillo por `GetPixel` resultó demasiado lenta e inconcluyente. La duración queda garantizada por la prueba unitaria; la composición visual del fundido está ejercitada en cada cambio de modo sin errores, pero no hay imagen del cuadro intermedio.
- **Verificación.** Captura del panel con material sobre el modo radial, con la barra de título legible; 144 fps con el panel abierto; render entre el 13 % y el 20 %; preferencias del sistema en la máquina de referencia: animaciones y transparencias activadas.

### `1063445` Fase D: resolución variable

- **Hecho.** `MultiResolution` (FFT larga de 8192 en graves y corta de 512 en agudos, misma normalización), sección `analisis` de la configuración, anillo de captura a 16384, selección de fuente por barra en `BarSpectrum`, controles con la latencia calculada en el HUD, prueba `resolution_test`. Desactivada por defecto.
- **Error de documentación detectado al diseñar la prueba.** El criterio original de la fase D pedía distinguir 40 de 42,5 Hz. Un semitono a 40 Hz son 2,5 Hz y exige una ventana de un segundo según la propia tabla 5.3 del documento 10; con 8192 muestras (5,86 Hz por bin) es imposible. El criterio pasó a 40 y 55 Hz, y la prueba conserva el caso de 40 y 42,5 Hz para demostrar que no se separa.
- **Resultados de la prueba.** Dos picos con 8192 y uno con 2048 para 40 y 55 Hz; un solo pico con 8192 para 40 y 42,5 Hz; latencia añadida 64 ms y ahorrada 16 ms; picos coherentes entre resoluciones con diferencia menor que $10^{-3}$ dB.
- **Medición.** Análisis al 1,2 % con la opción activa; 144 fps.

## 4. Errores y fallos consolidados

| Origen | Descripción | Cómo se detectó | Corrección | Commit |
|---|---|---|---|---|
| Heredado | Toolset v143 no instalado | Aviso de MSBuild | Retarget a v145 | `c4f746e` |
| Heredado | Rutas absolutas a una carpeta inexistente; bibliotecas ausentes | Error C1083 | Bibliotecas en el repositorio, rutas `$(ProjectDir)` | `c4f746e` |
| Heredado | Configuraciones Win32 imposibles de compilar | Revisión del `.sln` | Eliminadas | `63b4311` |
| Heredado | LNK4098 por CRT de la GLFW estática | Salida del enlazador | GLFW como DLL | `63b4311` |
| Heredado | Nueve avisos C4244 | Salida del compilador | Conversiones explícitas | `63b4311` |
| Heredado | Estéreo leído como mono, 44100 fijo, sin solapamiento, sin ventana, búfer equivocado, congelación en silencio, suavizado por cuadro, `cv.wait` sin predicado, `sleep_for` sin `timeBeginPeriod`, código muerto | Lectura del código y mediciones | Pipeline nuevo | `7d25bce` |
| Hallazgo | Driver que ignora vsync: 7000 fps | Contador de fps en el título | Limitador adaptativo | `7d25bce` |
| Externo | Dos alertas de Dependabot en la documentación Python de nlohmann/json | Aviso de GitHub al hacer push | Recorte de `json-develop` a `include/` | `915ede1` |
| Propio | `CppProperties.json` de Visual Studio colado en un commit | Revisión del `git status` tras el commit | Retirado e ignorado | `a7a3c9d` |
| Aportado | `FetchContent` de ImGui fallaba y exigía red; `.vcxproj` sin ImGui; cabeceras ausentes | Ambas compilaciones fallaban | ImGui vendorizado; proyecto y CMake corregidos | `4f4c6f7` |
| Arnés | Captura de pantalla con el contenido de otra ventana | Captura mostrando un navegador | `PrintWindow` con `PW_RENDERFULLCONTENT` | `4f4c6f7` |
| Propio | COM excluido por `WIN32_LEAN_AND_MEAN`; include ausente de `types.h` | Errores C2065, C3861 | `<objbase.h>`; include | `f2a032f` |
| Propio | Espera activa del limitador: 26 % de un núcleo | Medición de CPU por hilo | Temporizador de alta resolución | `6320f82` |
| Documentación | Criterio de 0,5 dB sin contar el festoneado de Hann | Fallo de la prueba | Criterio 1,42 dB; sección 5.4 del documento 10 | `6320f82` |
| Prueba | Umbral de fuga de 40 dB para una banda dentro del lóbulo principal | Fallo de la prueba | Umbral -31 dB para contiguas | `6320f82` |
| Arnés | Liberaciones de tecla sin el bit `KF_UP` | Modos que no cambiaban tras la tecla 6 | `lParam` correcto | `6320f82` |
| Transitorio | LNK1104 sobre el ejecutable sin instancia en ejecución | Fallo de enlace | Reintento; documentado | `6320f82` |
| Propio | Declaración adelantada de `ImDrawCmd` ausente | Errores C4430, C2143 | Añadida | `440d2da` |
| Propio | Material dibujado sobre la barra de título | Captura de pantalla | Lista de fondo de ImGui | `440d2da` |
| Prueba | "Terminada" al superar 0,999; muestra a 140 ms inexistente a 60 fps | Fallo de la prueba | Valor exactamente 1; evaluación en el instante exacto | `440d2da` |
| Herramienta | Imposible capturar un cuadro intermedio del fundido | Medición inconclusa | Prueba unitaria de la duración; anotado como no verificado visualmente | `440d2da` |
| Documentación | Criterio de 40 y 42,5 Hz inconsistente con la teoría | Diseño de la prueba | Criterio 40 y 55 Hz; caso del semitono conservado como negativo | `1063445` |

## 5. Resultados medidos

Máquina de referencia, ventana de 1024 por 600 salvo indicación.

| Magnitud | Antes de la sesión | Estado final |
|---|---|---|
| Espectros por segundo | 12 a 23 | 100 (límite del periodo de 10 ms del motor de audio) |
| Cuadros por segundo | 7000 sin control, o el que diera el driver | 144, clavados al monitor por el limitador |
| Frecuencia de muestreo usada | 44100 fija | 48000 real del dispositivo |
| Retraso añadido por el suavizado | 150 a 300 ms | ataque 12 ms, caída 160 ms por defecto, independientes de los fps |
| Hilo de análisis | no medido | 0,3 % de un núcleo; 2,2 % con ondas por banda; 1,2 % con resolución variable |
| Hilo de captura | no medido | 0,2 % a 0,9 % |
| Hilo de render | no medido | 26 % con espera activa; 16 % con temporizador; 13 % a 21 % con HUD, material o modo apilado |
| Cierre por la X | no medido | 108 a 130 ms, código 0 |
| Avisos de compilación | 9 (C4244) más LNK4098 | 0 en MSBuild Debug y Release y en CMake a `/W4` |
| Pruebas numéricas | ninguna | 4 objetivos, 40 comprobaciones, todas superadas |
| Tamaño de `json-develop` | 18 MB, 1447 archivos | 1,2 MB, 48 archivos |
| Alertas de seguridad de GitHub | 2 | 0 |

Resumen de las pruebas numéricas:

| Prueba | Comprobaciones | Resultado clave |
|---|---|---|
| `band_metrics_test` | 14 | Parseval por bandas con error relativo menor que $3 \cdot 10^{-8}$ en cuatro particiones |
| `band_synthesis_test` | 6 | Suma de ondas de banda igual a la mezcla con error máximo $2{,}6 \cdot 10^{-7}$ |
| `motion_test` | 12 | Transición de 150 ms completa exactamente a los 150 ms a 60, 144 y 240 fps |
| `resolution_test` | 8 | 40 y 55 Hz separados con 8192 y fundidos con 2048; 40 y 42,5 Hz no separados |

## 6. Estado actual

### Implementado y verificado

- Pipeline de captura y análisis en tiempo real con trama de análisis completa y triple búfer.
- Seis modos visuales: barras con peak-hold, radial, osciloscopio, cascada, medidores por banda, osciloscopio apilado.
- Bandas configurables en cuatro particiones, con nombre, color, ataque, caída y ganancia; métricas por banda; herencia de dinámica por barra.
- Ondas por banda por IFFT enmascarada con suma exacta demostrada numéricamente.
- Resolución variable opcional con la latencia expuesta en la interfaz.
- Panel de control con material acrílico propio, transiciones por tiempo real y respeto a las preferencias de accesibilidad de Windows.
- Marco oscuro y esquinas redondeadas por DWM; Mica y Acrílico del sistema aceptados por DWM en la máquina de referencia.
- Dos rutas de compilación equivalentes y cuatro pruebas numéricas.

### Implementado pero no verificado en la máquina de referencia

- Degradación de Mica y Acrílico en Windows 10 (implementada por detección de build y resultado de la llamada; no ejecutada en Windows 10).
- Conversión de formatos PCM de 16, 24 y 32 bits en la captura (el dispositivo de referencia entrega float32; en modo compartido Windows siempre lo hace).
- Reintento tras invalidación del dispositivo de audio (código presente; no se provocó la desconexión).
- Cuadro intermedio del fundido entre modos (duración garantizada por prueba unitaria; sin captura).

### No implementado

- Recarga del atlas de fuentes al mover la ventana entre monitores con distinto DPI (solo escala inicial).
- Medición automatizada del contraste texto-fondo sobre la escena más brillante.
- Luz de foco (reveal) y filtro dual de Kawase, del documento 12.
- Regla logarítmica arrastrable para los cortes de banda (los cortes se editan con controles numéricos acotados).
- Todo lo del documento 09 que no absorbieron los documentos 10 a 12: iconos en ImGui, CI/CD, multiplataforma, Bloom, presets múltiples de bandas.

### Límites físicos, no defectos

- 100 espectros por segundo es el techo del loopback de WASAPI en modo compartido (periodo de 10 ms del motor).
- La resolución en frecuencia y la latencia están ligadas por el principio de incertidumbre; ninguna opción del programa lo evita, y la interfaz muestra el coste de cada elección.
- Separar por frecuencia no separa instrumentos.

## 7. Deuda técnica y riesgos

- **Licencia.** FFTW es GPL. Distribuir el ejecutable obliga a distribuirlo bajo GPL o a adquirir la licencia comercial. Las demás bibliotecas son zlib, MIT o BSD.
- **Bibliotecas en el repositorio.** Unos 38 MB de binarios versionados por decisión del usuario. Ventaja: clonar y compilar. Coste: tamaño del repositorio y actualización manual.
- **`audio-visualizer.vcxproj.filters` está rastreado** aunque `.gitignore` lo excluye (estaba rastreado antes de la regla). Es inocuo; puede dejarse de rastrear con `git rm --cached`.
- **Listas de fuentes generadas por script.** El `.vcxproj`, sus filtros y `CMakeLists.txt` se regeneran con `tools/update_build_lists.py`. Añadir un archivo a `src/` sin ejecutarlo deja el build desincronizado. El documento 08 lo indica.
- **Validación de la configuración de bandas en cada cuadro.** Mientras el HUD está abierto, publica la configuración cada cuadro y tanto el render como el análisis la revalidan; el coste medido es despreciable, pero es trabajo redundante.
- **Modo per_bin.** Con 853 bandas el HUD no muestra la tabla por banda y el osciloscopio apilado no reconstruye ondas, por diseño; la experiencia en ese modo es limitada.
- **Solo Windows.** Captura WASAPI, efectos DWM y limitador con temporizador de Windows. La abstracción multiplataforma está fuera de alcance por decisión del documento 01.

## 8. Cómo reproducir las verificaciones

Compilación y pruebas numéricas:

```powershell
cmake -B build -S . -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Prueba de humo con capturas y CPU por hilo (requiere que el ejecutable esté compilado en `x64\Release`, reproduce un sonido del sistema durante unos segundos):

```powershell
pwsh -File tools\smoke_test.ps1 -ExeDir .\x64\Release
pwsh -File tools\cpu_per_thread.ps1 -ExeDir .\x64\Release -Seconds 10
```

Regeneración de las listas de fuentes tras añadir o quitar archivos en `src/`:

```powershell
python tools\update_build_lists.py
```

Las capturas se guardan junto al script. Las cifras de CPU dependen de la máquina; las de esta bitácora son de la de referencia descrita en la sección 1.
