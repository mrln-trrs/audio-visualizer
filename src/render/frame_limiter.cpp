#include "render/frame_limiter.h"

#include <iostream>
#include <thread>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#include <GLFW/glfw3.h>

#pragma comment(lib, "winmm.lib")

namespace render {

double FrameLimiter::PeriodFor(int max_fps, int monitor_hz) {
    const int hz = max_fps > 0 ? max_fps : (monitor_hz > 0 ? monitor_hz : 144);
    return 1.0 / hz;
}

void FrameLimiter::Start(bool vsync, int max_fps, int monitor_hz, double now) {
    timeBeginPeriod(1);
    frame_period_ = PeriodFor(max_fps, monitor_hz);
    active_ = !vsync; // sin vsync el limitador actúa desde el principio
    next_deadline_ = now + frame_period_;
}

void FrameLimiter::Stop() {
    timeEndPeriod(1);
}

void FrameLimiter::WaitForNextFrame() {
    if (!active_) return;
    // Dormir en pasos de 1 ms hasta ~2 ms antes del plazo y afinar el resto en espera activa.
    double t = glfwGetTime();
    while (t < next_deadline_ - 0.002) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        t = glfwGetTime();
    }
    while (t < next_deadline_) {
        std::this_thread::yield();
        t = glfwGetTime();
    }
    // Plazo anclado al anterior para no acumular deriva; si vamos muy tarde (ventana arrastrada,
    // tirón), se resincroniza en vez de correr para alcanzar.
    next_deadline_ += frame_period_;
    if (t > next_deadline_ + frame_period_) next_deadline_ = t + frame_period_;
}

void FrameLimiter::UpdateEverySecond(double measured_fps, int monitor_hz, int max_fps, double now) {
    frame_period_ = PeriodFor(max_fps, monitor_hz);
    if (!active_ && monitor_hz > 0 && measured_fps > monitor_hz * 1.5) {
        active_ = true;
        next_deadline_ = now + frame_period_;
        std::cout << "Render: vsync ignorado por el driver (" << static_cast<int>(measured_fps)
                  << " fps), limitador activado a " << static_cast<int>(1.0 / frame_period_) << " fps." << std::endl;
    }
}

} // namespace render
