#pragma once

// Limitador de cuadros adaptativo. Algunos drivers ignoran glfwSwapInterval(1) y el bucle corre a
// miles de fps quemando un núcleo. Se activa solo si se detecta ese caso (fps medidos muy por
// encima del refresco del monitor) y entonces clava el bucle al refresco o a max_fps.
//
// La espera usa un temporizador de alta resolución de Windows (precisión de ~0,5 ms sin espera
// activa) y remata el último medio milisegundo cediendo el procesador, de modo que el hilo de
// render no consume un tercio de un núcleo girando como hacía la espera por sondeo.
namespace render {

class FrameLimiter {
public:
    // `now` en segundos (glfwGetTime). Activa timeBeginPeriod(1) y crea el temporizador.
    void Start(bool vsync, int max_fps, int monitor_hz, double now);
    void Stop();

    // Llamar tras glfwSwapBuffers. Duerme hasta el plazo del cuadro si está activo.
    void WaitForNextFrame();

    // Llamar una vez por segundo con los fps medidos. Recalcula el periodo y decide si activarse.
    void UpdateEverySecond(double measured_fps, int monitor_hz, int max_fps, double now);

    bool active() const { return active_; }
    double frame_period() const { return frame_period_; }
    bool high_resolution_timer() const { return timer_ != nullptr; }

private:
    static double PeriodFor(int max_fps, int monitor_hz);
    void SleepUntil(double deadline);

    bool active_ = false;
    double frame_period_ = 1.0 / 144.0;
    double next_deadline_ = 0.0;
    void* timer_ = nullptr; // HANDLE del temporizador de alta resolución, nullptr si no disponible
};

} // namespace render
