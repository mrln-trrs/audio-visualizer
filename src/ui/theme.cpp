#include "ui/theme.h"

#include <imgui.h>

namespace ui {

void HelpMarker(const char* description) {
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
        ImGui::TextUnformatted(description);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void ApplyObsidianTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;

    style.WindowPadding = ImVec2(14, 14);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.ScrollbarSize = 14.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.98f, 1.00f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.55f, 0.65f, 1.00f);
    c[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.13f, 0.92f);
    c[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.07f, 0.10f, 0.70f);
    c[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.13f, 0.96f);
    c[ImGuiCol_Border] = ImVec4(0.20f, 0.25f, 0.35f, 0.50f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.14f, 0.20f, 0.90f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.22f, 0.32f, 0.95f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.19f, 0.28f, 1.00f);
    c[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.08f, 0.12f, 0.95f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.14f, 0.22f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.06f, 0.08f, 0.75f);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.10f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.06f, 0.08f, 0.60f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.28f, 0.38f, 0.80f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.38f, 0.52f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.46f, 0.62f, 1.00f);
    c[ImGuiCol_CheckMark] = ImVec4(0.25f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.22f, 0.62f, 0.96f, 0.90f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    c[ImGuiCol_Button] = ImVec4(0.14f, 0.20f, 0.32f, 0.85f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.32f, 0.50f, 1.00f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
    c[ImGuiCol_Header] = ImVec4(0.14f, 0.20f, 0.32f, 0.70f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.22f, 0.32f, 0.50f, 0.90f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.16f, 0.24f, 0.38f, 1.00f);
    c[ImGuiCol_Separator] = ImVec4(0.20f, 0.24f, 0.34f, 0.60f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.25f, 0.35f, 0.50f, 0.80f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.30f, 0.45f, 0.65f, 1.00f);
    c[ImGuiCol_Tab] = ImVec4(0.10f, 0.12f, 0.18f, 0.90f);
    c[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.32f, 0.50f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.16f, 0.25f, 0.40f, 1.00f);
    c[ImGuiCol_TabDimmed] = ImVec4(0.08f, 0.09f, 0.14f, 0.90f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.12f, 0.18f, 0.28f, 1.00f);
}

} // namespace ui
