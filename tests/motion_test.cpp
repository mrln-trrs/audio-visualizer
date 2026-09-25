// Prueba de las transiciones de Fluent (docs/12, sección 7; docs/04, historia 7.3):
//   1. La curva de deceleración empieza en 0, termina en 1 y es monótona.
//   2. Una Transition de 150 ms alcanza el objetivo exactamente a los 150 ms y no antes de los
//      140 ms, con independencia de la cadencia con que se consulte (60, 144 o 240 fps).
//   3. Con las animaciones desactivadas el cambio es instantáneo.
// Verifica la lógica de tiempo real que gobierna el fundido entre modos y la apertura del panel.

#include <cstdio>
#include <cmath>
#include <string>

#include "ui/motion.h"

namespace {
int failures = 0;
void Check(bool ok, const std::string& what, double got, double expected) {
    std::printf("%s %-64s obtenido=%.6g esperado=%.6g\n", ok ? "[OK]  " : "[FALLO]", what.c_str(), got, expected);
    if (!ok) ++failures;
}
}

int main() {
    Check(ui::EaseDecelerate(0.0f) == 0.0f, "EaseDecelerate(0) = 0", ui::EaseDecelerate(0.0f), 0.0);
    Check(ui::EaseDecelerate(1.0f) == 1.0f, "EaseDecelerate(1) = 1", ui::EaseDecelerate(1.0f), 1.0);
    bool monotonic = true;
    for (int i = 1; i <= 100; ++i) {
        if (ui::EaseDecelerate(i / 100.0f) < ui::EaseDecelerate((i - 1) / 100.0f)) monotonic = false;
    }
    Check(monotonic, "EaseDecelerate monotona creciente", monotonic ? 1 : 0, 1);
    // Deceleracion: a mitad del tiempo ya ha recorrido mas de la mitad del camino.
    Check(ui::EaseDecelerate(0.5f) > 0.8f, "EaseDecelerate(0.5) > 0.8 (sale rapido y frena)", ui::EaseDecelerate(0.5f), 0.875);

    for (int fps : { 60, 144, 240 }) {
        ui::Transition t;
        t.Configure(0.150, true);
        double now = 10.0;
        t.SetTarget(true, now);
        const double step = 1.0 / fps;
        double reached_at = -1.0;
        double at_140 = -1.0;
        for (int frame = 1; frame <= fps; ++frame) {
            now += step;
            const float v = t.value(now);
            // "Terminada" es valor exactamente 1: la curva se acerca a 1 antes (1 - (1-t)^3 supera
            // 0,999 a los 135 ms) pero solo lo alcanza al cumplirse la duracion.
            if (reached_at < 0.0 && v >= 1.0f) reached_at = now - 10.0;
        }
        Check(reached_at > 0.0 && std::fabs(reached_at - 0.150) <= step + 1e-9, "transicion de 150 ms completa a los 150 ms (+-1 cuadro) a " + std::to_string(fps) + " fps", reached_at, 0.150);
        // La funcion de valor se evalua en t = 140 ms exactos (a 60 fps ningun cuadro cae ahi).
        at_140 = t.value(10.0 + 0.140);
        Check(at_140 < 1.0f, "a los 140 ms la transicion aun no ha terminado a " + std::to_string(fps) + " fps", at_140, 0.9997);
    }

    ui::Transition instant;
    instant.Configure(0.150, false);
    instant.SetTarget(true, 5.0);
    Check(instant.value(5.0) == 1.0f, "con animaciones desactivadas el valor salta al objetivo", instant.value(5.0), 1.0);

    // Cambio de objetivo a mitad de camino: arranca desde el valor actual, sin saltos.
    ui::Transition back;
    back.Configure(0.200, true);
    back.SetTarget(true, 0.0);
    const float mid = back.value(0.1);
    back.SetTarget(false, 0.1);
    Check(std::fabs(back.value(0.1) - mid) < 1e-6f, "invertir la transicion no produce salto", back.value(0.1), mid);

    std::printf("\n%s: %d fallo(s)\n", failures == 0 ? "PRUEBA SUPERADA" : "PRUEBA FALLIDA", failures);
    return failures == 0 ? 0 : 1;
}
