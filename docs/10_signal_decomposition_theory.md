# Fundamentos Matemáticos de la Descomposición de una Señal de Audio en Bandas - Audio Visualizer 3.0

> **Estado:** Propuesta (versión 3.0). Nada de lo descrito está implementado, salvo donde se indica expresamente que la 2.0 ya lo hace.  
> **Alcance:** Fundamento matemático de la separación de una señal en bandas: demostraciones, límites físicos, volúmenes de información y consecuencias.  
> **Documentos relacionados:** [05_architecture_and_pipeline.md](05_architecture_and_pipeline.md), [11_analysis_frame_architecture.md](11_analysis_frame_architecture.md), [01_prd_lean.md](01_prd_lean.md)  
> **Convención:** este documento distingue entre lo *implementado* (verificable en el código de `master`) y lo *propuesto* (diseño para la versión 3.0). Toda cifra cuantitativa se deriva o se referencia; no hay estimaciones sin base.

Este documento responde con rigor a cuatro preguntas: si una pista de audio puede separarse en las ondas que la componen, con qué fórmulas, cuánta información produce esa separación y qué límites físicos tiene. Cada afirmación se demuestra o se referencia a un resultado estándar de análisis de señales. Las consecuencias prácticas para el proyecto se recogen en la sección 9.

## Índice

1. [Modelo de la señal](#1-modelo-de-la-señal)
2. [La transformada discreta de Fourier como descomposición exacta](#2-la-transformada-discreta-de-fourier-como-descomposición-exacta)
3. [Enventanado y transformada de Fourier de tiempo corto](#3-enventanado-y-transformada-de-fourier-de-tiempo-corto)
4. [Reconstrucción perfecta y separación en bandas](#4-reconstrucción-perfecta-y-separación-en-bandas)
5. [El límite físico: principio de incertidumbre de Gabor](#5-el-límite-físico-principio-de-incertidumbre-de-gabor)
6. [Cuánta información se genera](#6-cuánta-información-se-genera)
7. [Resolución variable: transformada Q constante](#7-resolución-variable-transformada-q-constante)
8. [Alternativa: bancos de filtros en el dominio del tiempo](#8-alternativa-bancos-de-filtros-en-el-dominio-del-tiempo)
9. [Qué pasaría si se implementa](#9-qué-pasaría-si-se-implementa)
10. [Lo que esta técnica no hace](#10-lo-que-esta-técnica-no-hace)
11. [Referencias](#11-referencias)

## 1. Modelo de la señal

La captura WASAPI entrega una sucesión de muestras reales $x[n]$, $n \in \mathbb{Z}$, tomadas a la frecuencia de muestreo $f_s$ (48000 Hz en la máquina de referencia). Cada muestra es la mezcla a mono de los canales del dispositivo, ya normalizada al intervalo $[-1, 1]$.

La afirmación intuitiva "la pista es la suma de todas las ondas" tiene una formulación exacta: el teorema de Fourier establece que toda señal de energía finita se expresa como superposición de senoidales. En tiempo discreto y para un bloque finito de $N$ muestras, esa superposición es la transformada discreta de Fourier de la sección 2. La palabra "onda" en el resto del documento significa una componente senoidal, o la suma de las componentes de un rango de frecuencias, nunca un instrumento ni una voz (ver sección 10).

## 2. La transformada discreta de Fourier como descomposición exacta

### 2.1 Definición

Para un bloque $x[0..N-1]$ se define

$$X[k] = \sum_{n=0}^{N-1} x[n]\, e^{-i 2\pi k n / N}, \qquad k = 0, \dots, N-1$$

### 2.2 Teorema de inversión

**Enunciado.** $x[n] = \dfrac{1}{N} \displaystyle\sum_{k=0}^{N-1} X[k]\, e^{i 2\pi k n / N}$.

**Demostración.** Sustituyendo la definición de $X[k]$:

$$\frac{1}{N} \sum_{k=0}^{N-1} \sum_{m=0}^{N-1} x[m]\, e^{-i 2\pi k m / N} e^{i 2\pi k n / N} = \sum_{m=0}^{N-1} x[m] \cdot \frac{1}{N} \sum_{k=0}^{N-1} e^{i 2\pi k (n-m) / N}$$

La suma interior es una suma geométrica de razón $r = e^{i 2\pi (n-m)/N}$. Si $n = m$, cada término vale 1 y la suma es $N$. Si $n \neq m$, $r \neq 1$ y $r^N = e^{i 2\pi (n-m)} = 1$, luego $\sum_{k=0}^{N-1} r^k = \frac{1 - r^N}{1 - r} = 0$. Por tanto la suma interior vale $N\,\delta_{nm}$ y la expresión se reduce a $x[n]$. $\blacksquare$

**Consecuencia.** La DFT no pierde información. Es un cambio de base en $\mathbb{C}^N$: las $N$ muestras y los $N$ coeficientes son dos descripciones equivalentes del mismo bloque. Esto es lo que permite "separar" y volver a "sumar" sin error.

### 2.3 Simetría hermítica para señales reales

Si $x[n] \in \mathbb{R}$, conjugando la definición: $\overline{X[k]} = \sum_n x[n] e^{+i 2\pi k n/N} = X[N-k]$. Los coeficientes $k > N/2$ son redundantes y basta con $N/2 + 1$ de ellos. Es la razón de que FFTW `r2c` devuelva 1025 valores complejos para $N = 2048$, y de que el número de grados de libertad se conserve: $N$ reales de entrada frente a $2 \cdot (N/2+1) - 2 = N$ reales independientes de salida (la parte imaginaria de $X[0]$ y $X[N/2]$ es nula).

### 2.4 Conservación de la energía (Parseval)

$$\sum_{n=0}^{N-1} |x[n]|^2 = \frac{1}{N} \sum_{k=0}^{N-1} |X[k]|^2$$

**Demostración.** $\sum_n |x[n]|^2 = \sum_n x[n]\, \overline{x[n]}$. Sustituyendo $\overline{x[n]}$ por su expresión inversa conjugada e intercambiando sumas, aparece $\sum_n x[n] e^{-i2\pi kn/N} = X[k]$, con lo que queda $\frac{1}{N}\sum_k X[k]\,\overline{X[k]}$. $\blacksquare$

**Consecuencia para el visualizador.** La energía total de la pista es la suma de las energías de los bins. Si se define un conjunto de bandas que particiona los bins, la suma de las energías por banda es exactamente la energía total. Ningún medidor de energía por banda puede "inventar" ni "perder" energía respecto al total.

## 3. Enventanado y transformada de Fourier de tiempo corto

### 3.1 Por qué una ventana

La DFT supone que el bloque es periódico. Un bloque recortado de una señal real no lo es: en los bordes hay discontinuidades que se traducen en energía espuria repartida por todo el espectro (fuga espectral). Multiplicar el bloque por una ventana $w[n]$ que se anula suavemente en los extremos atenúa esa fuga. La versión 2.0 ya usa la ventana de Hann.

### 3.2 Definición de la STFT

Con salto $H$ entre tramas y ventana $w$ de longitud $N$:

$$X_m[k] = \sum_{n=0}^{N-1} x[n + mH]\, w[n]\, e^{-i 2\pi k n / N}$$

En la versión 2.0: $N = 2048$, $H = 256$, $f_s = 48000$. Cada trama cubre $N/f_s = 42{,}67$ ms y las tramas se producen cada $H/f_s = 5{,}33$ ms.

### 3.3 Ventana de Hann periódica

$$w[n] = \frac{1}{2}\left(1 - \cos\frac{2\pi n}{N}\right), \qquad n = 0, \dots, N-1$$

Nota de implementación. La 2.0 original usaba el denominador $N-1$ (ventana simétrica). Desde la reestructuración del código en `src/`, `analysis::MakePeriodicHann` implementa la forma periódica con denominador $N$, que es la que cumple la propiedad de suma constante de la sección 4.2. La diferencia numérica en el espectro es despreciable; la diferencia para la reconstrucción exacta es esencial.

## 4. Reconstrucción perfecta y separación en bandas

### 4.1 Transformada inversa por solapamiento y suma

Dada la STFT, se reconstruye

$$y[n] = \frac{\displaystyle\sum_m \tilde{x}_m[n - mH]\, w[n - mH]}{\displaystyle\sum_m w^2[n - mH]}, \qquad \tilde{x}_m = \text{IDFT}(X_m)$$

donde cada trama se invierte, se vuelve a multiplicar por la ventana (ventana de síntesis igual a la de análisis) y se suma en su posición. El denominador normaliza por la suma de ventanas al cuadrado.

### 4.2 Condición de solapamiento constante

**Enunciado.** Para la ventana de Hann periódica y un salto $H$ tal que $N/H = R$ es un entero mayor o igual a 2, se cumple

$$\sum_{m} w[n - mH] = \frac{R}{2} \quad \text{para todo } n.$$

**Demostración.** Escribiendo $w[n] = \frac{1}{2} - \frac{1}{2}\cos(2\pi n/N)$ y sumando sobre las $R$ tramas que cubren una posición $n$ cualquiera:

$$\sum_{m=0}^{R-1} w[n - mH] = \frac{R}{2} - \frac{1}{2} \sum_{m=0}^{R-1} \cos\!\left(\frac{2\pi (n - mH)}{N}\right)$$

El segundo sumatorio es la parte real de $e^{i2\pi n/N} \sum_{m=0}^{R-1} e^{-i 2\pi m H/N} = e^{i2\pi n/N} \sum_{m=0}^{R-1} e^{-i 2\pi m / R}$. Esa suma recorre las $R$ raíces $R$-ésimas de la unidad, cuya suma es cero para $R \geq 2$ (mismo argumento geométrico que en 2.2). Queda $R/2$. $\blacksquare$

Con $N = 2048$ y $H = 256$, $R = 8$ y la suma vale 4. La misma técnica demuestra que $\sum_m w^2[n-mH]$ también es constante para $R \geq 4$, porque $w^2$ solo contiene armónicos hasta $\cos(4\pi n/N)$. Con $R = 8$ el denominador de 4.1 es la constante $3R/8 = 3$.

**Consecuencia.** Con estos parámetros, $y[n] = x[n]$ exactamente (salvo error de redondeo en coma flotante, del orden de $10^{-7}$ relativo en `float`). La cadena STFT, ISTFT es una identidad.

### 4.3 Separación en bandas por enmascaramiento

Sea $\{B_1, \dots, B_K\}$ una partición de los índices de bin $\{0, \dots, N/2\}$. Se define la máscara $M_b[k] = 1$ si $k \in B_b$ y $0$ en caso contrario, extendida a $k > N/2$ por simetría hermítica. La onda de la banda $b$ es

$$x_b[n] = \text{ISTFT}\big( M_b[k]\, X_m[k] \big)[n]$$

**Teorema (la suma de las bandas es la señal).** $\displaystyle\sum_{b=1}^{K} x_b[n] = x[n]$.

**Demostración.** La ISTFT es lineal en $X_m[k]$ (composición de IDFT, producto por ventana y suma, todas lineales). Como las máscaras particionan los bins, $\sum_b M_b[k] = 1$ para todo $k$. Entonces $\sum_b x_b = \text{ISTFT}\big(\sum_b M_b X_m\big) = \text{ISTFT}(X_m) = x$ por 4.2. $\blacksquare$

Este resultado es el que hace exacto el modo "osciloscopio apilado" propuesto en la 3.0: las $K$ trazas superiores y la traza inferior con la mezcla son consistentes por construcción. La onda de una banda es real (la máscara respeta la simetría hermítica) y está alineada en el tiempo con las demás (todas comparten la misma ventana y el mismo salto).

### 4.4 Máscaras suaves

Una máscara binaria produce en el borde de la banda un filtro con transición abrupta, cuya respuesta al impulso oscila (fenómeno de Gibbs). Visualmente aparece como un "eco" débil antes y después de los transitorios en la onda de esa banda. Se mitiga con máscaras que decaen en uno o dos bins en el borde, por ejemplo con un coseno alzado. La propiedad $\sum_b M_b = 1$ se conserva si las rampas de bandas contiguas son complementarias, así que el teorema 4.3 sigue valiendo.

## 5. El límite físico: principio de incertidumbre de Gabor

### 5.1 Enunciado

Para cualquier señal $g(t)$ de energía finita, con dispersión temporal $\sigma_t$ y dispersión frecuencial $\sigma_f$ definidas como desviaciones típicas de $|g(t)|^2$ y $|G(f)|^2$ normalizadas,

$$\sigma_t\, \sigma_f \;\geq\; \frac{1}{4\pi}$$

La igualdad se alcanza únicamente para la gaussiana.

### 5.2 Esquema de demostración

Sea $g$ de energía unidad, centrada ($\int t|g|^2 = 0$, $\int f|G|^2 = 0$). Por la desigualdad de Cauchy-Schwarz,

$$\left| \int t\, g(t)\, \overline{g'(t)}\, dt \right|^2 \leq \int t^2 |g|^2\, dt \cdot \int |g'|^2\, dt = \sigma_t^2 \cdot (2\pi)^2 \sigma_f^2$$

donde la última igualdad usa que la transformada de $g'$ es $i2\pi f\, G(f)$ y Parseval. Integrando por partes el lado izquierdo, $\int t\, g\, \overline{g'}\, dt = -\frac{1}{2}\int |g|^2 dt + i\,\text{Im}(\cdot) $, cuya parte real vale $-1/2$. Luego $|\cdot|^2 \geq 1/4$, y despejando, $\sigma_t \sigma_f \geq 1/(4\pi)$. La igualdad en Cauchy-Schwarz exige $g' \propto t\, g$, cuya solución es la gaussiana. $\blacksquare$

### 5.3 Qué significa para el visualizador

No existe una "frecuencia unitaria" observable en tiempo real. Una senoidal pura solo está definida si dura infinito. Al cortar la señal en una ventana de duración $T$, cada componente aparece con una anchura espectral inversamente proporcional a $T$. Elegir $N$ es elegir dónde situarse en este compromiso:

| $N$ | Duración a 48 kHz | Ancho de bin $f_s/N$ | Ancho a -3 dB de Hann (1,44 bins) | Latencia añadida al centro (N/2) |
|---|---|---|---|---|
| 512 | 10,7 ms | 93,8 Hz | 135 Hz | 5,3 ms |
| 1024 | 21,3 ms | 46,9 Hz | 67 Hz | 10,7 ms |
| 2048 (actual) | 42,7 ms | 23,4 Hz | 34 Hz | 21,3 ms |
| 4096 | 85,3 ms | 11,7 Hz | 17 Hz | 42,7 ms |
| 8192 | 170,7 ms | 5,9 Hz | 8,4 Hz | 85,3 ms |
| 48000 | 1000 ms | 1,0 Hz | 1,4 Hz | 500 ms |

Distinguir dos notas graves separadas un semitono (unos 2,5 Hz a 40 Hz) exigiría ventanas de casi medio segundo, con un cuarto de segundo de retraso visual. Ninguna fórmula evita este coste: es la desigualdad 5.1. Lo que sí puede hacerse es usar ventanas distintas por rango, que es la sección 7.

## 6. Cuánta información se genera

### 6.1 Muestreo de señales paso banda

Una señal cuyo espectro ocupa una banda de anchura $B$ queda totalmente determinada por $2B$ muestras reales por segundo, o equivalentemente $B$ muestras complejas por segundo (teorema del muestreo aplicado a la envolvente compleja). Un bin de la STFT tiene anchura $f_s/N$, así que su evolución temporal se describe con $f_s/N$ valores complejos por segundo. La STFT lo muestrea a $f_s/H$ tramas por segundo, es decir con un factor de sobremuestreo

$$\rho = \frac{f_s/H}{f_s/N} = \frac{N}{H} = R$$

que con los parámetros actuales vale 8. La STFT es 8 veces redundante respecto a la representación mínima, un coste aceptable a cambio de la reconstrucción sencilla de la sección 4.

### 6.2 Teorema de conservación de la información

**Enunciado.** Si las bandas $B_1, \dots, B_K$ particionan el espectro $[0, f_s/2]$ y cada onda $x_b$ se muestrea a su tasa crítica $2 B_b$, el número total de muestras por segundo es $f_s$, igual al de la señal original.

**Demostración.** $\sum_b 2 B_b = 2 \sum_b B_b = 2 \cdot f_s/2 = f_s$. $\blacksquare$

**Consecuencia.** Separar la señal en bandas no crea información. La reordena. Cualquier volumen de datos por encima de $f_s$ muestras por segundo es redundancia introducida por comodidad de implementación (sobremuestreo), y puede elegirse libremente según el coste que se quiera asumir.

### 6.3 Volúmenes concretos para el proyecto

Con $f_s = 48000$, $N = 2048$, $H = 256$, `float` de 4 bytes:

| Representación | Cálculo | Bytes por segundo |
|---|---|---|
| Audio mono original | $48000 \times 4$ | 192 KB |
| STFT completa (compleja) | $1025 \times 187{,}5 \times 8$ | 1,54 MB |
| Solo magnitudes (versión 2.0) | $1025 \times 187{,}5 \times 4$ | 0,77 MB |
| Historial de 256 tramas complejas en memoria | $1025 \times 256 \times 8$ | 2,1 MB (total, no por segundo) |
| 8 bandas como ondas a $f_s$ completa | $8 \times 48000 \times 4$ | 1,54 MB |
| 32 bandas como ondas a $f_s$ completa | $32 \times 48000 \times 4$ | 6,14 MB |
| 1025 bins como ondas a $f_s$ completa | $1025 \times 48000 \times 4$ | 197 MB |
| 1025 bins a su tasa crítica (6.2) | $= f_s \times 4$ | 192 KB |

La penúltima fila es el error de diseño a evitar: reconstruir cada bin como una onda a frecuencia de muestreo completa multiplica por mil el volumen sin añadir nada. El diseño correcto conserva la STFT compleja (1,5 MB/s, cabe en cualquier GPU como una textura) y reconstruye ondas solo para las bandas que se muestran en pantalla en ese instante, que son decenas, no miles.

### 6.4 Coste de cómputo

Una IDFT de tamaño $N$ por FFT cuesta del orden de $N \log_2 N$ operaciones. Reconstruir $K$ bandas por trama cuesta $K \cdot N \log_2 N$; por segundo, $K \cdot N \log_2 N \cdot f_s / H$.

| $K$ bandas | Operaciones por segundo | Fracción aproximada de un núcleo moderno |
|---|---|---|
| 8 | $8 \times 2048 \times 11 \times 187{,}5 \approx 34 \cdot 10^6$ | menos del 1 % |
| 32 | $\approx 135 \cdot 10^6$ | del 1 al 3 % |
| 128 | $\approx 540 \cdot 10^6$ | del 5 al 10 % |

FFTW en precisión simple con SIMD supera con holgura estas cifras. El presupuesto de la versión 2.0 (procesado por debajo del 1 % de un núcleo) se mantiene hasta 8 bandas y se sube moderadamente hasta 32.

## 7. Resolución variable: transformada Q constante

### 7.1 Motivación

El oído y la música son logarítmicos en frecuencia: una octava entre 40 y 80 Hz ocupa 40 Hz; entre 5 y 10 kHz, 5000 Hz. Con una FFT de anchura fija, los graves quedan cubiertos por muy pocos bins (una octava grave, 2 bins; una aguda, 213) y los agudos por muchos más de los necesarios. La sección 5 impide afinar los graves sin alargar la ventana, pero no obliga a alargarla también para los agudos.

### 7.2 Definición

Se fija el factor de calidad $Q = f_k / \Delta f_k$, constante para todos los bins. Con $b$ bins por octava, la separación entre bins es $f_{k+1} = 2^{1/b} f_k$ y

$$Q = \frac{1}{2^{1/b} - 1}, \qquad N_k = Q \frac{f_s}{f_k}$$

Cada bin usa una ventana de longitud propia $N_k$, larga en graves y corta en agudos.

### 7.3 Consecuencias numéricas

Con $b = 12$ (un bin por semitono), $Q \approx 16{,}8$:

| Frecuencia $f_k$ | $N_k$ a 48 kHz | Duración de la ventana | Latencia añadida al centro |
|---|---|---|---|
| 40 Hz | 20 200 | 420 ms | 210 ms |
| 100 Hz | 8 100 | 168 ms | 84 ms |
| 440 Hz | 1 830 | 38 ms | 19 ms |
| 2 kHz | 400 | 8,4 ms | 4,2 ms |
| 10 kHz | 80 | 1,7 ms | 0,8 ms |

Los graves ganan resolución de semitono a cambio de un retraso de dos décimas de segundo. Los agudos responden en milisegundos. Es una decisión estética legítima siempre que se sepa que las trazas graves van retrasadas respecto a las agudas, y que ese retraso es consecuencia de 5.1, no un defecto de implementación.

### 7.4 Implementación práctica en tiempo real

La CQT exacta requiere una FFT por bin. La aproximación habitual en tiempo real es una pirámide de tres o cuatro resoluciones: FFT de 8192 para el rango de 20 a 250 Hz, de 2048 para 250 a 2000 Hz y de 512 por encima. Cada rango toma su espectro de la FFT que le da una resolución cercana a la Q deseada. El coste es el de tres FFT en lugar de una, todavía marginal. La reconstrucción de bandas de la sección 4 sigue siendo válida dentro de cada resolución.

## 8. Alternativa: bancos de filtros en el dominio del tiempo

### 8.1 Planteamiento

En lugar de enmascarar en frecuencia, se filtra la señal con $K$ filtros paso banda contiguos en el dominio del tiempo. Un diseño clásico es el divisor de Linkwitz-Riley de cuarto orden: en cada frecuencia de corte, la suma del paso bajo y el paso alto tiene magnitud exactamente plana y fase de paso todo. La suma de las bandas reproduce la señal original en magnitud, con una distorsión de fase que no afecta a la visualización.

### 8.2 Coste

Cada filtro de cuarto orden son dos secciones bicuadráticas, unas 10 operaciones por muestra. Para $K = 32$ bandas: $32 \times 2 \times 10 \times 48000 \approx 31 \cdot 10^6$ operaciones por segundo. Comparable a la ISTFT de 8 bandas de la sección 6.4.

### 8.3 Comparación

| Criterio | Enmascaramiento STFT (sección 4) | Banco de filtros IIR |
|---|---|---|
| Alineación temporal entre bandas | Exacta, todas comparten la ventana | Cada banda tiene su propio retardo de grupo, mayor en graves |
| Cambio de bandas en caliente | Recalcular máscaras, sin estado | Rediseñar coeficientes y reiniciar estado del filtro |
| Reutiliza la FFT que ya se calcula | Sí | No, camino independiente |
| Latencia | La de la ventana, 21 ms al centro | La del retardo de grupo, de 1 a 20 ms según banda |
| Resolución en graves | Limitada por $N$ (sección 5) | Limitada por el orden del filtro, transiciones suaves |
| Adecuado para | Espectrograma, ondas por banda alineadas, mapas de energía | Osciloscopio con mínima latencia, medidores por banda |

Recomendación: implementar primero el enmascaramiento STFT porque reutiliza la infraestructura existente y garantiza el teorema 4.3. Dejar el banco de filtros como segunda vía si se quiere una traza grave con menos latencia.

## 9. Qué pasaría si se implementa

### 9.1 Latencia

No cambia respecto a la versión 2.0 mientras se mantenga $N = 2048$: la onda de cada banda sale de la misma ventana que el espectro. Si se adopta resolución variable (sección 7), las bandas graves añaden hasta 200 ms. Debe mostrarse en la interfaz como una propiedad del modo, no ocultarse.

### 9.2 CPU y memoria

Procesado: del 1 al 3 % de un núcleo con 32 bandas (6.4). Memoria: 2 MB para un historial de 256 tramas complejas, más $K \times N$ floats para las ondas de las bandas activas. Transferencia a GPU: 1,5 MB/s si se sube la STFT completa como textura, una fracción mínima del ancho de banda PCIe.

### 9.3 Comportamiento visual esperado

- El osciloscopio apilado mostrará $K$ trazas exactamente alineadas y una traza suma idéntica a la señal original. Un golpe de bombo aparece simultáneamente en la traza grave y en la suma.
- Con máscaras binarias, cada transitorio irá acompañado en su banda de un pequeño rizado previo y posterior (sección 4.4). Con máscaras suaves de dos bins, el rizado es imperceptible.
- Al cambiar la definición de bandas en caliente, las ondas cambian de forma en la siguiente trama. No hay clics ni estados transitorios porque la ISTFT no tiene memoria más allá del solapamiento (unas 43 ms).
- Al dibujar una onda de 2048 muestras en una traza de, por ejemplo, 400 píxeles, hay que reducir 5 muestras a un píxel. Tomar una de cada cinco produce aliasing visual (la traza parpadea). Debe dibujarse el mínimo y el máximo de cada grupo, como hacen los osciloscopios digitales.

### 9.4 Nuevas visualizaciones que habilita

Con la STFT compleja y las bandas disponibles como datos, cada visualización es un shader que lee una textura:

- Osciloscopio apilado por bandas, con la suma debajo.
- Osciloscopio en modo XY o Lissajous entre dos bandas.
- Dinámica por banda: ataque y caída distintos para graves, medios y agudos, que hoy son dos escalares globales.
- Detección de golpes por flujo espectral, $SF_m = \sum_k \max(0, |X_m[k]| - |X_{m-1}[k]|)$, para disparar eventos visuales.
- Fase por bin, hoy descartada, para efectos de coherencia y para estimar la frecuencia exacta de un pico entre bins (interpolación por diferencia de fase, precisión muy por debajo del ancho de bin sin alargar la ventana).
- Cromagrama: energía plegada a las 12 clases de altura, si se adopta la resolución de la sección 7.

### 9.5 Riesgos

| Riesgo | Causa | Mitigación |
|---|---|---|
| Rizado visible en las ondas por banda | Máscaras binarias (4.4) | Rampas de coseno alzado de 1 a 2 bins |
| Expectativa de "ver cada frecuencia" | Malinterpretar la sección 5 | Documentar el compromiso y exponer $N$ como parámetro con su latencia asociada |
| Explosión de datos | Reconstruir ondas para todos los bins (6.3) | Reconstruir solo bandas visibles; la STFT compleja es la representación canónica |
| Desajuste entre trazas graves y agudas | Resolución variable (7.3) | Indicar el retardo por banda en la interfaz o compensarlo retrasando las bandas rápidas |
| Reconstrucción no exacta | Ventana simétrica en lugar de periódica (3.3) | Cambiar el denominador a $N$ al implementar la ISTFT |

## 10. Lo que esta técnica no hace

Separar por frecuencia no es separar por fuente. Un bajo eléctrico y un bombo comparten el rango de 40 a 120 Hz; una voz y una guitarra comparten los medios. La descomposición de este documento asigna cada componente a una banda por su frecuencia, no por el instrumento que la produjo. La separación de fuentes (voz, batería, bajo, resto) es un problema distinto, que hoy se resuelve con redes neuronales entrenadas (por ejemplo, arquitecturas de tipo Demucs) con latencias de segundos y cargas de cómputo incompatibles con un visualizador a 144 fps. Queda fuera del alcance de este proyecto.

## 11. Referencias

- Oppenheim, A. V. y Schafer, R. W. *Discrete-Time Signal Processing*, 3.ª ed. Capítulos 8 (DFT) y 10 (análisis de Fourier de señales mediante la DFT).
- Gabor, D. "Theory of Communication". *Journal of the IEE*, 93(26), 1946. Origen del principio de incertidumbre tiempo-frecuencia.
- Allen, J. B. y Rabiner, L. R. "A Unified Approach to Short-Time Fourier Analysis and Synthesis". *Proceedings of the IEEE*, 65(11), 1977. Condiciones de reconstrucción perfecta por solapamiento y suma.
- Griffin, D. y Lim, J. "Signal Estimation from Modified Short-Time Fourier Transform". *IEEE Trans. ASSP*, 32(2), 1984. Reconstrucción con ventana de síntesis y normalización por $\sum w^2$.
- Brown, J. C. "Calculation of a Constant Q Spectral Transform". *JASA*, 89(1), 1991. Definición de la CQT.
- Linkwitz, S. "Active Crossover Networks for Noncoincident Drivers". *JAES*, 24(1), 1976. Divisores de suma plana.
- Harris, F. J. "On the Use of Windows for Harmonic Analysis with the Discrete Fourier Transform". *Proceedings of the IEEE*, 66(1), 1978. Propiedades de las ventanas, incluida Hann.
