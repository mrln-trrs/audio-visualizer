# Guía de Compilación, Toolchain y Despliegue - Audio Visualizer 2.0

> **Estado:** Implementado. Describe las dos rutas de compilación, ambas verificadas con cero avisos, las cuatro pruebas numéricas y los requisitos de la 3.0 (ya en uso).  
> **Alcance:** Requisitos, compilación con CMake y con la solución de Visual Studio, post-build, empaquetado y resolución de errores.  
> **Documentos relacionados:** [08_contributing_and_standards.md](08_contributing_and_standards.md), [12_fluent_design_ui.md](12_fluent_design_ui.md)  
> **Convención:** este documento distingue entre lo *implementado* (verificable en el código de `master`) y lo *propuesto* (diseño para la versión 3.0). Toda cifra cuantitativa se deriva o se referencia; no hay estimaciones sin base.

Esta guía explica en detalle cómo configurar el entorno, compilar el proyecto en diferentes editores y herramientas (CMake CLI, Visual Studio, VS Code, Ninja), y cómo empaquetar o distribuir el ejecutable resultante.

---

## 1. Requisitos del Sistema

- **Sistema Operativo**: Windows 10 versión 1903+ o Windows 11 (64 bits).
- **Compilador C++**: Microsoft Visual C++ (MSVC) v143 (VS 2022) o v145 (VS 2026), o Clang/LLVM con soporte C++17.
- **SDK de Windows**: Windows 10 SDK (10.0.19041.0 o superior, resolviendo a `10.0.26100.0+`).
- **CMake**: Versión 3.20 o superior (recomendado 3.28+).
- **GPU y Drivers**: Tarjeta gráfica compatible con **OpenGL 3.3 Core Profile** con drivers actualizados de Intel, NVIDIA o AMD.

> [!NOTE]
> No es necesario instalar bibliotecas externas previamente (ni usar `vcpkg` ni `conan`). Todas las dependencias (`GLFW`, `GLEW`, `FFTW3`, `nlohmann/json` y `Dear ImGui`) están versionadas en el repositorio para x64. La configuración de CMake no necesita conexión de red.

---

## 2. Compilación mediante Línea de Comandos (CMake)

### 2.1 Con el generador por defecto de Visual Studio
Abre PowerShell o CMD y ejecuta:

```powershell
# 1. Configurar y generar la solución de CMake en la carpeta 'build'
cmake -B build -S . -A x64

# 2. Compilar en configuración Release
cmake --build build --config Release

# 3. Ejecutar la aplicación compilada
.\build\Release\audio-visualizer.exe
```

Para compilar en modo **Debug** (útil para depurar con símbolos completos):
```powershell
cmake --build build --config Debug
.\build\Debug\audio-visualizer.exe
```

### 2.2 Con Ninja (Compilación Ultra-Rápida)
Si tienes el Developer Command Prompt de Visual Studio activo y `ninja` en el PATH:

```powershell
cmake -B build-ninja -S . -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build-ninja
.\build-ninja\audio-visualizer.exe
```

---

## 3. Compilación desde Visual Studio (IDE)

### Método A: Abrir como Proyecto CMake ("Abrir Carpeta")
1. Abre Visual Studio 2022 o 2026.
2. Selecciona **Archivo -> Abrir -> Carpeta...** y elige `D:\proyectos\audio-visualizer`.
3. Visual Studio detectará automáticamente `CMakeLists.txt` y generará la caché de CMake.
4. En la barra superior, asegúrate de seleccionar `x64-Release` (o `x64-Debug`).
5. Pulsa **F5** o haz clic en el botón de reproducción verde (`audio-visualizer.exe`).

### Método B: Abrir la Solución Tradicional (`audio-visualizer.sln`)
1. Abre `audio-visualizer.sln`.
2. En el selector de configuración, elige **Release** y **x64**.
3. Pulsa **F5** o haz clic en **Compilar -> Compilar solución** (Ctrl + Mayús + B).

---

## 4. Configuración en Visual Studio Code

1. Instala las extensiones recomendadas:
   - **C/C++** (`ms-vscode.cpptools`)
   - **CMake Tools** (`ms-vscode.cmake-tools`)
2. Abre la carpeta del repositorio en VS Code.
3. CMake Tools te pedirá seleccionar un *Kit* de compiladores. Selecciona:
   `Visual Studio Community 2026 Release - amd64` (o equivalente x64).
4. En la barra de estado inferior, selecciona la variante `[Release]`.
5. Pulsa **F7** para compilar o **F5** para depurar directamente.

---

## 5. El Paso Post-Build y Estructura de Salida

Tanto en CMake como en la solución `.vcxproj`, existe un paso de copia automatizado (`POST_BUILD`) que garantiza que el ejecutable tenga acceso inmediato a sus bibliotecas dinámicas, shaders y configuración:

```
build/Release/ (o x64/Release/)
|-- audio-visualizer.exe          Ejecutable compilado
|-- config.json                   Copia del archivo de configuración inicial
|-- libfftw3f-3.dll               Biblioteca de FFT rápida (Single Precision / Float)
|-- glew32.dll                    Gestor de extensiones OpenGL 3.3
|-- glfw3.dll                     Gestor de ventanas y eventos GLFW
`-- shaders/                      Carpeta con los shaders GLSL en tiempo de ejecución
    |-- bars.vert / bars.frag
    |-- quad.vert
    |-- radial.frag
    |-- waveform.frag
    `-- waterfall.frag
```

---

## 5.5 Pruebas Numéricas

CMake define cuatro objetivos de prueba sin audio ni ventana: `band_metrics_test` (partición en bandas, métricas por banda y conservación de la energía), `band_synthesis_test` (máscaras, reconstrucción por solapamiento y suma, y teorema de la suma de bandas), `motion_test` (curvas y duración de las transiciones de Fluent) y `resolution_test` (resolución variable frente a latencia):

```powershell
cmake --build build --config Release --target band_metrics_test band_synthesis_test motion_test resolution_test
ctest --test-dir build -C Release --output-on-failure
```

Los criterios y sus resultados están en el documento 11, fases B, C y D, y en el documento 12, fase 2. Los objetivos de prueba no forman parte de la solución `.sln`.

## 6. Empaquetado y Distribución

Para distribuir la aplicación a otro equipo con Windows:
1. Compila en modo `Release`.
2. Copia todo el contenido de la carpeta `build/Release/` en un archivo ZIP (o carpeta standalone).
3. **No se requiere instalación**: El programa es portable. Solo requiere que la máquina destino tenga instalado el runtime estándar de C++ (*Microsoft Visual C++ Redistributable 2015-2022* o superior).

---

## 7. Solución de Problemas Frecuentes

| Error / Síntoma | Causa Probable | Solución |
|---|---|---|
| `fatal error C1083: Cannot open include file: 'imgui.h'` | Falta la carpeta `imgui-1.91.5/` o el `.vcxproj` no la tiene en sus rutas de inclusión. | Comprobar que `imgui-1.91.5/` está junto al `.vcxproj` (es parte del repositorio) y que aparece en `AdditionalIncludeDirectories`. |
| `fatal error C1083: Cannot open include file: 'GL/glew.h'` | Se está ejecutando `cl.exe` manualmente sin los flags `/I`. | Utiliza siempre CMake (`cmake --build build`) o Visual Studio, los cuales configuran las rutas relativas automáticamente. |
| `LNK4272: library machine type 'x64' conflicts with target machine type 'x86'` | CMake o el compilador intentaron generar binarios para 32 bits (Win32). | Asegúrate de especificar `-A x64` en CMake (`cmake -B build -S . -A x64`). Las bibliotecas incluidas son exclusivamente de 64 bits. |
| `0xc000007b (Error al iniciar la aplicación)` | Conflicto de arquitectura de DLLs (ej. DLL de 32 bits mezclada con ejecutable de 64 bits). | Limpia la carpeta `build/` y vuelve a compilar con CMake para que copie las DLLs x64 oficiales. |
| `LNK1104: cannot open file audio-visualizer.exe` | La aplicación anterior sigue en ejecución en segundo plano o el depurador está anclado a ella. | Abre el Administrador de Tareas y finaliza `audio-visualizer.exe`, o cierra la ventana activa antes de compilar. |
| `El ejecutable se cierra inmediatamente sin mostrar ventana` | El dispositivo de audio predeterminado no está activo o está deshabilitado en Windows. | Conecta o activa unos auriculares/altavoces en la bandeja del sistema de Windows. |

---

## 8. Requisitos Adicionales de la Versión 3.0

| Componente | Requisito | Motivo | Degradación si falta |
|---|---|---|---|
| Materiales del sistema (Mica, Acrílico) | Windows 11 22H2 (build 22621) o superior; `dwmapi.lib` se enlaza por `#pragma comment` en `src/platform/window_effects.cpp` | `DwmSetWindowAttribute` con el atributo 38 solo existe desde esa build (documento 12, sección 2) | Fondo opaco, sin error; el HUD muestra el motivo |
| Framebuffer transparente | Driver OpenGL con soporte de composición; `GLFW_TRANSPARENT_FRAMEBUFFER` | Necesario para ver el material del sistema a través de la ventana | Ventana opaca; el material propio del panel sigue funcionando |
| Post-procesado | OpenGL 3.3 Core, sin cambios | FBO y texturas ya disponibles en 3.3 | No aplica |
| Reconstrucción de bandas | FFTW ya incluida (`fftwf_plan_dft_c2r_1d`) | La IFFT usa la misma biblioteca | No aplica |

No se prevén dependencias nuevas de terceros. Toda la 3.0 se construye con las bibliotecas ya versionadas en el repositorio.
