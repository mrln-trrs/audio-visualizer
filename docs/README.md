# Índice de Documentación del Proyecto - Audio Visualizer 2.0

Bienvenido al centro de documentación técnica y de diseño de **Audio Visualizer 2.0**. Aquí encontrarás la suite documental completa que cubre desde la visión del producto y la arquitectura matemática hasta las guías de compilación y manuales de usuario.

---

## Mapa de Documentos

```mermaid
flowchart TD
    Index[docs/README.md<br/>Centro de Documentacion] --> D1[01. PRD Lean<br/>docs/01_prd_lean.md]
    Index --> D2[02. Especificacion Tecnica<br/>docs/02_tech_spec.md]
    Index --> D3[03. UX Flows y 7 Estados<br/>docs/03_ux_flows.md]
    Index --> D4[04. Kanban y Criterios BDD<br/>docs/04_kanban_bdd.md]
    Index --> D5[05. Arquitectura y Pipeline<br/>docs/05_architecture_and_pipeline.md]
    Index --> D6[06. Compilacion y Toolchain<br/>docs/06_build_and_toolchain.md]
    Index --> D7[07. Manual de Usuario y Config<br/>docs/07_user_manual_and_config.md]
    Index --> D8[08. Guias y Contribucion<br/>docs/08_contributing_and_standards.md]
    Index --> D9[09. Recomendaciones y Mejoras<br/>docs/09_recommendations_and_future_improvements.md]
```

---

## Descripción de los Documentos

### 1. [01_prd_lean.md - PRD Lean (Product Requirements)](01_prd_lean.md)
- **Objetivo**: Define la visión de producto de Audio Visualizer 2.0.
- **Contenido**: Matriz In-Scope vs. Out-of-Scope (filtro anti-sobreingeniería), requerimientos funcionales (RF-01 a RF-07), requerimientos no funcionales (latencia, eficiencia de CPU/GPU, tolerancia a desconexiones) y métricas de éxito.

### 2. [02_tech_spec.md - Especificación Técnica](02_tech_spec.md)
- **Objetivo**: Define la ingeniería de software y decisiones de diseño.
- **Contenido**: Matriz *Build vs. Adopt*, diagrama de arquitectura de los 3 hilos concurrentes, inicialización de Modern OpenGL 3.3 Core Profile, integración de Dear ImGui y modelo de datos COM para WASAPI.

### 3. [03_ux_flows.md - Matriz de UX Flows y 7 Estados del Sistema](03_ux_flows.md)
- **Objetivo**: Modela el comportamiento de la interfaz y la experiencia del usuario.
- **Contenido**: Diagrama de estados (Carga, Silencio, Reproducción, Interacción HUD, Cambio de Dispositivo, Error/Desconexión y Cierre Limpio) y asignación ergonómica de atajos de teclado.

### 4. [04_kanban_bdd.md - Plan Ágil, Kanban y Criterios BDD](04_kanban_bdd.md)
- **Objetivo**: Estructura de historias de usuario y criterios de aceptación.
- **Contenido**: Definición de Preparado (DoR), Definición de Hecho (DoD) y desglose de las 4 épicas con escenarios formales en formato *Given-When-Then*.

### 5. [05_architecture_and_pipeline.md - Arquitectura y Pipeline Matemático](05_architecture_and_pipeline.md)
- **Objetivo**: Explicación matemática profunda del procesamiento de señal de audio.
- **Contenido**: Deducción de fórmulas de enventanado de Hann con normalización energética, FFT r2c (1025 bins), mapeo lineal y logarítmico por octavas, interpolación sub-bin, conversión a dBFS, filtros IIR independientes de FPS, física de *Peak-Hold* y shaders GLSL.

### 6. [06_build_and_toolchain.md - Guía de Compilación, Toolchain y Despliegue](06_build_and_toolchain.md)
- **Objetivo**: Manual técnico para compilar y empaquetar el proyecto.
- **Contenido**: Instrucciones para CMake CLI, Ninja, Visual Studio 2022/2026, VS Code, detalle del paso *Post-Build* automatizado y tabla de solución a errores frecuentes de compilación y enlaces DLL.

### 7. [07_user_manual_and_config.md - Manual de Usuario y Referencia de Configuración](07_user_manual_and_config.md)
- **Objetivo**: Guía de referencia para el usuario final y operadores.
- **Contenido**: Controles de teclado, guía visual del panel interactivo Dear ImGui, tabla completa de todas las claves de `config.json` (rangos, tipos y valores por defecto) y telemetría en la barra de título.

### 8. [08_contributing_and_standards.md - Guía de Contribución y Estándares](08_contributing_and_standards.md)
- **Objetivo**: Manual para desarrolladores que deseen extender o colaborar en el proyecto.
- **Contenido**: Estándares de código C++17, directrices de concurrencia y RAII, tutorial paso a paso con código para agregar un nuevo modo de visualización y checklist de Definition of Done para Pull Requests.

### 9. [09_recommendations_and_future_improvements.md - Guía de Recomendaciones y Mejoras](09_recommendations_and_future_improvements.md)
- **Objetivo**: Propuestas de ingeniería avanzada, optimizaciones y arquitectura futura.
- **Contenido**: Matriz de priorización (Esfuerzo vs. Impacto), cola SPSC Lock-Free con código C++, ventanas de análisis (Hann vs. Blackman-Harris), post-procesado con Bloom (resplandor neón), Compute Shaders y SSBO para 100k partículas a 240 FPS, presets múltiples de usuario en JSON, iconografía con FontAwesome, pipeline de CI/CD con GitHub Actions y diseño de abstracción multiplataforma para Linux/macOS.
