#pragma once

#include <algorithm>

// Curvas y transiciones de Fluent Design (docs/12, sección 7). Todo se evalúa con el tiempo real
// transcurrido, nunca por cuadro, para que dure lo mismo a 60 o a 144 fps.
namespace ui {

// Deceleración: sale rápido y frena al llegar. Aproximación cúbica de cubic-bezier(0.1, 0.9, 0.2, 1).
inline float EaseDecelerate(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// Estándar: acelera y frena. Aproximación de cubic-bezier(0.8, 0, 0.2, 1).
inline float EaseStandard(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - 4.0f * (1.0f - t) * (1.0f - t) * (1.0f - t);
}

// Transición entre dos estados con duración fija. `value()` va de 0 a 1 (o de 1 a 0) siguiendo
// la curva de deceleración; si las animaciones están desactivadas, salta al instante.
class Transition {
public:
    void Configure(double duration_seconds, bool enabled) { duration_ = duration_seconds; enabled_ = enabled; }

    // Fija el objetivo (true = 1, false = 0). Si cambia, arranca la animación desde el valor actual.
    void SetTarget(bool on, double now) {
        if (on == target_) return;
        from_ = value(now);
        target_ = on;
        start_ = now;
    }

    float value(double now) const {
        const float to = target_ ? 1.0f : 0.0f;
        if (!enabled_ || duration_ <= 0.0) return to;
        const float t = static_cast<float>(std::clamp((now - start_) / duration_, 0.0, 1.0));
        return from_ + (to - from_) * EaseDecelerate(t);
    }

    bool active(double now) const { return enabled_ && duration_ > 0.0 && (now - start_) < duration_; }
    bool target() const { return target_; }

private:
    double duration_ = 0.2;
    bool enabled_ = true;
    bool target_ = false;
    float from_ = 0.0f;
    double start_ = -1e9;
};

} // namespace ui
