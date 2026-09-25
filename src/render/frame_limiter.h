#pragma once

// Limitador de cuadros adaptativo. Algunos drivers ignoran glfwSwapInterval(1) y el bucle corre a
// miles de fps quemando un núcleo. Se activa solo si se detecta ese caso (fps medidos muy por
// encima del refresco del monitor) y entonces clava el bucle al refresco o a max_fps.
namespace render {

class FrameLimiter {
public:
    // `now` en segundos (glfwGetTime). Activa timeBeginPeriod(1) para que los sleeps sean de 1 ms.
    void Start(bool vsync, int max_fps, int monitor_hz, double now);
    void Stop();

    // Llamar tras glfwSwapBuffers. Duerme hasta el plazo del cuadro si está activo.
    void WaitForNextFrame();

    // Llamar una vez por segundo con los fps medidos. Recalcula el periodo y decide si activarse.
    void UpdateEverySecond(double measured_fps, int monitor_hz, int max_fps, double now);

    bool active() const { return active_; }
    double frame_period() const { return frame_period_; }

private:
    static double PeriodFor(int max_fps, int monitor_hz);

    bool active_ = false;
    double frame_period_ = 1.0 / 144.0;
    double next_deadline_ = 0.0;
};

} // namespace render
