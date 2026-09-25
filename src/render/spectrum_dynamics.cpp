#include "render/spectrum_dynamics.h"

#include <algorithm>
#include <cmath>

namespace render {

void SpectrumDynamics::Update(const std::vector<float>& spectrum, int num_bars, const core::VisualizerConfig& cfg, float dt) {
    if (static_cast<int>(heights_.size()) != num_bars) {
        heights_.assign(num_bars, 0.0f);
        peaks_.assign(num_bars, 0.0f);
        peak_timers_.assign(num_bars, 0.0f);
    }

    // Coeficientes dependientes del tiempo real: misma respuesta a 60, 144 o 240 fps.
    const float a_up = 1.0f - std::exp(-dt * 1000.0f / std::max(0.1f, cfg.attack_ms));
    const float a_down = 1.0f - std::exp(-dt * 1000.0f / std::max(0.1f, cfg.release_ms));
    const float hold_sec = cfg.peak_hold_time_ms * 0.001f;
    const int available = static_cast<int>(spectrum.size());

    for (int i = 0; i < num_bars; ++i) {
        const float raw = (i < available) ? spectrum[i] : 0.0f;
        const float target = std::clamp(raw * cfg.amplitude_factor, 0.0f, 1.0f);

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
