#include "renderer.h"
#include <iostream>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <cmath>
#include <atomic>
#include "config.h"

// Factor para el espacio entre las barras, como un porcentaje del ancho de la barra.
// Un valor de 0.1 significa un espacio del 10% del ancho de la barra.
const float BAR_GAP_FACTOR = 0.1f;

// Almacenamos las alturas actuales de las barras para implementar el decaimiento.
static std::vector<double> current_heights;
// Almacenamos las alturas suavizadas para el efecto "ola".
static std::vector<double> smoothed_heights;
// Punteros a los datos compartidos.
static VisualizerData* sharedVisualizerDataPtr = nullptr;

// Función de callback para redimensionar la ventana.
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // Redimensionar el viewport de OpenGL para que coincida con la nueva ventana.
    glViewport(0, 0, width, height);

    if (sharedVisualizerDataPtr) {
        int new_num_bars = width;
        sharedVisualizerDataPtr->atomic_num_bars.store(new_num_bars);

        // Bloquear el mutex del búfer de visualización para redimensionar de forma segura.
        std::unique_lock<std::mutex> lock(sharedVisualizerDataPtr->mtx);

        // Redimensionar los búferes de datos compartidos para que coincidan con el nuevo ancho.
        sharedVisualizerDataPtr->out_data[0].resize(new_num_bars, 0.0);
        sharedVisualizerDataPtr->out_data[1].resize(new_num_bars, 0.0);

        // Redimensionar los búferes de decaimiento y suavizado.
        current_heights.resize(new_num_bars, 0.0);
        smoothed_heights.resize(new_num_bars, 0.0);

        // El hilo de procesamiento se encargará de redimensionar sus propios búferes.
        // Aquí no es necesario hacer nada más, ya que el siguiente ciclo de procesamiento
        // usará el nuevo valor de 'atomic_num_bars'.
    }
}

// The main rendering thread function
void RenderThread(VisualizerData& sharedVisualizerData, SharedConfigData& sharedConfigData) {
    std::cout << "Rendering thread started." << std::endl;
    // Asignar el puntero para que la función de callback pueda acceder a los datos.
    sharedVisualizerDataPtr = &sharedVisualizerData;

    // Inicializar los vectores de altura.
    const int initial_width = 1024;
    current_heights.resize(initial_width, 0.0);
    smoothed_heights.resize(initial_width, 0.0);

    // Obtener los factores de configuración de forma segura.
    std::unique_lock<std::mutex> config_lock(sharedConfigData.mtx);
    const float decay_factor = sharedConfigData.config.decay_factor;
    const float smoothing_factor = sharedConfigData.config.smoothing_factor;
    const float amplitude_factor = sharedConfigData.config.amplitude_factor;
    const std::vector<float> base_color = sharedConfigData.config.base_color_rgb;
    config_lock.unlock();

    // Initialize GLFW.
    if (!glfwInit()) {
        std::cerr << "Error: Failed to initialize GLFW." << std::endl;
        return;
    }

    // Create a windowed mode window and its OpenGL context.
    GLFWwindow* window = glfwCreateWindow(initial_width, 600, "Audio Visualizer", NULL, NULL);
    if (!window) {
        std::cerr << "Error: Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return;
    }

    // Make the window's context current.
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync to synchronize with the monitor's refresh rate.

    // Registrar la función de callback de redimensionamiento.
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Main rendering loop
    while (!glfwWindowShouldClose(window)) {
        // Poll for and process events.
        glfwPollEvents();

        // Clear the screen to a dark gray color.
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Begin drawing quads (rectangles) for the bars.
        glBegin(GL_QUADS);

        // Get the index of the buffer that the processing thread just wrote to
        int read_index = sharedVisualizerData.write_buffer_index.load();
        int current_num_bars = sharedVisualizerData.atomic_num_bars.load();

        // Loop through the data and draw a bar for each frequency bin.
        for (int i = 0; i < current_num_bars; ++i) {
            // Leer los datos procesados del búfer compartido.
            double raw_value = sharedVisualizerData.out_data[read_index][i];

            // Implementar decaimiento: las barras caen gradualmente.
            if (raw_value > current_heights[i]) {
                current_heights[i] = raw_value;
            }
            else {
                current_heights[i] *= decay_factor;
            }

            // Implementar suavizado: las barras se mueven en un efecto "ola".
            smoothed_heights[i] = (current_heights[i] * smoothing_factor) + (smoothed_heights[i] * (1.0 - smoothing_factor));

            // Aplicar el factor de amplitud.
            double bar_height_normalized = smoothed_heights[i] * amplitude_factor;

            // Asegurarse de que el valor está en el rango [0, 1].
            if (bar_height_normalized > 1.0) bar_height_normalized = 1.0;
            if (bar_height_normalized < 0.0) bar_height_normalized = 0.0;

            // Bar dimensions and position.
            double bar_width = 2.0 / current_num_bars;
            double x_position = -1.0 + i * bar_width;

            // Ajustar el ancho de la barra para crear un espacio.
            double adjusted_bar_width = bar_width * (1.0 - BAR_GAP_FACTOR);

            double y_height = bar_height_normalized * 1.5 - 1.0;

            // Set the color of the bar using the base color from config and a gradient.
            const GLfloat h = static_cast<GLfloat>(bar_height_normalized);
            glColor3f(base_color[0] + h * 0.5f,
                base_color[1] - h * 0.5f,
                base_color[2] + h * 0.5f);

            // Draw a quad (rectangle) for the bar.
            const GLfloat x0 = static_cast<GLfloat>(x_position);
            const GLfloat x1 = static_cast<GLfloat>(x_position + adjusted_bar_width);
            const GLfloat y1 = static_cast<GLfloat>(y_height);
            glVertex2f(x0, -1.0f);
            glVertex2f(x1, -1.0f);
            glVertex2f(x1, y1);
            glVertex2f(x0, y1);
        }

        // End drawing.
        glEnd();

        // Swap front and back buffers.
        glfwSwapBuffers(window);
    }

    // Clean up.
    glfwTerminate();
}
