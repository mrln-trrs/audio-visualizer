# Changelog

Todos los cambios relevantes de este proyecto se documentan en este archivo.
El formato se basa en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/) y el proyecto sigue [Versionado Semántico](https://semver.org/lang/es/).

## [Sin publicar]

### Pendiente
- Dependencias por vcpkg en modo manifiesto, GLEW sustituido por glad y CI en GitHub Actions (`windows-latest`).

## [3.0.0] - 2026-11-26

Primera versión etiquetada. Recoge el trabajo de la 3.0 descrito en `docs/11`, `docs/12` y `docs/13`.

### Añadido
- Trama de análisis (`AnalysisFrame`) con triple búfer entre análisis y render.
- Bandas configurables con métricas (energía, RMS, pico), dinámica de ataque y caída por banda y modo **Medidores por banda** (tecla `6`).
- Ondas por banda reconstruidas por IFFT enmascarada y modo **Osciloscopio apilado** (tecla `5`); seis modos en total.
- Resolución variable opcional (FFT larga en graves, corta en agudos).
- Diseño Fluent: material acrílico propio, Mica y Acrílico del sistema en Windows 11, transiciones y respeto a las preferencias de accesibilidad.
- Cuatro pruebas numéricas con CTest (`band_metrics`, `band_synthesis`, `motion`, `resolution`).
- Licencia GPL-3.0 (`LICENSE`), obligada por FFTW.
- `CMakePresets.json` (`vs-x64`, `release`, `debug`).

### Cambiado
- README actualizado a la 3.0: seis modos, requisitos, compilación con CMake y Visual Studio, dependencias y sus licencias, pendientes.
- Versión del proyecto en CMake: 3.0.0.

### Corregido
- `.gitignore`: las reglas `x64/` y `build*/` pasan a `/x64/` y `/build*/` para no excluir `glew-2.1.0/{lib,bin}/Release/x64`. Los binarios x64 de GLEW siguen sin estar en el repositorio; el README explica cómo obtenerlos.

## Versiones anteriores (sin etiqueta)

- **2.0**: OpenGL 3.3 Core con shaders, panel Dear ImGui, peak-hold, cuatro modos (barras, radial, osciloscopio, cascada), selector de dispositivo WASAPI y build con CMake.

[Sin publicar]: https://github.com/mrln-trrs/audio-visualizer/compare/v3.0.0...HEAD
[3.0.0]: https://github.com/mrln-trrs/audio-visualizer/releases/tag/v3.0.0
