#pragma once

#include "imgui.h"

namespace prisme_theme {

    // YeisApp / Prisme brand colors
    // Purple accent: RGB(138, 99, 210) = #8A63D2
    // Teal accent:   RGB(64, 190, 180) = #40BEB4
    // Dark bg:       RGB(18, 18, 24)
    // Card bg:       RGB(28, 28, 38)

    inline ImVec4 accent()     { return ImVec4(0.54f, 0.39f, 0.82f, 1.00f); }  // purple
    inline ImVec4 accent2()    { return ImVec4(0.25f, 0.75f, 0.71f, 1.00f); }  // teal
    inline ImVec4 bg_dark()    { return ImVec4(0.07f, 0.07f, 0.09f, 0.97f); }
    inline ImVec4 bg_card()    { return ImVec4(0.11f, 0.11f, 0.15f, 1.00f); }
    inline ImVec4 bg_sidebar() { return ImVec4(0.09f, 0.09f, 0.12f, 1.00f); }
    inline ImVec4 text_main()  { return ImVec4(0.92f, 0.92f, 0.96f, 1.00f); }
    inline ImVec4 text_dim()   { return ImVec4(0.50f, 0.50f, 0.58f, 1.00f); }
    inline ImVec4 border()     { return ImVec4(0.20f, 0.20f, 0.28f, 0.50f); }

    inline void apply() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Rounding — smooth modern feel
        style.WindowRounding    = 8.0f;
        style.FrameRounding     = 6.0f;
        style.GrabRounding      = 6.0f;
        style.TabRounding       = 4.0f;
        style.ScrollbarRounding = 6.0f;
        style.ChildRounding     = 6.0f;
        style.PopupRounding     = 6.0f;

        // Borders
        style.WindowBorderSize  = 0.0f;
        style.FrameBorderSize   = 0.0f;
        style.TabBorderSize     = 0.0f;

        // Spacing
        style.WindowPadding     = ImVec2(0, 0);
        style.FramePadding      = ImVec2(10, 6);
        style.ItemSpacing       = ImVec2(10, 8);
        style.ItemInnerSpacing  = ImVec2(8, 4);
        style.IndentSpacing     = 20.0f;
        style.ScrollbarSize     = 10.0f;
        style.GrabMinSize       = 10.0f;

        ImVec4 purple        = accent();
        ImVec4 purple_hover  = ImVec4(0.62f, 0.47f, 0.92f, 1.00f);
        ImVec4 purple_active = ImVec4(0.48f, 0.34f, 0.76f, 1.00f);
        ImVec4 teal          = accent2();

        // Window
        colors[ImGuiCol_WindowBg]             = bg_dark();
        colors[ImGuiCol_ChildBg]              = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_PopupBg]              = ImVec4(0.09f, 0.09f, 0.12f, 0.98f);

        // Title — hidden (we draw our own)
        colors[ImGuiCol_TitleBg]              = bg_dark();
        colors[ImGuiCol_TitleBgActive]        = bg_dark();
        colors[ImGuiCol_TitleBgCollapsed]     = bg_dark();

        // Border
        colors[ImGuiCol_Border]               = border();
        colors[ImGuiCol_BorderShadow]         = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

        // Frame (inputs, sliders, combos)
        colors[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.13f, 0.17f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.17f, 0.17f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.18f, 0.28f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
        colors[ImGuiCol_TabHovered]           = purple_hover;
        colors[ImGuiCol_TabSelected]          = purple;
        colors[ImGuiCol_TabSelectedOverline]  = teal;
        colors[ImGuiCol_TabDimmed]            = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
        colors[ImGuiCol_TabDimmedSelected]    = ImVec4(0.30f, 0.22f, 0.50f, 1.00f);

        // Buttons
        colors[ImGuiCol_Button]               = ImVec4(0.16f, 0.16f, 0.22f, 1.00f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.20f, 0.32f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = purple_active;

        // Checkmark + slider
        colors[ImGuiCol_CheckMark]            = teal;
        colors[ImGuiCol_SliderGrab]           = purple;
        colors[ImGuiCol_SliderGrabActive]     = purple_hover;

        // Header (collapsing headers, selectables)
        colors[ImGuiCol_Header]               = ImVec4(0.16f, 0.16f, 0.22f, 0.60f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.20f, 0.32f, 0.80f);
        colors[ImGuiCol_HeaderActive]         = purple;

        // Scrollbar
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.05f, 0.07f, 0.50f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.25f, 0.25f, 0.35f, 0.80f);
        colors[ImGuiCol_ScrollbarGrabHovered] = purple;
        colors[ImGuiCol_ScrollbarGrabActive]  = purple_hover;

        // Separator
        colors[ImGuiCol_Separator]            = ImVec4(0.20f, 0.20f, 0.28f, 0.40f);
        colors[ImGuiCol_SeparatorHovered]     = purple;
        colors[ImGuiCol_SeparatorActive]      = purple_hover;

        // Resize grip
        colors[ImGuiCol_ResizeGrip]           = ImVec4(0.20f, 0.20f, 0.28f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered]    = purple;
        colors[ImGuiCol_ResizeGripActive]     = purple_hover;

        // Text
        colors[ImGuiCol_Text]                 = text_main();
        colors[ImGuiCol_TextDisabled]         = text_dim();

        // Misc
        colors[ImGuiCol_MenuBarBg]            = bg_dark();
        colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.54f, 0.39f, 0.82f, 0.30f);
        colors[ImGuiCol_NavHighlight]         = purple;
    }

} // namespace prisme_theme
