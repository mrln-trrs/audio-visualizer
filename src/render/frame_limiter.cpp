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

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace render {
namespace {

// Margen que se remata cediendo el procesador tras despertar del temporizador.
constexpr double kSpinMarginSeconds = 0.0005;

} // namespace

double FrameLimiter::PeriodFor(int max_fps, int monitor_hz) {
    const int hz = max_fps > 0 ? max_fps : (monitor_hz > 0 ? monitor_hz : 144);
    return 1.0 / hz;
}

void FrameLimiter::Start(bool vsync, int max_fps, int monitor_hz, double now) {
    timeBeginPeriod(1);
    // Windows 10 1803 o superior. Si no existe, timer_ queda nulo y se usa sleep por sondeo.
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer_) {
        std::cout << "Render: temporizador de alta resolucion no disponible, se usa espera por sondeo." << std::endl;
    }
    frame_period_ = PeriodFor(max_fps, monitor_hz);
    active_ = !vsync; // sin vsync el limitador actúa desde el principio
    next_deadline_ = now + frame_period_;
}

void FrameLimiter::Stop() {
    if (timer_) {
        CloseHandle(static_cast<HANDLE>(timer_));
        timer_ = nullptr;
    }
    timeEndPeriod(1);
}

void FrameLimiter::SleepUntil(double deadline) {
    double t = glfwGetTime();
    const double wait = deadline - kSpinMarginSeconds - t;
    if (wait > 0.0) {
        if (timer_) {
            // Tiempo relativo en unidades de 100 ns, negativo por convención de la API.
            LARGE_INTEGER due;
            due.QuadPart = -static_cast<LONGLONG>(wait * 1e7);
            if (SetWaitableTimer(static_cast<HANDLE>(timer_), &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(static_cast<HANDLE>(timer_), INFINITE);
            }
        }
        else {
            while (glfwGetTime() < deadline - 0.002) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        t = glfwGetTime();
    }
    // Remate fino: como mucho medio milisegundo cediendo el procesador.
    while (t < deadline) {
        std::this_thread::yield();
        t = glfwGetTime();
    }
}

void FrameLimiter::WaitForNextFrame() {
    if (!active_) return;
    SleepUntil(next_deadline_);
    // Plazo anclado al anterior para no acumular deriva; si vamos muy tarde (ventana arrastrada,
    // tirón), se resincroniza en vez de correr para alcanzar.
    const double t = glfwGetTime();
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
