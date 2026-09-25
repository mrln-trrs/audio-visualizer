#include "app/application.h"

#include <thread>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "core/shared_state.h"
#include "core/config.h"
#include "audio/capture_thread.h"
#include "analysis/analysis_thread.h"
#include "render/renderer.h"

namespace app {

int Run() {
    // Ocultar la consola. Comentar esta línea para ver los mensajes de diagnóstico.
    FreeConsole();

    core::AudioData audio;
    core::VisualizerData vis;
    core::SharedConfigData config;

    // config.json se copia a la carpeta de salida en el post-build y el depurador arranca con
    // esa carpeta como directorio de trabajo.
    core::LoadConfig(config, "config.json");

    // Dispositivo guardado en config.json. El hilo de captura lo resuelve a un id WASAPI tras
    // enumerar; si ya no existe, usa el predeterminado.
    {
        std::lock_guard<std::mutex> lock(vis.dev_mtx);
        vis.requested_device_name = config.config.selected_device_name;
    }

    std::thread capture(audio::CaptureThread, std::ref(audio), std::ref(vis));
    std::thread analysis(analysis::AnalysisThread, std::ref(audio), std::ref(vis));

    // El render corre en este hilo hasta que se cierra la ventana.
    render::RenderThread(vis, config, audio);

    // Pedir el cierre. El flag se cambia con el mutex tomado para que el hilo de análisis no
    // pueda perder la notificación entre comprobar el predicado y dormirse.
    {
        std::lock_guard<std::mutex> lock(audio.mtx);
        vis.should_terminate.store(true);
    }
    audio.cv.notify_all();

    capture.join();
    analysis.join();
    return 0;
}

} // namespace app
