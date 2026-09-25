#pragma once

struct ImDrawList;

// Interfaz que el HUD usa para pedir un fondo de material detrás de un panel, sin conocer OpenGL.
// La implementación vive en render/panel_material. Si no hay material, el HUD usa el fondo
// opaco normal de Dear ImGui.
namespace ui {

class IPanelMaterial {
public:
    virtual ~IPanelMaterial() = default;
    // Encola el dibujo del material para el rectángulo del panel (coordenadas de ImGui, origen
    // arriba a la izquierda, píxeles). `alpha` atenúa material y sombra durante las transiciones.
    // Debe llamarse justo después de ImGui::Begin.
    virtual void AddPanelBackground(ImDrawList* draw_list, float x, float y, float w, float h, float rounding, float alpha) = 0;
};

} // namespace ui
