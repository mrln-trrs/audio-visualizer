#include <iostream>
#include <thread>
#include <Windows.h> // FreeConsole()
#include "common.h"
#include "audio-capture.h"
#include "audio-processing.h"
#include "renderer.h"
#include "config.h"

int main() {
    // Ocultar la consola. Comentar esta línea para ver los mensajes de diagnóstico.
    FreeConsole();

    AudioData sharedAudioData;
    VisualizerData sharedVisualizerData;
    SharedConfigData sharedConfigData;

    // config.json se copia a la carpeta de salida en el post-build y el depurador
    // arranca con esa carpeta como directorio de trabajo.
    LoadConfig(sharedConfigData, "config.json");

    std::thread audioCaptureThread(AudioCaptureThread, std::ref(sharedAudioData), std::ref(sharedVisualizerData));
    std::thread signalProcessingThread(AudioProcessingThread, std::ref(sharedAudioData), std::ref(sharedVisualizerData), std::ref(sharedConfigData));

    // El renderizado corre en este hilo hasta que se cierra la ventana.
    RenderThread(sharedVisualizerData, sharedConfigData, sharedAudioData);

    // Pedir el cierre. El flag se cambia con el mutex tomado para que el hilo de procesado
    // no pueda perder la notificación entre comprobar el predicado y dormirse.
    {
        std::lock_guard<std::mutex> lock(sharedAudioData.mtx);
        sharedVisualizerData.should_terminate.store(true);
    }
    sharedAudioData.cv.notify_all();

    audioCaptureThread.join();
    signalProcessingThread.join();

    return 0;
}
