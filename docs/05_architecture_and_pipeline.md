# Arquitectura del Sistema y Pipeline Matemático de Audio - Audio Visualizer 2.0

> **Estado:** Implementado en la 2.0, con una nota sobre la ventana en la sección 2.3 que condiciona la 3.0.  
> **Alcance:** Arquitectura de hilos, pipeline de señal y fundamento matemático de lo que hoy calcula el programa.  
> **Documentos relacionados:** [10_signal_decomposition_theory.md](10_signal_decomposition_theory.md) para la extensión a bandas y sus demostraciones, [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md) para su arquitectura.  
> **Convención:** este documento distingue entre lo *implementado* (verificable en el código de `master`) y lo *propuesto* (diseño para la versión 3.0). Toda cifra cuantitativa se deriva o se referencia; no hay estimaciones sin base.

Este documento detalla la arquitectura de software multihilo, el flujo de datos de señal de audio en tiempo real y el fundamento matemático de cada etapa de procesamiento y renderizado en **Audio Visualizer 2.0**.

---

## 1. Arquitectura de Hilos y Concurrencia

La aplicación se ejecuta sobre una arquitectura de **tres hilos concurrentes desacoplados**, diseñada para garantizar que la captura de audio nunca sufra pérdida de muestras (*dropouts*), el análisis de Fourier corra a máxima velocidad vectorial y el hilo de renderizado alcance la tasa de refresco nativa del monitor (60, 144 o 240+ Hz) sin contención de bloqueos.

```mermaid
flowchart TD
    subgraph S1[Entrada de Audio]
        WASAPI[Motor de Audio Windows<br/>WASAPI Loopback<br/>Paquetes de ~10 ms]
    end

    subgraph H1[Hilo 1: Captura (AudioCaptureThread)]
        CapSession[RunCaptureSession<br/>Formato: Float32 o PCM 16/24/32<br/>Mezcla estereo a mono]
        DevSwitch{¿device_change_pending?}
    end

    subgraph M1[Punto de Intercambio 1: AudioData]
        Ring[(Búfer Anular Circular<br/>8192 muestras float<br/>Mascara binaria: i AND 8191)]
        Sync1[std::mutex + condition_variable<br/>Predicado: total_samples >= consumido + 256]
    end

    subgraph H2[Hilo 2: Procesado FFT (AudioProcessingThread)]
        Hann[Ventana periódica de Hann<br/>Normalizacion energetica]
        FFTW[FFTW 3.3.5 r2c 1D<br/>1025 bins complejos]
        Bands[Mapeo de frecuencias<br/>Lineal o Logaritmica<br/>Interpolacion sub-bin]
        DB[Escala logaritmica dBFS<br/>20 log10 m<br/>Normalizacion 0..1]
    end

    subgraph M2[Punto de Intercambio 2: VisualizerData]
        DoubleBuf[(std::vector<float> spectrum<br/>std::vector<float> waveform<br/>std::atomic<uint64_t> generation)]
        Sync2[std::mutex ligero para swap]
    end

    subgraph H0[Hilo 0 / Principal: Renderizado y GUI (RenderThread)]
        GLContext[Contexto Modern OpenGL 3.3 Core]
        Filters[Filtro temporal IIR de 1er orden<br/>a_up y a_down continuos]
        PeakPhys[Fisica de Peak-Hold<br/>Retardo + caida gravitatoria]
        Shaders[Pipeline de Shaders GLSL<br/>Barras, Radial, Waveform, Cascada]
        ImGui[Dear ImGui Overlay HUD]
        Limiter[Limitador adaptativo vsync / multimedia timer]
    end

    WASAPI --> CapSession
    CapSession --> DevSwitch
    DevSwitch -- Si --> CapSession
    DevSwitch -- No --> Ring
    Ring --> Sync1
    Sync1 --> Hann
    Hann --> FFTW
    FFTW --> Bands
    Bands --> DB
    DB --> DoubleBuf
    DoubleBuf --> Sync2
    Sync2 --> Filters
    Filters --> PeakPhys
    PeakPhys --> Shaders
    Shaders --> ImGui
    ImGui --> Limiter
```

---

## 2. Pipeline de Señal y Fundamentos Matemáticos

### 2.1 Captura y Mezcla a Mono (WASAPI)
WASAPI en modo loopback entrega paquetes intercalados (*interleaved*) de $C$ canales (habitualmente estéreo, $C=2$) con $N$ cuadros a la frecuencia nativa del endpoint (44.1 kHz o 48 kHz).
Para cada cuadro $f \in [0, N-1]$, la muestra mono $x[f]$ se normaliza a $[-1.0, 1.0]$:

$$\text{Float32:} \quad x[f] = \frac{1}{C} \sum_{c=0}^{C-1} s[f \cdot C + c]$$
$$\text{PCM16:} \quad x[f] = \frac{1}{C} \sum_{c=0}^{C-1} \frac{s_{16}[f \cdot C + c]}{32768.0}$$
$$\text{PCM24:} \quad x[f] = \frac{1}{C} \sum_{c=0}^{C-1} \frac{s_{24}[f \cdot C + c]}{8388608.0}$$

### 2.2 Búfer Anular (Ring Buffer) y Salto Deslizante (Hop Size)
- Tamaño del búfer anular: $M = 8192 = 2^{13}$.
- Índice de inserción sin divisiones: $\text{idx} = \text{total\_samples} \ \& \ (M - 1)$.
- Tamaño de la ventana FFT: $N_{\text{FFT}} = 2048$ (~42.7 ms a 48 kHz, resolución de bin $\Delta f = \frac{f_s}{N_{\text{FFT}}} = 23.4375\text{ Hz}$).
- Salto deslizante: $N_{\text{HOP}} = 256$ (~5.3 ms a 48 kHz).
- **Cero acumulación de retraso**: Si el hilo de render o el SO experimentan un retardo y llegan múltiples paquetes juntos, el procesador lee directamente las últimas 2048 muestras a partir de $\text{total\_samples}$, saltándose el historial obsoleto.

### 2.3 Enventanado de Hann (Reducción de Dispersión Espectral)
Para evitar el lóbulo secundario (*spectral leakage*) propio de ventanas rectangulares, se aplica la función periódica de Hann ponderada:

$$w[n] = 0.5 \cdot \left(1 - \cos\left(\frac{2\pi n}{N_{\text{FFT}}}\right)\right), \quad n \in [0, N_{\text{FFT}} - 1]$$

Nota. Esta es la forma periódica (denominador $N_{\text{FFT}}$), implementada en `src/analysis/window_function.cpp`. Es la única que cumple la condición de solapamiento constante necesaria para la reconstrucción exacta por solapamiento y suma (demostración en el documento 10, sección 4.2). La forma simétrica (denominador $N_{\text{FFT}} - 1$), usada en la 2.0 original, difiere en el espectro en el orden de $10^{-3}$ relativo y no se aprecia en pantalla.

Normalización energética para que una onda senoidal pura a escala completa (amplitud 1.0) genere un pico de magnitud 1.0 exacto:

$$K_{\text{norm}} = \frac{2}{\sum_{n=0}^{N_{\text{FFT}}-1} w[n]}$$

### 2.4 FFT Real a Compleja (FFTW)
Se ejecuta la transformada unidimensional real a compleja (`fftwf_plan_dft_r2c_1d`). Para $N_{\text{FFT}} = 2048$ muestras reales de entrada, genera $K = 1025$ coeficientes complejos simétricos $X[k] = \text{Re}[k] + i \cdot \text{Im}[k]$ para frecuencias $f_k = k \cdot \Delta f$:

$$|X[k]| = K_{\text{norm}} \cdot \sqrt{\text{Re}[k]^2 + \text{Im}[k]^2}$$

### 2.5 Mapeo de Frecuencia a Barras

#### A. Escala Lineal
Para la barra $i \in [0, B-1]$ con ancho constante $\Delta f_{\text{barra}}$ (por defecto 10 Hz):
$$f_0(i) = i \cdot \Delta f_{\text{barra}}, \quad f_1(i) = (i+1) \cdot \Delta f_{\text{barra}}$$

#### B. Escala Logarítmica (Percepción Musical por Octavas)
El oído humano percibe el tono logarítmicamente. Entre $f_{\min}$ (30 Hz) y $f_{\max}$ (16 kHz):

$$f_0(i) = f_{\min} \cdot \left(\frac{f_{\max}}{f_{\min}}\right)^{\frac{i}{B}}, \quad f_1(i) = f_{\min} \cdot \left(\frac{f_{\max}}{f_{\min}}\right)^{\frac{i+1}{B}}$$

Los límites en índices fraccionales de bins son $k_0 = \frac{f_0}{\Delta f}$ y $k_1 = \frac{f_1}{\Delta f}$.

#### C. Algoritmo de Muestreo Sub-Bin vs. Agrupación
- **Si $k_1 - k_0 < 1.0$ (Banda estrecha en graves)**: Varios píxeles caen dentro de un mismo bin FFT. Se realiza una **interpolación lineal** en el centro fraccional $k_c = \frac{k_0 + k_1}{2}$. Esto previene bandas negras o vacías en frecuencias graves.
- **Si $k_1 - k_0 \ge 1.0$ (Banda ancha en agudos)**: Abarca múltiples bins. Se calcula el **valor de pico (máximo)** de los bins cubiertos para capturar transitorios y armónicos nítidos.

### 2.6 Escala Decibélica (dBFS) y Rango Dinámico
La percepción de intensidad sonora sigue una escala logarítmica de potencia. La magnitud $m \in [0, 1]$ se convierte a decibelios relativos a plena escala (dBFS) y se normaliza al rango configurado $R_{\text{dB}}$ (habitualmente 60 dB):

$$\text{dB} = 20 \log_{10}(m + 10^{-9})$$
$$y_{\text{espectro}} = \text{clamp}\left(\frac{\text{dB} + R_{\text{dB}}}{R_{\text{dB}}}, 0.0, 1.0\right)$$

---

## 3. Animación Temporal y Física de Renderizado

### 3.1 Filtro de Suavizado Temporal IIR (Independiente de FPS)
Para que el visualizador se mueva con idéntica fluidez tanto a 60 Hz como a 144 Hz o 240 Hz, los factores de interpolación dependen del tiempo transcurrido real entre cuadros $\Delta t$:

$$\alpha_{\text{ataque}} = 1 - e^{-\frac{\Delta t \cdot 1000}{\tau_{\text{ataque}}}}, \quad \alpha_{\text{caída}} = 1 - e^{-\frac{\Delta t \cdot 1000}{\tau_{\text{caída}}}}$$

Donde $\tau_{\text{ataque}}$ y $\tau_{\text{caída}}$ son constantes en milisegundos definidas en la configuración (ej. 12 ms y 160 ms).
La altura animada de la barra $h_i(t)$ responde a:

$$h_i(t) = h_i(t - \Delta t) + \left(y_i - h_i(t - \Delta t)\right) \cdot \begin{cases} \alpha_{\text{ataque}} & \text{si } y_i > h_i(t - \Delta t) \\ \alpha_{\text{caída}} & \text{si } y_i \le h_i(t - \Delta t) \end{cases}$$

### 3.2 Mecánica de los Marcadores de Pico (Peak-Hold)
Cada barra dispone de un indicador de pico $p_i$ y un temporizador de sostenimiento $t_{\text{hold}, i}$:
1. **Nuevo Máximo**: Si $h_i(t) \ge p_i(t - \Delta t)$:
   $$p_i(t) = h_i(t), \quad t_{\text{hold}, i} = T_{\text{retardo}} \quad (\text{ej. } 350\text{ ms})$$
2. **Fase de Sostenimiento**: Si $h_i(t) < p_i$ y $t_{\text{hold}, i} > 0$:
   $$t_{\text{hold}, i} = t_{\text{hold}, i} - \Delta t$$
3. **Fase de Caída por Gravedad**: Si $t_{\text{hold}, i} \le 0$:
   $$p_i(t) = \max\left(h_i(t), \; p_i(t - \Delta t) - v_{\text{gravedad}} \cdot \Delta t\right)$$

---

## 4. Pipeline Gráfico y Shaders GLSL

### 4.1 Modo 1: Barras con Peak-Hold
- **Malla**: VBO dinámico subido mediante `glBufferData(..., GL_DYNAMIC_DRAW)`. Cada barra genera 2 quads (8 vértices, 12 índices).
- **Color**: Interpolación vertical desde el color base oscuro en la base hasta un color brillante proporcional a la altura en la cúspide, rematado por el marcador de pico con luminosidad amplificada.

### 4.2 Modo 2: Espectro Radial / Circular ([`radial.frag`](../shaders/radial.frag))
- **Mapeo Polar**: Para cada fragmento en pantalla $(x, y)$, se calcula su vector respecto al centro:
  $$\mathbf{st} = \frac{(x, y) - 0.5 \cdot \mathbf{res}}{\min(\text{res}_x, \text{res}_y)}, \quad r = \|\mathbf{st}\|, \quad \theta = \text{atan2}(\mathbf{st}_y, \mathbf{st}_x)$$
- **Simetría Espejada**: $\theta_{\text{norm}} = \frac{|\theta|}{\pi} \in [0, 1]$.
- **Muestreo**: Se consulta la textura 1D del espectro en $\text{texture}(u\_spectrum\_tex, (\theta_{\text{norm}}, 0.5))$.
- **Resplandor Central (Bass Pulse)**: Las frecuencias bajas muestreadas cerca de $\theta_{\text{norm}} \approx 0.02$ modulan un halo gaussiano en el núcleo central $r < 0.15$.

### 4.3 Modo 3: Osciloscopio / Waveform ([`waveform.frag`](../shaders/waveform.frag))
- Muestrea las muestras crudas de audio en el dominio del tiempo $w(u) \in [-1.0, 1.0]$.
- Calcula la distancia vertical $d = |v - (0.5 + 0.42 \cdot w(u))|$.
- Simula la emisión luminosa de fósforo P31 de tubos CRT:
  $$I(d) = 1.5 \cdot e^{-(180 \cdot d)^2} + 0.45 \cdot e^{-28 \cdot d} + 0.15 \cdot e^{-8 \cdot d}$$
- Incorpora una retícula milimétrica sutil modulada en el fondo.

### 4.4 Modo 4: Espectrograma Cascada 2D ([`waterfall.frag`](../shaders/waterfall.frag))
- Utiliza una textura 2D circular de $512 \times 256$ en memoria de GPU (`GL_R32F`).
- Cada nuevo espectro se escribe en la fila $y_{\text{head}} = (y_{\text{head}} + 1) \bmod 256$ mediante `glTexSubImage2D`.
- El fragment shader aplica desplazamiento toroidal fraccional $\text{fract}(u\_scroll\_head - (1.0 - v))$ y mapea la intensidad a una paleta térmica continua de 5 nodos de color (*Inferno/Magma*).

---

## 5. Extensión a la Descomposición en Bandas (Propuesta 3.0)

El pipeline de esta versión termina en la sección 2.6 con un vector de valores en $[0,1]$. La versión 3.0 conserva las secciones 2.1 a 2.4 sin cambios y sustituye 2.5 y 2.6 por una trama de análisis completa. La justificación matemática de que la señal puede separarse en bandas y reconstruirse exactamente, cuánta información produce y qué límite impone el principio de incertidumbre está en [10_signal_decomposition_theory.md](10_signal_decomposition_theory.md). La arquitectura (estructuras, hilos, texturas, configuración) está en [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md).

Correspondencia entre este documento y la extensión:

| Sección aquí | Se conserva en 3.0 | Cambia en 3.0 |
|---|---|---|
| 2.1 Captura y mezcla | Sí | No |
| 2.2 Anillo y salto | Sí | No |
| 2.3 Ventana de Hann | Sí, ya periódica | No |
| 2.4 FFT r2c | Sí | Se conserva la fase además de la magnitud |
| 2.5 Mapeo a barras | Pasa al renderizador de barras | El procesado publica bins y bandas, no píxeles |
| 2.6 Escala dB | Sí | Se publica dB sin normalizar y la normalización es por renderizador |
| 3.1 Filtro IIR | Sí | Constantes por banda en lugar de globales |
| 3.2 Peak-hold | Sí | Sin cambios |
| 4 Shaders | Sí | Se añaden osciloscopio apilado, medidores por banda y Lissajous |
