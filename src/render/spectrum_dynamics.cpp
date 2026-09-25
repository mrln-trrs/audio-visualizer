#include "render/spectrum_dynamics.h"

#include <algorithm>
#include <cmath>

namespace render {
namespace {

inline float Coefficient(float dt, float tau_ms) {
    return 1.0f - std::exp(-dt * 1000.0f / std::max(0.1f, tau_ms));
}

} // namespace

void SpectrumDynamics::Update(const std::vector<float>& values, int count, const core::VisualizerConfig& cfg, float dt) {
    Update(values, count, cfg, nullptr, nullptr, dt);
}

void SpectrumDynamics::Update(const std::vector<float>& values, int count, const core::VisualizerConfig& cfg,
                              const float* attack_ms, const float* release_ms, float dt) {
    if (static_cast<int>(heights_.size()) != count) {
        heights_.assign(count, 0.0f);
        peaks_.assign(count, 0.0f);
        peak_timers_.assign(count, 0.0f);
    }

    // Coeficientes dependientes del tiempo real: misma respuesta a 60, 144 o 240 fps.
    const float a_up_global = Coefficient(dt, cfg.attack_ms);
    const float a_down_global = Coefficient(dt, cfg.release_ms);
    const float hold_sec = cfg.peak_hold_time_ms * 0.001f;
    const int available = static_cast<int>(values.size());

    for (int i = 0; i < count; ++i) {
        const float raw = (i < available) ? values[i] : 0.0f;
        const float target = std::clamp(raw * cfg.amplitude_factor, 0.0f, 1.0f);
        const float a_up = attack_ms ? Coefficient(dt, attack_ms[i]) : a_up_global;
        const float a_down = release_ms ? Coefficient(dt, release_ms[i]) : a_down_global;

        float& h = heights_[i];
        h += (target - h) * (target > h ? a_up : a_down);

        float& p = peaks_[i];
        float& timer = peak_timers_[i];
        if (h >= p) {
            p = h;
            timer = hold_sec;
        }
        else if (timer > 0.0f) {
            timer -= dt;
        }
        else {
            p = std::max(h, p - cfg.peak_decay_speed * dt);
        }
    }
}

} // namespace render
