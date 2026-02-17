#pragma once

#include "imgui.h"

namespace prisme_theme {

    inline void apply() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Rounding
        style.WindowRounding    = 6.0f;
        style.FrameRounding     = 4.0f;
        style.GrabRounding      = 3.0f;
        style.TabRounding       = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.ChildRounding     = 4.0f;
        style.PopupRounding     = 4.0f;

        // Borders
        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 0.0f;
        style.TabBorderSize     = 0.0f;

        // Spacing
        style.WindowPadding     = ImVec2(12, 12);
        style.FramePadding      = ImVec2(8, 4);
        style.ItemSpacing       = ImVec2(8, 6);
        style.ItemInnerSpacing  = ImVec2(6, 4);
        style.IndentSpacing     = 20.0f;
        style.ScrollbarSize     = 12.0f;
        style.GrabMinSize       = 10.0f;

        // Colors
        ImVec4 bg_dark       = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        ImVec4 bg_mid        = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        ImVec4 bg_light      = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
        ImVec4 border        = ImVec4(0.25f, 0.22f, 0.35f, 0.60f);

        ImVec4 purple        = ImVec4(0.54f, 0.39f, 0.82f, 1.00f);
        ImVec4 purple_hover  = ImVec4(0.62f, 0.47f, 0.90f, 1.00f);
        ImVec4 purple_active = ImVec4(0.45f, 0.32f, 0.72f, 1.00f);

        ImVec4 teal          = ImVec4(0.25f, 0.75f, 0.71f, 1.00f);

        ImVec4 text          = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
        ImVec4 text_dim      = ImVec4(0.60f, 0.58f, 0.65f, 1.00f);

        // Window
        colors[ImGuiCol_WindowBg]             = bg_dark;
        colors[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.10f, 0.13f, 0.95f);

        // Title
        colors[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.08f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgActive]        = ImVec4(0.28f, 0.20f, 0.45f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.10f, 0.08f, 0.16f, 0.60f);

        // Border
        colors[ImGuiCol_Border]               = border;
        colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // Frame
        colors[ImGuiCol_FrameBg]              = bg_mid;
        colors[ImGuiCol_FrameBgHovered]       = bg_light;
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.18f, 0.28f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab]                  = ImVec4(0.18f, 0.15f, 0.28f, 1.00f);
        colors[ImGuiCol_TabHovered]           = purple_hover;
        colors[ImGuiCol_TabSelected]          = purple;
        colors[ImGuiCol_TabSelectedOverline]  = teal;
        colors[ImGuiCol_TabDimmed]            = ImVec4(0.12f, 0.10f, 0.18f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.30f, 0.22f, 0.50f, 1.00f);

        // Buttons
        colors[ImGuiCol_Button]               = ImVec4(0.22f, 0.18f, 0.35f, 1.00f);
        colors[ImGuiCol_ButtonHovered]        = purple_hover;
        colors[ImGuiCol_ButtonActive]         = purple_active;

        // Checkmark + slider
        colors[ImGuiCol_CheckMark]            = teal;
        colors[ImGuiCol_SliderGrab]           = purple;
        colors[ImGuiCol_SliderGrabActive]     = purple_hover;

        // Header
        colors[ImGuiCol_Header]               = ImVec4(0.22f, 0.18f, 0.35f, 0.60f);
        colors[ImGuiCol_HeaderHovered]        = purple_hover;
        colors[ImGuiCol_HeaderActive]         = purple;

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.06f, 0.08f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.28f, 0.40f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = purple;
        colors[ImGuiCol_ScrollbarGrabActive]  = purple_hover;

        // Separator
        colors[ImGuiCol_Separator]            = border;
        colors[ImGuiCol_SeparatorHovered]     = purple;
        colors[ImGuiCol_SeparatorActive]      = purple_hover;

        // Resize grip
        colors[ImGuiCol_ResizeGrip]           = ImVec4(0.22f, 0.18f, 0.35f, 0.40f);
        colors[ImGuiCol_ResizeGripHovered]    = purple;
        colors[ImGuiCol_ResizeGripActive]     = purple_hover;

        // Text
        colors[ImGuiCol_Text]                 = text;
        colors[ImGuiCol_TextDisabled]         = text_dim;

        // Misc
        colors[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.54f, 0.39f, 0.82f, 0.35f);
        colors[ImGuiCol_NavHighlight]         = purple;
    }

} // namespace prisme_theme
