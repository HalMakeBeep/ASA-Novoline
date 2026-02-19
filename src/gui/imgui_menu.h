#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "prisme_theme.h"
#include "../config/settings.h"
#include "../config/profile_manager.h"
#include "../core/diagnostics.h"
#include "../core/console.h"
#include <cmath>

namespace imgui_menu {

    // ── Fonts set by dx_hook during init ──
    inline ImFont* g_font_main  = nullptr;
    inline ImFont* g_font_title = nullptr;
    inline float   g_dpi_scale  = 1.0f;

    // ── State ──
    inline int active_tab = 0;

    // ── Helper: draw a soft glow rectangle ──
    inline void glow_rect(ImDrawList* dl, ImVec2 mn, ImVec2 mx, ImU32 col, float radius = 8.0f, int layers = 4) {
        for (int i = layers; i >= 0; i--) {
            float expand = (float)i * 3.0f;
            float alpha  = 0.08f / (float)(i + 1);
            ImU32 c = (col & 0x00FFFFFF) | ((ImU32)(alpha * 255) << 24);
            dl->AddRectFilled(
                ImVec2(mn.x - expand, mn.y - expand),
                ImVec2(mx.x + expand, mx.y + expand),
                c, radius + expand);
        }
    }

    // ── Helper: draw glow circle ──
    inline void glow_circle(ImDrawList* dl, ImVec2 center, float radius, ImU32 col, int layers = 3) {
        for (int i = layers; i >= 0; i--) {
            float expand = (float)i * 2.5f;
            float alpha  = 0.12f / (float)(i + 1);
            ImU32 c = (col & 0x00FFFFFF) | ((ImU32)(alpha * 255) << 24);
            dl->AddCircleFilled(center, radius + expand, c, 32);
        }
    }

    // ── Custom toggle switch with glow ──
    inline bool toggle(const char* label, bool* v) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        ImDrawList* draw = window->DrawList;

        float s = g_dpi_scale;
        float height = 20.0f * s;
        float width  = 36.0f * s;
        float radius = height * 0.5f;

        ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
        ImGui::ItemSize(bb, 0);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) *v = !*v;

        float t = *v ? 1.0f : 0.0f;

        // Glow when enabled
        if (*v) {
            ImU32 glow_col = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());
            glow_rect(draw, bb.Min, bb.Max, glow_col, radius, 3);
        }

        // Background pill
        ImVec4 bg_col = *v ? prisme_theme::accent() : ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
        if (hovered) { bg_col.x += 0.06f; bg_col.y += 0.06f; bg_col.z += 0.06f; }
        draw->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(bg_col), radius);

        // Circle knob with subtle shadow
        float knob_x = bb.Min.x + radius + t * (width - height);
        ImVec2 knob_center(knob_x, bb.Min.y + radius);
        float knob_r = radius - 3.0f * s;
        draw->AddCircleFilled(ImVec2(knob_center.x + 1, knob_center.y + 1), knob_r, IM_COL32(0, 0, 0, 60), 24);
        draw->AddCircleFilled(knob_center, knob_r, IM_COL32(255, 255, 255, 245), 24);

        return pressed;
    }

    // ── Labeled toggle row ──
    inline bool toggle_row(const char* label, bool* v, const char* desc = nullptr) {
        ImGui::PushID(label);
        float avail = ImGui::GetContentRegionAvail().x;
        ImVec2 pos = ImGui::GetCursorPos();
        float s = g_dpi_scale;

        ImGui::SetCursorPosY(pos.y + 3 * s);
        ImGui::TextUnformatted(label);
        if (desc) {
            ImGui::SetCursorPosX(pos.x);
            ImGui::PushStyleColor(ImGuiCol_Text, prisme_theme::text_dim());
            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextUnformatted(desc);
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleColor();
        }

        float toggle_w = 36.0f * s;
        ImGui::SameLine(avail - toggle_w);
        ImGui::SetCursorPosY(pos.y + 2 * s);
        bool changed = toggle("##t", v);

        // Subtle separator
        ImGui::Dummy(ImVec2(0, 2 * s));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(
            p, ImVec2(p.x + avail, p.y),
            IM_COL32(255, 255, 255, 12));
        ImGui::Dummy(ImVec2(0, 4 * s));

        ImGui::PopID();
        return changed;
    }

    // ── Section header with accent glow ──
    inline void section(const char* label) {
        float s = g_dpi_scale;
        ImGui::Dummy(ImVec2(0, 6 * s));

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Accent bar
        ImU32 accent_col = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());
        dl->AddRectFilled(p, ImVec2(p.x + 3 * s, p.y + ImGui::GetTextLineHeight()), accent_col, 1.5f);

        // Subtle glow behind bar
        for (int i = 3; i >= 0; i--) {
            float ex = (float)i * 2.0f;
            ImU32 gc = (accent_col & 0x00FFFFFF) | ((ImU32)(15 / (i + 1)) << 24);
            dl->AddRectFilled(
                ImVec2(p.x - ex, p.y - ex),
                ImVec2(p.x + 3 * s + ex, p.y + ImGui::GetTextLineHeight() + ex),
                gc, 2.0f);
        }

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10 * s);
        ImGui::PushStyleColor(ImGuiCol_Text, prisme_theme::accent());
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 3 * s));
    }

    // ── Slider with label ──
    inline bool slider_row(const char* label, float* v, float mn, float mx, const char* fmt = "%.0f") {
        ImGui::PushID(label);
        float s = g_dpi_scale;
        ImGui::TextUnformatted(label);
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, prisme_theme::accent());
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.18f, 1.00f));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        bool changed = ImGui::SliderFloat("##s", v, mn, mx, fmt);
        ImGui::PopStyleColor(2);
        ImGui::Dummy(ImVec2(0, 4 * s));
        ImGui::PopID();
        return changed;
    }

    // ── Combo with label ──
    inline bool combo_row(const char* label, int* v, const char* const items[], int count) {
        ImGui::PushID(label);
        float s = g_dpi_scale;
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        bool changed = ImGui::Combo("##c", v, items, count);
        ImGui::Dummy(ImVec2(0, 4 * s));
        ImGui::PopID();
        return changed;
    }

    // ── Hotkey widget ──
    inline const char* key_name(int vk) {
        switch (vk) {
            case VK_LBUTTON: return "LMB"; case VK_RBUTTON: return "RMB";
            case VK_MBUTTON: return "MMB"; case VK_XBUTTON1: return "M4";
            case VK_XBUTTON2: return "M5"; case VK_SHIFT: return "Shift";
            case VK_CONTROL: return "Ctrl"; case VK_MENU: return "Alt";
            case VK_SPACE: return "Space"; case VK_TAB: return "Tab";
            default:
                if (vk >= 0x30 && vk <= 0x39) { static char b[2]; b[0]=(char)vk; b[1]=0; return b; }
                if (vk >= 0x41 && vk <= 0x5A) { static char b[2]; b[0]=(char)vk; b[1]=0; return b; }
                if (vk >= VK_F1 && vk <= VK_F12) { static char b[4]; snprintf(b,4,"F%d",vk-VK_F1+1); return b; }
                return "???";
        }
    }

    inline bool hotkey_row(const char* label, int* key) {
        static int* active_key = nullptr;
        ImGui::PushID(label);
        float s = g_dpi_scale;

        ImGui::TextUnformatted(label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80 * s);

        char btn[64];
        if (active_key == key) snprintf(btn, sizeof(btn), "[ ... ]##hk");
        else snprintf(btn, sizeof(btn), "[ %s ]##hk", key_name(*key));

        bool is_active = (active_key == key);
        ImGui::PushStyleColor(ImGuiCol_Button, is_active ? ImVec4(0.54f,0.39f,0.82f,0.5f) : ImVec4(0.16f,0.16f,0.22f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.54f,0.39f,0.82f,0.35f));
        if (ImGui::Button(btn, ImVec2(80 * s, 0))) active_key = key;
        ImGui::PopStyleColor(2);

        bool changed = false;
        if (active_key == key) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) { active_key = nullptr; }
            else if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
                for (int vk = 0x01; vk < 0xFF; vk++) {
                    if (vk == VK_LBUTTON || vk == VK_ESCAPE) continue;
                    if (GetAsyncKeyState(vk) & 0x8000) { *key = vk; active_key = nullptr; changed = true; break; }
                }
            }
        }
        ImGui::Dummy(ImVec2(0, 4 * s));
        ImGui::PopID();
        return changed;
    }

    // ── Tab definitions ──
    enum Tab { TAB_AIMBOT, TAB_VISUALS, TAB_MISC, TAB_CONFIG, TAB_COUNT };
    inline const char* tab_labels[] = { "Aimbot", "Visuals", "Misc", "Config" };

    // ── Draw prism logo ──
    inline void draw_logo(ImDrawList* dl, ImVec2 center, float size) {
        ImU32 accent_col = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());
        ImU32 teal_col   = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent2());

        float h = size;
        float w = size * 0.85f;

        // Glow behind logo
        glow_circle(dl, center, size * 0.7f, accent_col, 5);

        // Outer prism triangle
        ImVec2 top(center.x, center.y - h * 0.5f);
        ImVec2 bl(center.x - w * 0.5f, center.y + h * 0.4f);
        ImVec2 br(center.x + w * 0.5f, center.y + h * 0.4f);

        // Fill with gradient-like effect (two triangles)
        ImU32 top_col = accent_col;
        ImU32 bot_col = teal_col;
        dl->AddTriangleFilled(top, bl, br, IM_COL32(138, 99, 210, 160));

        // Inner highlight triangle
        float inset = 0.25f;
        ImVec2 itop(center.x, top.y + h * inset);
        ImVec2 ibl(bl.x + w * inset * 0.5f, br.y - h * inset * 0.3f);
        ImVec2 ibr(br.x - w * inset * 0.5f, br.y - h * inset * 0.3f);
        dl->AddTriangleFilled(itop, ibl, ibr, IM_COL32(64, 190, 180, 140));

        // Edges with glow
        dl->AddTriangle(top, bl, br, accent_col, 2.0f);

        // Light refraction line through prism
        ImVec2 left_in(center.x - w * 0.15f, center.y);
        ImVec2 right_out(center.x + w * 0.15f, center.y);
        dl->AddLine(ImVec2(bl.x - 4, center.y - 2), left_in, IM_COL32(255, 255, 255, 120), 1.5f);
        dl->AddLine(right_out, ImVec2(br.x + 4, center.y - 6), IM_COL32(138, 99, 210, 180), 1.5f);
        dl->AddLine(right_out, ImVec2(br.x + 4, center.y), IM_COL32(64, 190, 180, 180), 1.5f);
        dl->AddLine(right_out, ImVec2(br.x + 4, center.y + 6), IM_COL32(80, 200, 120, 180), 1.5f);
    }

    // ── Sidebar ──
    inline void draw_sidebar(float width, float height) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float s = g_dpi_scale;
        ImU32 accent_col = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());

        // Sidebar background with subtle gradient
        ImU32 bg_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.10f, 0.09f, 0.14f, 1.00f));
        ImU32 bg_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(0.07f, 0.07f, 0.10f, 1.00f));
        dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + width, wp.y + height),
            bg_top, bg_top, bg_bot, bg_bot);

        // Right edge separator with glow
        ImVec2 edge_top(wp.x + width - 1, wp.y);
        ImVec2 edge_bot(wp.x + width - 1, wp.y + height);
        dl->AddLine(edge_top, edge_bot, IM_COL32(255, 255, 255, 10));

        // Logo
        float logo_y = wp.y + 30 * s;
        draw_logo(dl, ImVec2(wp.x + width / 2, logo_y), 22 * s);

        // Brand text under logo
        if (g_font_title) ImGui::PushFont(g_font_title);
        float title_w = ImGui::CalcTextSize("PRISME").x;
        ImVec2 title_pos(wp.x + (width - title_w) / 2, logo_y + 20 * s);

        // Text glow
        for (int i = 3; i >= 0; i--) {
            float a = 0.06f / (float)(i + 1);
            ImU32 gc = (accent_col & 0x00FFFFFF) | ((ImU32)(a * 255) << 24);
            dl->AddRectFilled(
                ImVec2(title_pos.x - 4 - i * 2, title_pos.y - 2 - i),
                ImVec2(title_pos.x + title_w + 4 + i * 2, title_pos.y + ImGui::GetTextLineHeight() + 2 + i),
                gc, 4.0f);
        }
        dl->AddText(title_pos, ImGui::ColorConvertFloat4ToU32(prisme_theme::accent()), "PRISME");
        if (g_font_title) ImGui::PopFont();

        // Separator
        float sep_y = logo_y + 42 * s;
        dl->AddLine(ImVec2(wp.x + 14, sep_y), ImVec2(wp.x + width - 14, sep_y), IM_COL32(255, 255, 255, 15));

        // Tab buttons
        ImGui::SetCursorPos(ImVec2(6 * s, sep_y - wp.y + 10 * s));
        float btn_w = width - 12 * s;
        for (int i = 0; i < TAB_COUNT; i++) {
            ImGui::PushID(i);
            bool is_active = (active_tab == i);
            ImVec2 btn_screen = ImGui::GetCursorScreenPos();

            // Active tab background glow
            if (is_active) {
                ImVec2 bg_mn = btn_screen;
                ImVec2 bg_mx(btn_screen.x + btn_w, btn_screen.y + 32 * s);
                glow_rect(dl, bg_mn, bg_mx, accent_col, 6.0f, 2);
            }

            ImVec4 btn_bg = is_active ? ImVec4(0.54f, 0.39f, 0.82f, 0.18f) : ImVec4(0, 0, 0, 0);
            ImGui::PushStyleColor(ImGuiCol_Button, btn_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.54f, 0.39f, 0.82f, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.54f, 0.39f, 0.82f, 0.25f));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.12f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14 * s, 8 * s));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            if (ImGui::Button(tab_labels[i], ImVec2(btn_w, 0)))
                active_tab = i;

            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(3);

            // Active accent bar
            if (is_active) {
                float item_h = ImGui::GetItemRectSize().y;
                ImVec2 bar_mn(btn_screen.x, btn_screen.y + 4 * s);
                ImVec2 bar_mx(btn_screen.x + 3 * s, btn_screen.y + item_h - 4 * s);
                dl->AddRectFilled(bar_mn, bar_mx, accent_col, 1.5f);
            }

            ImGui::Dummy(ImVec2(0, 2 * s));
            ImGui::PopID();
        }

        // Version text at bottom
        ImGui::SetCursorPos(ImVec2(0, height - wp.y - wp.y + height - 24 * s));
        // Recalculate position
        float ver_y = wp.y + height - 22 * s;
        ImGui::SetWindowFontScale(0.75f);
        float ver_w = ImGui::CalcTextSize("v1.0").x;
        dl->AddText(ImVec2(wp.x + (width - ver_w) / 2, ver_y),
            ImGui::ColorConvertFloat4ToU32(prisme_theme::text_dim()), "v1.0");
        ImGui::SetWindowFontScale(1.0f);
    }

    // ── Content panels ──

    inline void draw_aimbot() {
        section("General");
        toggle_row("Enable Aimbot", &config::aimbot::enabled);
        if (!config::aimbot::enabled) return;

        hotkey_row("Aim Key", &config::aimbot::aim_key);
        toggle_row("Mouse Aim", &config::aimbot::use_mouse);
        slider_row("Smoothing", &config::aimbot::smoothing, 1.0f, 25.0f, "%.1f");
        toggle_row("Show FOV Circle", &config::aimbot::show_fov);
        slider_row("FOV Size", &config::aimbot::fov, 5.0f, 180.0f);

        section("Targeting");
        toggle_row("Target Line", &config::aimbot::target_line);
        toggle_row("Visible Only", &config::aimbot::visible_only);
        toggle_row("Sticky Lock", &config::aimbot::sticky_lock);

        const char* modes[] = { "Closest To Center", "Closest Distance" };
        combo_row("Target Mode", &config::aimbot::target_mode, modes, 2);

        toggle_row("Prioritize Players", &config::aimbot::prioritize_players);
        toggle_row("Target Wild Dinos", &config::aimbot::target_wild_dinos);
        toggle_row("Target Enemy Tamed", &config::aimbot::target_enemy_tamed);

        section("Aim Bone");
        auto select_bone = [](const char* name, bool* target, bool* others[], int cnt) {
            bool sel = *target;
            ImGui::PushStyleColor(ImGuiCol_Header, sel ? prisme_theme::accent() : ImVec4(0.16f, 0.16f, 0.22f, 1.0f));
            if (ImGui::Selectable(name, sel, 0, ImVec2(65 * g_dpi_scale, 0))) {
                *target = true;
                for (int i = 0; i < cnt; i++) *others[i] = false;
            }
            ImGui::PopStyleColor();
        };
        bool* h_o[] = { &config::aimbot::bone::chest, &config::aimbot::bone::neck, &config::aimbot::bone::pelvis };
        bool* c_o[] = { &config::aimbot::bone::head, &config::aimbot::bone::neck, &config::aimbot::bone::pelvis };
        bool* n_o[] = { &config::aimbot::bone::head, &config::aimbot::bone::chest, &config::aimbot::bone::pelvis };
        bool* p_o[] = { &config::aimbot::bone::head, &config::aimbot::bone::chest, &config::aimbot::bone::neck };
        select_bone("Head", &config::aimbot::bone::head, h_o, 3); ImGui::SameLine();
        select_bone("Chest", &config::aimbot::bone::chest, c_o, 3); ImGui::SameLine();
        select_bone("Neck", &config::aimbot::bone::neck, n_o, 3); ImGui::SameLine();
        select_bone("Pelvis", &config::aimbot::bone::pelvis, p_o, 3);
    }

    inline void draw_visuals() {
        static int vis_tab = 0;
        float s = g_dpi_scale;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14 * s, 6 * s));
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
        if (ImGui::BeginTabBar("VisTabs")) {
            if (ImGui::BeginTabItem("Players")) { vis_tab = 0; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Dinos"))   { vis_tab = 1; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Radar"))    { vis_tab = 2; ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 6 * s));

        if (vis_tab == 0) {
            toggle_row("Enable Player ESP", &config::player_esp::enabled, "Show player overlays");
            toggle_row("Box", &config::player_esp::box);
            toggle_row("Cornered Box", &config::player_esp::cornered_box);
            toggle_row("Skeleton", &config::player_esp::skeleton);
            toggle_row("Snapline", &config::player_esp::snapline);
            toggle_row("Distance", &config::player_esp::show_distance);
            toggle_row("Name", &config::player_esp::show_name);
            slider_row("Max Distance", &config::player_esp::max_distance, 50.0f, 1000.0f);

            section("Tribes");
            toggle_row("Show Tribe", &config::player_esp::show_tribe);
            toggle_row("Show Tribe ID", &config::player_esp::show_tribe_id);
            const char* tribe_modes[] = { "Separate Line", "Inline Tag", "Tribe Only" };
            combo_row("Tribe Text Mode", &config::player_esp::tribe_text_mode, tribe_modes, 3);
            toggle_row("Relation Colors", &config::player_esp::use_tribe_relation_color);
            toggle_row("Show Friendly", &config::player_esp::show_friendly_tribes);
            toggle_row("Show Enemy", &config::player_esp::show_enemy_tribes);
            toggle_row("Show Neutral", &config::player_esp::show_neutral_tribes);
        }
        else if (vis_tab == 1) {
            toggle_row("Enable Dino ESP", &config::dino_esp::enabled, "Show dino overlays");
            toggle_row("Show Wild", &config::dino_esp::show_wild);
            toggle_row("Show Enemy Tamed", &config::dino_esp::show_tamed);
            toggle_row("Show Friendly", &config::dino_esp::show_friendly);
            toggle_row("Box", &config::dino_esp::box);
            toggle_row("Distance", &config::dino_esp::show_distance);
            toggle_row("Name / Type", &config::dino_esp::show_name);
            slider_row("Max Distance", &config::dino_esp::max_distance, 50.0f, 1000.0f);
        }
        else if (vis_tab == 2) {
            toggle_row("Enable Radar", &config::radar::enabled);
            if (config::radar::enabled) {
                slider_row("Size", &config::radar::size, 100.0f, 400.0f);
                slider_row("Position X", &config::radar::pos_x, 0.0f, 500.0f);
                slider_row("Position Y", &config::radar::pos_y, 0.0f, 500.0f);
            }
        }
    }

    inline void draw_misc() {
        section("Camera");
        toggle_row("FOV Changer", &config::misc::fov_changer);
        slider_row("Camera FOV", &config::misc::fov_value, 30.0f, 170.0f);

        section("Performance");
        toggle_row("Performance Mode", &config::style::performance_mode);
        toggle_row("Cache Actor Lists", &config::performance::cache_actors);
        slider_row("Aim Update Hz", &config::performance::aim_update_hz, 30.0f, 240.0f);
        slider_row("Players Refresh (ms)", &config::performance::players_refresh_ms, 16.0f, 500.0f);
        slider_row("Dinos Refresh (ms)", &config::performance::dinos_refresh_ms, 16.0f, 750.0f);
    }

    inline void draw_config() {
        float s = g_dpi_scale;
        section("Debug");
        toggle_row("Debug Overlay", &config::debug::show_info);
        toggle_row("Console Window", &config::debug::console);
        toggle_row("Capture Input On Menu", &config::style::capture_game_input_when_menu_open);

        if (config::debug::console && !dbg::active) dbg::init();

        section("Profiles");
        static const char* cfg_status = "";
        static int profile_idx = 0;
        int active_slot = config::profiles::get_active_slot();
        if (active_slot >= 1 && active_slot <= 5) profile_idx = active_slot - 1;

        const char* slots[] = { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5" };
        combo_row("Profile Slot", &profile_idx, slots, 5);

        int sel = profile_idx + 1;
        if (sel != config::profiles::get_active_slot()) config::profiles::set_active_slot(sel);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(18 * s, 6 * s));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.16f, 0.28f, 1.0f));
        if (ImGui::Button("Load"))   cfg_status = config::profiles::load_slot(sel)   ? "Loaded" : "Load failed";
        ImGui::SameLine();
        if (ImGui::Button("Save"))   cfg_status = config::profiles::save_slot(sel)   ? "Saved"  : "Save failed";
        ImGui::SameLine();
        if (ImGui::Button("Import")) cfg_status = config::profiles::import_slot(sel) ? "Imported" : "Import failed";
        ImGui::SameLine();
        if (ImGui::Button("Export")) cfg_status = config::profiles::export_slot(sel) ? "Exported" : "Export failed";
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (cfg_status[0]) {
            ImGui::Dummy(ImVec2(0, 4 * s));
            ImGui::PushStyleColor(ImGuiCol_Text, prisme_theme::accent2());
            ImGui::Text("%s", cfg_status);
            ImGui::PopStyleColor();
            diagnostics::set_config_status(cfg_status);
        }
    }

    // ── Main draw ──
    inline void draw() {
        float s = g_dpi_scale;
        const float sidebar_w = 130.0f * s;
        const float win_w = 700.0f * s;
        const float win_h = 520.0f * s;
        const float pad = 20.0f * s;

        ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;

        // Enable AA
        ImGui::GetStyle().AntiAliasedLines = true;
        ImGui::GetStyle().AntiAliasedFill  = true;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.065f, 0.065f, 0.085f, 0.97f));

        if (!ImGui::Begin("##Prisme", nullptr, flags)) {
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            return;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);

        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Window shadow (outer glow)
        for (int i = 5; i >= 0; i--) {
            float ex = (float)i * 4.0f;
            ImU32 sc = IM_COL32(0, 0, 0, 30 / (i + 1));
            dl->AddRect(ImVec2(wp.x - ex, wp.y - ex), ImVec2(wp.x + ws.x + ex, wp.y + ws.y + ex), sc, 12.0f + ex, 0, 1.0f);
        }

        // Top accent line with glow
        ImU32 accent_col = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());
        dl->AddRectFilled(ImVec2(wp.x + sidebar_w, wp.y), ImVec2(wp.x + ws.x, wp.y + 2), accent_col, 0);
        for (int i = 3; i >= 0; i--) {
            float a = 0.05f / (float)(i + 1);
            ImU32 gc = (accent_col & 0x00FFFFFF) | ((ImU32)(a * 255) << 24);
            dl->AddRectFilled(
                ImVec2(wp.x + sidebar_w, wp.y - i * 2),
                ImVec2(wp.x + ws.x, wp.y + 2 + i * 3),
                gc);
        }

        // Draw sidebar
        draw_sidebar(sidebar_w, ws.y);

        // Content area
        ImGui::SetCursorPos(ImVec2(sidebar_w + pad, pad + 4));
        ImGui::BeginChild("##content",
            ImVec2(ws.x - sidebar_w - pad * 2, ws.y - pad * 2 - 4),
            false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        switch (active_tab) {
            case TAB_AIMBOT:  draw_aimbot();  break;
            case TAB_VISUALS: draw_visuals(); break;
            case TAB_MISC:    draw_misc();    break;
            case TAB_CONFIG:  draw_config();  break;
        }

        ImGui::EndChild();
        ImGui::End();
    }

} // namespace imgui_menu
