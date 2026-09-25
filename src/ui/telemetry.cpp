#include "ui/telemetry.h"

#include <cstdio>

namespace ui {

void Telemetry::Record(float fps, float ups) {
    last_fps = fps;
    last_ups = ups;
    fps_history[history_offset] = fps;
    history_offset = (history_offset + 1) % HISTORY;
}

std::string BuildWindowTitle(const char* mode_name, const Telemetry& t, int sample_rate) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "Audio Visualizer 3.0  |  [%s]  |  %.0f fps (%d Hz%s)  |  %.0f esp/s  |  audio %d Hz",
        mode_name, t.last_fps, t.monitor_hz, t.limiter_active ? ", limitador" : ", vsync", t.last_ups, sample_rate);
    return buf;
}

} // namespace ui
