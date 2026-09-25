# Manual de Usuario y Referencia de Configuración - Audio Visualizer 2.0

Este documento proporciona una guía exhaustiva para el usuario final sobre la operación del visualizador, el uso del panel interactivo **Dear ImGui**, la calibración de parámetros y la referencia técnica de `config.json`.

---

## 1. Inicio Rápido y Operación Básica

1. Pon a reproducir cualquier fuente de audio en tu equipo (Spotify, YouTube, reproductor de medios, DAW o videojuegos).
2. Ejecuta `audio-visualizer.exe` (desde la raíz o desde `build/Release/`).
3. El visualizador detectará el audio y comenzará a responder instantáneamente a la música.
4. Para abrir el menú de ajustes en vivo, pulsa la tecla **`Tab`** o **`H`**.

---

## 2. Atajos de Teclado y Control

| Tecla | Función | Descripción |
|---|---|---|
| **`Tab`** o **`H`** | Alternar HUD | Muestra u oculta el panel de control interactivo de Dear ImGui. |
| **`1`** | Modo Barras | Activa el ecualizador de barras clásico con marcadores de pico *Peak-Hold*. |
| **`2`** | Modo Radial | Activa el espectro circular procedural con pulso reactivo en el centro. |
| **`3`** | Modo Osciloscopio | Activa la forma de onda continua en el dominio del tiempo estilo tubo analógico CRT. |
| **`4`** | Modo Cascada 2D | Activa el espectrograma continuo con mapa de calor térmico (*Waterfall*). |
| **`Alt + F4`** | Salir | Cierra limpiamente la aplicación y libera todos los recursos. |

---

## 3. Guía del Panel de Control Interactivo (HUD)

El panel flotante de Dear ImGui permite calibrar todos los aspectos visuales y de audio en tiempo real sin salir de la aplicación ni pausar la música.

```
+-------------------------------------------------------------+
| Panel de Control - Audio Visualizer 2.0 (Tab / H)       [X] |
+-------------------------------------------------------------+
| Modo de Visualizacion:                                      |
| (*) Barras con Picos (1)   ( ) Radial (2)                   |
| ( ) Osciloscopio (3)       ( ) Cascada 2D (4)               |
|-------------------------------------------------------------|
| Filtros y Dinamica:                                         |
| Ganancia:       [====|=============] 1.00x                  |
| Ataque (ms):    [==|===============] 12 ms                  |
| Caida (ms):     [======|===========] 160 ms                 |
| Rango (dB):     [=======|==========] 60 dB                  |
|-------------------------------------------------------------|
| Escala de Frecuencia:                                       |
| (*) Lineal                 ( ) Logaritmica (Octavas)        |
| Hz por Barra:   [====|=============] 10.0 Hz                |
|-------------------------------------------------------------|
| Efecto Peak-Hold (Barras):                                  |
| [X] Habilitar Marcadores de Pico                            |
| Retardo Pico:   [====|=============] 350 ms                 |
| Velocidad Caida:[=====|============] 1.8                    |
|-------------------------------------------------------------|
| Paleta de Colores:                                          |
| Color Base:     [ #A62626 ]                                 |
| Color Picos:    [ #FFD933 ]                                 |
| Presets: [Cyberpunk Neon] [Matrix Emerald] [Solar Amber]    |
|-------------------------------------------------------------|
| Dispositivo de Audio (WASAPI):                              |
| Salida:         [ Altavoces (Realtek Audio) [Predet]   v ]  |
|-------------------------------------------------------------|
| [ Guardar en config.json ]   Configuracion guardada!        |
+-------------------------------------------------------------+
```

### Secciones Detalladas del HUD:

#### A. Selector de Modos de Visualización
- Conmuta entre los 4 algoritmos de renderizado por GPU de forma instantánea.

#### B. Filtros y Dinámica
- **Ganancia (`amplitude_factor`)**: Multiplicador de escala vertical aplicado al espectro (0.1x a 3.0x). Si la música está grabada muy baja, súbela a 1.5x - 2.0x.
- **Ataque (`attack_ms`)**: Rapidez con la que las barras reaccionan ante transitorios y golpes repentinos de volumen. Valores bajos (5-15 ms) ofrecen una respuesta explosiva y enérgica; valores altos (30-50 ms) ofrecen una respuesta más pausada.
- **Caída (`release_ms`)**: Inercia de descenso cuando baja la música. Valores entre 120 y 200 ms producen el balance óptimo entre naturalidad y suavidad visual.
- **Rango Dinámico (`dynamic_range_db`)**: Profundidad en decibelios visualizados (ej. 60 dB representa de 0 dBFS a -60 dBFS). Un valor mayor permite ver matices muy sutiles de fondo; un valor menor se concentra en los instrumentos predominantes.

#### C. Escala de Frecuencia
- **Lineal**: Cada barra cubre un ancho en hercios idéntico (`bin_grouping_factor`, típicamente 10 Hz por barra). Excelente para análisis técnico de armónicos.
- **Logarítmica**: Reparte el ancho de pantalla según la escala musical (por octavas). Otorga mayor presencia visual a los bombos, bajos y medios, asemejándose a los visualizadores de equipos de alta fidelidad (Hi-Fi).
- **Rango Hz (`min_frequency`, `max_frequency`)**: Permite recortar frecuencias inaudibles o centrar la visualización entre 30 Hz y 16 kHz.

#### D. Efecto Peak-Hold
- **Habilitar Marcadores de Pico**: Dibuja una marca luminosa horizontal en el punto más alto alcanzado por cada barra.
- **Retardo Pico (`peak_hold_time_ms`)**: Tiempo en milisegundos que el marcador se mantiene flotando en la cúspide antes de comenzar a caer (recomendado: 250 - 450 ms).
- **Velocidad de Caída (`peak_decay_speed`)**: Rapidez de descenso gravitatorio una vez agotado el retardo.

#### E. Paleta de Colores y Presets Rápidos
- **Color Base**: Modula el degradado inferior del ecualizador o el color de emisión del anillo radial.
- **Color Picos**: Color de los marcadores de pico y de los halos exteriores.
- **Botones de Preset**:
  - *Cyberpunk Neon*: Cyan eléctrico (`#0DC0F2`) y Magenta brillante (`#FF26A6`).
  - *Matrix Emerald*: Verde esmeralda (`#1AD959`) y Lima fósforo (`#D9FF33`).
  - *Solar Amber*: Rojo caldera cálido (`#D9401A`) y Ámbar dorado (`#FFD933`).

#### F. Selector de Dispositivos WASAPI
- Muestra una lista desplegable con todos los endpoints de salida activos en Windows (Altavoces, Auriculares USB, DACs externos, Voicemeeter, monitores HDMI).
- Al seleccionar una opción, el motor conmuta atómicamente la captura sin detener la animación.

#### G. Botón "Guardar en config.json"
- Escribe de inmediato todas las configuraciones ajustadas en el archivo `config.json` en disco. Las preferencias quedarán guardadas permanentemente para las siguientes sesiones.

---

## 4. Referencia Completa de `config.json`

El archivo de configuración reside en el mismo directorio que el ejecutable y se carga automáticamente al iniciar.

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

### Tabla de Parámetros:

| Clave | Tipo | Por Defecto | Rango Recomendado | Explicación |
|---|---|---|---|---|
| `visual_mode` | Entero | `0` | `0` a `3` | `0`: Barras, `1`: Radial, `2`: Osciloscopio, `3`: Cascada. |
| `amplitude_factor` | Flotante | `1.0` | `0.1` a `3.0` | Ganancia multiplicativa sobre el espectro normalizado. |
| `attack_ms` | Flotante | `12.0` | `1.0` a `80.0` | Tiempo de respuesta al ascenso en milisegundos. |
| `release_ms` | Flotante | `160.0` | `20.0` a `500.0` | Tiempo de caída inercial en milisegundos. |
| `dynamic_range_db` | Flotante | `60.0` | `20.0` a `100.0` | Rango vertical visible en decibelios (dBFS). |
| `frequency_scale` | String | `"linear"` | `"linear"` o `"log"` | Reparto de frecuencias en el eje horizontal. |
| `bin_grouping_factor` | Flotante | `10.0` | `2.0` a `50.0` | Ancho de banda por barra en hercios (escala lineal). |
| `min_frequency` | Flotante | `30.0` | `10.0` a `200.0` | Frecuencia mínima en hercios (escala logarítmica). |
| `max_frequency` | Flotante | `16000.0` | `8000.0` a `22000.0` | Frecuencia máxima en hercios (escala logarítmica). |
| `peak_hold_enabled` | Booleano | `true` | `true` / `false` | Activa o desactiva las marcas de pico en modo barras. |
| `peak_hold_time_ms` | Flotante | `350.0` | `50.0` a `1000.0` | Tiempo de sostenimiento del pico antes de caer. |
| `peak_decay_speed` | Flotante | `1.8` | `0.5` a `5.0` | Velocidad de caída por gravedad de los picos. |
| `base_color_rgb` | Array [3] | `[0.65, 0.15, 0.15]` | `[0..1, 0..1, 0..1]` | Componentes Rojo, Verde y Azul del color base. |
| `peak_color_rgb` | Array [3] | `[1.0, 0.85, 0.2]` | `[0..1, 0..1, 0..1]` | Componentes Rojo, Verde y Azul de los marcadores de pico. |
| `selected_device_name`| String | `""` | Nombre del dispositivo | Dispositivo WASAPI guardado (`""` = predeterminado de Windows). |
| `vsync` | Booleano | `true` | `true` / `false` | Sincronización vertical con el refresco de pantalla. |
| `max_fps` | Entero | `0` | `0` a `360` | Límite del limitador de cuadros (`0` = detecta Hz del monitor). |

---

## 5. Diagnóstico y Métricas en la Barra de Título

La barra de título se actualiza una vez por segundo ofreciendo telemetría en tiempo real:

```
Audio Visualizer 2.0  |  [Barras]  |  144 fps (144 Hz, vsync)  |  100 esp/s  |  audio 48000 Hz
```

- **`[Modo]`**: Modo visual activo actualmente.
- **`N fps`**: Tasa real de cuadros por segundo que se están renderizando en pantalla.
- **`N Hz, vsync / limitador`**: Tasa de refresco detectada del monitor y mecanismo de sincronía activo.
- **`N esp/s`**: Transformadas rápidas de Fourier calculadas y publicadas por segundo (~100 espectros/s a 48 kHz).
- **`audio N Hz`**: Frecuencia de muestreo nativa del hardware de sonido de Windows.
