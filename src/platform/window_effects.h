#pragma once

#include <string>

struct GLFWwindow;

// Efectos de ventana del sistema (Windows 11) y preferencias de accesibilidad del usuario.
// Fundamento y valores de los atributos: docs/12_fluent_design_ui.md, secciones 2 y 10.
namespace platform {

enum class SystemBackdrop { None, Mica, Acrylic };

struct WindowEffectsResult {
    bool dark_frame = false;       // marco y barra de título oscuros
    bool rounded_corners = false;
    bool backdrop = false;         // material del sistema aplicado
    std::string message;           // motivo si algo no se pudo aplicar
};

// Aplica marco oscuro, esquinas redondeadas y, si se pide, un material del sistema. Si el sistema
// no lo soporta (Windows 10, DWM sin el atributo) la ventana queda opaca y se devuelve el motivo.
// Para ver el material del sistema la ventana debe tener framebuffer transparente y limpiar con
// alfa cero en las zonas donde debe verse.
WindowEffectsResult ApplyWindowEffects(GLFWwindow* window, SystemBackdrop backdrop);

// Preferencias del sistema: efectos de transparencia y animaciones (Configuración de Windows).
struct SystemEffectsPreference {
    bool transparency = true;
    bool animations = true;
};
SystemEffectsPreference ReadSystemEffectsPreference();

// True en Windows 11 22H2 (build 22621) o superior, donde existe DWMWA_SYSTEMBACKDROP_TYPE.
bool SupportsSystemBackdrop();

} // namespace platform
