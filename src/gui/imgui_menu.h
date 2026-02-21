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

    // ── Helper: draw soft glow rect ──
    inline void glow_rect(ImDrawList* dl, ImVec2 mn, ImVec2 mx, ImU32 col, float rad = 8.f, int layers = 3) {
        for (int i = layers; i >= 0; i--) {
            float ex = (float)i * 3.f;
            float a = 0.07f / (float)(i + 1);
            ImU32 c = (col & 0x00FFFFFF) | ((ImU32)(a * 255) << 24);
            dl->AddRectFilled(ImVec2(mn.x - ex, mn.y - ex), ImVec2(mx.x + ex, mx.y + ex), c, rad + ex);
        }
    }

    // ── Draw icons using ImDrawList ──
    inline void icon_crosshair(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
        dl->AddCircle(c, r, col, 16, 1.5f);
        dl->AddLine(ImVec2(c.x - r * 0.5f, c.y), ImVec2(c.x + r * 0.5f, c.y), col, 1.5f);
        dl->AddLine(ImVec2(c.x, c.y - r * 0.5f), ImVec2(c.x, c.y + r * 0.5f), col, 1.5f);
    }

    inline void icon_eye(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
        dl->AddEllipseFilled(c, ImVec2(r, r * 0.55f), col, 0, 16);
        dl->AddCircleFilled(c, r * 0.3f, IM_COL32(18, 18, 24, 255), 12);
    }

    inline void icon_wrench(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
        dl->AddLine(ImVec2(c.x - r * 0.5f, c.y + r * 0.5f), ImVec2(c.x + r * 0.2f, c.y - r * 0.2f), col, 2.0f);
        dl->AddCircle(ImVec2(c.x + r * 0.35f, c.y - r * 0.35f), r * 0.35f, col, 12, 1.5f);
    }

    inline void icon_gear(ImDrawList* dl, ImVec2 c, float r, ImU32 col) {
        dl->AddCircle(c, r * 0.7f, col, 16, 1.5f);
        dl->AddCircleFilled(c, r * 0.25f, col, 10);
        for (int i = 0; i < 6; i++) {
            float a = (float)i / 6.f * 6.2832f;
            ImVec2 p1(c.x + cosf(a) * r * 0.55f, c.y + sinf(a) * r * 0.55f);
            ImVec2 p2(c.x + cosf(a) * r * 0.9f, c.y + sinf(a) * r * 0.9f);
            dl->AddLine(p1, p2, col, 2.0f);
        }
    }

    // ── Custom toggle switch ──
    inline bool toggle(const char* label, bool* v) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window->SkipItems) return false;

        ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        ImDrawList* draw = window->DrawList;

        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;
        float height = 20.f * s;
        float width  = 36.f * s;
        float radius = height * 0.5f;

        ImRect bb(pos, ImVec2(pos.x + width, pos.y + height));
        ImGui::ItemSize(bb, 0);
        if (!ImGui::ItemAdd(bb, id)) return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
        if (pressed) *v = !*v;

        float t = *v ? 1.0f : 0.0f;

        // Glow when ON
        if (*v) {
            ImU32 gc = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());
            glow_rect(draw, bb.Min, bb.Max, gc, radius, 2);
        }

        // Pill background
        ImVec4 bg = *v ? prisme_theme::accent() : ImVec4(0.22f, 0.22f, 0.28f, 1.f);
        if (hovered) { bg.x += 0.05f; bg.y += 0.05f; bg.z += 0.05f; }
        draw->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(bg), radius);

        // Knob
        float kx = bb.Min.x + radius + t * (width - height);
        ImVec2 kc(kx, bb.Min.y + radius);
        float kr = radius - 3.f * s;
        draw->AddCircleFilled(ImVec2(kc.x + 1, kc.y + 1), kr, IM_COL32(0, 0, 0, 50), 20);
        draw->AddCircleFilled(kc, kr, IM_COL32(255, 255, 255, 240), 20);

        return pressed;
    }

    // ── Toggle row: label left, toggle right ──
    inline bool toggle_row(const char* label, bool* v, const char* desc = nullptr) {
        ImGui::PushID(label);
        float avail = ImGui::GetContentRegionAvail().x;
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;
        ImVec2 pos = ImGui::GetCursorPos();

        ImGui::SetCursorPosY(pos.y + 3 * s);
        ImGui::TextUnformatted(label);
        if (desc) {
            ImGui::PushStyleColor(ImGuiCol_Text, prisme_theme::text_dim());
            ImGui::TextUnformatted(desc);
            ImGui::PopStyleColor();
        }

        float tw = 36.f * s;
        ImGui::SameLine(avail - tw);
        ImGui::SetCursorPosY(pos.y + 2 * s);
        bool changed = toggle("##t", v);

        ImGui::Dummy(ImVec2(0, 2 * s));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + avail, p.y), IM_COL32(255, 255, 255, 10));
        ImGui::Dummy(ImVec2(0, 4 * s));

        ImGui::PopID();
        return changed;
    }

    // ── Section header ──
    inline void section(const char* label) {
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;
        ImGui::Dummy(ImVec2(0, 6 * s));
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 ac = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());
        dl->AddRectFilled(p, ImVec2(p.x + 3 * s, p.y + ImGui::GetTextLineHeight()), ac, 1.5f);

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10 * s);
        ImGui::PushStyleColor(ImGuiCol_Text, prisme_theme::accent());
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 3 * s));
    }

    // ── Slider ──
    inline bool slider_row(const char* label, float* v, float mn, float mx, const char* fmt = "%.0f") {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        bool c = ImGui::SliderFloat("##s", v, mn, mx, fmt);
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::PopID();
        return c;
    }

    // ── Combo ──
    inline bool combo_row(const char* label, int* v, const char* const items[], int count) {
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        bool c = ImGui::Combo("##c", v, items, count);
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::PopID();
        return c;
    }

    // ── Hotkey ──
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
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;

        ImGui::TextUnformatted(label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80 * s);

        char btn[64];
        if (active_key == key) snprintf(btn, sizeof(btn), "[ ... ]##hk");
        else snprintf(btn, sizeof(btn), "[ %s ]##hk", key_name(*key));

        ImGui::PushStyleColor(ImGuiCol_Button, (active_key == key) ? ImVec4(0.54f,0.39f,0.82f,0.5f) : ImVec4(0.16f,0.16f,0.22f,1.f));
        if (ImGui::Button(btn, ImVec2(80 * s, 0))) active_key = key;
        ImGui::PopStyleColor();

        bool changed = false;
        if (active_key == key) {
            if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) active_key = nullptr;
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
    inline const char* tab_labels[] = { "  Aimbot", "  Visuals", "  Misc", "  Config" };


    // ── Sidebar ──
    inline void draw_sidebar(float width, float height) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;
        ImU32 accent_col = ImGui::ColorConvertFloat4ToU32(prisme_theme::accent());

        // Sidebar gradient background
        ImU32 bg_top = ImGui::ColorConvertFloat4ToU32(ImVec4(0.10f, 0.09f, 0.14f, 1.f));
        ImU32 bg_bot = ImGui::ColorConvertFloat4ToU32(ImVec4(0.07f, 0.07f, 0.10f, 1.f));
        dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + width, wp.y + height), bg_top, bg_top, bg_bot, bg_bot);

        // Right edge line
        dl->AddLine(ImVec2(wp.x + width - 1, wp.y), ImVec2(wp.x + width - 1, wp.y + height), IM_COL32(255, 255, 255, 10));

        // ── Brand text ──
        float title_y = wp.y + 14 * s;
        if (g_font_title) ImGui::PushFont(g_font_title);
        float tw = ImGui::CalcTextSize("PRISME").x;
        ImVec2 tp(wp.x + (width - tw) / 2, title_y);
        // Text glow
        for (int i = 2; i >= 0; i--) {
            float a = 0.04f / (float)(i + 1);
            dl->AddRectFilled(
                ImVec2(tp.x - 4 - i * 2, tp.y - 2 - i),
                ImVec2(tp.x + tw + 4 + i * 2, tp.y + ImGui::GetTextLineHeight() + 2 + i),
                (accent_col & 0x00FFFFFF) | ((ImU32)(a * 255) << 24), 4.f);
        }
        dl->AddText(tp, accent_col, "PRISME");
        if (g_font_title) ImGui::PopFont();

        // Separator
        float sep_y = title_y + 28 * s;
        dl->AddLine(ImVec2(wp.x + 14, sep_y), ImVec2(wp.x + width - 14, sep_y), IM_COL32(255, 255, 255, 15));

        // ── Tab buttons with icons ──
        float btn_w = width - 12 * s;
        float icon_r = 7.f * s;
        float start_y = sep_y - wp.y + 12 * s;

        ImGui::SetCursorPos(ImVec2(6 * s, start_y));
        for (int i = 0; i < TAB_COUNT; i++) {
            ImGui::PushID(i);
            bool is_act = (active_tab == i);
            ImVec2 bpos = ImGui::GetCursorScreenPos();

            // Glow for active
            if (is_act) {
                glow_rect(dl, bpos, ImVec2(bpos.x + btn_w, bpos.y + 30 * s), accent_col, 6.f, 2);
            }

            ImVec4 btn_bg = is_act ? ImVec4(0.54f, 0.39f, 0.82f, 0.18f) : ImVec4(0, 0, 0, 0);
            ImGui::PushStyleColor(ImGuiCol_Button, btn_bg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.54f, 0.39f, 0.82f, 0.12f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.54f, 0.39f, 0.82f, 0.25f));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.22f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14 * s, 8 * s));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);

            if (ImGui::Button(tab_labels[i], ImVec2(btn_w, 0)))
                active_tab = i;

            float item_h = ImGui::GetItemRectSize().y;
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(3);

            // Draw icon to the left of text
            ImU32 icon_col = is_act ? accent_col : IM_COL32(160, 160, 180, 200);
            ImVec2 ic(bpos.x + 16 * s, bpos.y + item_h * 0.5f);
            switch (i) {
                case TAB_AIMBOT:  icon_crosshair(dl, ic, icon_r, icon_col); break;
                case TAB_VISUALS: icon_eye(dl, ic, icon_r, icon_col); break;
                case TAB_MISC:    icon_wrench(dl, ic, icon_r, icon_col); break;
                case TAB_CONFIG:  icon_gear(dl, ic, icon_r, icon_col); break;
            }

            // Active accent bar
            if (is_act) {
                dl->AddRectFilled(
                    ImVec2(bpos.x, bpos.y + 4 * s),
                    ImVec2(bpos.x + 3 * s, bpos.y + item_h - 4 * s),
                    accent_col, 1.5f);
            }

            ImGui::Dummy(ImVec2(0, 2 * s));
            ImGui::PopID();
        }

        // Version at bottom
        float ver_y = wp.y + height - 22 * s;
        ImGui::SetWindowFontScale(0.75f);
        float vw = ImGui::CalcTextSize("v1.0").x;
        dl->AddText(ImVec2(wp.x + (width - vw) / 2, ver_y),
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
        auto sel_bone = [](const char* name, bool* t, bool* o[], int n) {
            ImGui::PushStyleColor(ImGuiCol_Header, *t ? prisme_theme::accent() : ImVec4(0.16f,0.16f,0.22f,1.f));
            if (ImGui::Selectable(name, *t, 0, ImVec2(60, 0))) {
                *t = true; for (int i = 0; i < n; i++) *o[i] = false;
            }
            ImGui::PopStyleColor();
        };
        bool* ho[] = { &config::aimbot::bone::chest, &config::aimbot::bone::neck, &config::aimbot::bone::pelvis };
        bool* co[] = { &config::aimbot::bone::head, &config::aimbot::bone::neck, &config::aimbot::bone::pelvis };
        bool* no[] = { &config::aimbot::bone::head, &config::aimbot::bone::chest, &config::aimbot::bone::pelvis };
        bool* po[] = { &config::aimbot::bone::head, &config::aimbot::bone::chest, &config::aimbot::bone::neck };
        sel_bone("Head", &config::aimbot::bone::head, ho, 3); ImGui::SameLine();
        sel_bone("Chest", &config::aimbot::bone::chest, co, 3); ImGui::SameLine();
        sel_bone("Neck", &config::aimbot::bone::neck, no, 3); ImGui::SameLine();
        sel_bone("Pelvis", &config::aimbot::bone::pelvis, po, 3);
    }

    inline void draw_visuals() {
        static int vt = 0;
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14 * s, 6 * s));
        if (ImGui::BeginTabBar("VTabs")) {
            if (ImGui::BeginTabItem("Players")) { vt = 0; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Dinos"))   { vt = 1; ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Radar"))    { vt = 2; ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 6 * s));

        if (vt == 0) {
            toggle_row("Enable Player ESP", &config::player_esp::enabled, "Toggle player overlays");
            toggle_row("Box", &config::player_esp::box);
            toggle_row("Cornered Box", &config::player_esp::cornered_box);
            toggle_row("Skeleton", &config::player_esp::skeleton);
            toggle_row("Health Bar", &config::player_esp::show_health);
            toggle_row("Snapline", &config::player_esp::snapline);
            toggle_row("Distance", &config::player_esp::show_distance);
            toggle_row("Name", &config::player_esp::show_name);
            slider_row("Max Distance", &config::player_esp::max_distance, 50.f, 1000.f);

            section("Tribes");
            toggle_row("Show Tribe", &config::player_esp::show_tribe);
            toggle_row("Show Tribe ID", &config::player_esp::show_tribe_id);
            const char* tm[] = { "Separate Line", "Inline Tag", "Tribe Only" };
            combo_row("Tribe Text Mode", &config::player_esp::tribe_text_mode, tm, 3);
            toggle_row("Relation Colors", &config::player_esp::use_tribe_relation_color);
            toggle_row("Show Friendly", &config::player_esp::show_friendly_tribes);
            toggle_row("Show Enemy", &config::player_esp::show_enemy_tribes);
            toggle_row("Show Neutral", &config::player_esp::show_neutral_tribes);
        }
        else if (vt == 1) {
            toggle_row("Enable Dino ESP", &config::dino_esp::enabled, "Toggle dino overlays");
            toggle_row("Show Wild", &config::dino_esp::show_wild);
            toggle_row("Show Enemy Tamed", &config::dino_esp::show_tamed);
            toggle_row("Show Friendly", &config::dino_esp::show_friendly);
            toggle_row("Box", &config::dino_esp::box);
            toggle_row("Distance", &config::dino_esp::show_distance);
            toggle_row("Name / Type", &config::dino_esp::show_name);
            slider_row("Max Distance", &config::dino_esp::max_distance, 50.f, 1000.f);
        }
        else if (vt == 2) {
            toggle_row("Enable Radar", &config::radar::enabled);
            if (config::radar::enabled) {
                toggle_row("Show Players", &config::radar::show_players);
                toggle_row("Show Dinos", &config::radar::show_dinos);
                toggle_row("Show Grid", &config::radar::show_grid);
                toggle_row("Show Compass", &config::radar::show_compass);
                slider_row("Range", &config::radar::range, 10.f, 500.f);
                slider_row("Size", &config::radar::size, 100.f, 400.f);
                slider_row("Position X", &config::radar::pos_x, 0.f, 1500.f);
                slider_row("Position Y", &config::radar::pos_y, 0.f, 1000.f);
            }
        }
    }

    inline void draw_misc() {
        section("Camera");
        toggle_row("FOV Changer", &config::misc::fov_changer);
        slider_row("Camera FOV", &config::misc::fov_value, 30.f, 170.f);

        section("Performance");
        toggle_row("Performance Mode", &config::style::performance_mode);
        toggle_row("Cache Actor Lists", &config::performance::cache_actors);
        slider_row("Aim Update Hz", &config::performance::aim_update_hz, 30.f, 240.f);
        slider_row("Players Refresh (ms)", &config::performance::players_refresh_ms, 16.f, 500.f);
        slider_row("Dinos Refresh (ms)", &config::performance::dinos_refresh_ms, 16.f, 750.f);
    }

    inline void draw_config() {
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;
        section("Debug");
        toggle_row("Debug Overlay", &config::debug::show_info);
        toggle_row("Console Window", &config::debug::console);
        toggle_row("Capture Input On Menu", &config::style::capture_game_input_when_menu_open);

        if (config::debug::console && !dbg::active) dbg::init();

        section("Profiles");
        static const char* cs = "";
        static int pi = 0;
        int as = config::profiles::get_active_slot();
        if (as >= 1 && as <= 5) pi = as - 1;

        const char* slots[] = { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5" };
        combo_row("Profile Slot", &pi, slots, 5);

        int sel = pi + 1;
        if (sel != config::profiles::get_active_slot()) config::profiles::set_active_slot(sel);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16 * s, 6 * s));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f,0.16f,0.28f,1.f));
        if (ImGui::Button("Load"))   cs = config::profiles::load_slot(sel)   ? "Loaded" : "Load failed";
        ImGui::SameLine();
        if (ImGui::Button("Save"))   cs = config::profiles::save_slot(sel)   ? "Saved"  : "Save failed";
        ImGui::SameLine();
        if (ImGui::Button("Import")) cs = config::profiles::import_slot(sel) ? "Imported" : "Import failed";
        ImGui::SameLine();
        if (ImGui::Button("Export")) cs = config::profiles::export_slot(sel) ? "Exported" : "Export failed";
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (cs[0]) {
            ImGui::Dummy(ImVec2(0, 4 * s));
            ImGui::PushStyleColor(ImGuiCol_Text, prisme_theme::accent2());
            ImGui::Text("%s", cs);
            ImGui::PopStyleColor();
            diagnostics::set_config_status(cs);
        }
    }

    // ── Main draw ──
    inline void draw() {
        float s = g_dpi_scale > 0 ? g_dpi_scale : 1.0f;
        const float sidebar_w = 140.f * s;
        const float win_w = 720.f * s;
        const float win_h = 520.f * s;
        const float pad = 20.f * s;

        ImGui::SetNextWindowSize(ImVec2(win_w, win_h), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
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

        // Window shadow
        for (int i = 4; i >= 0; i--) {
            float ex = (float)i * 4.f;
            dl->AddRect(ImVec2(wp.x - ex, wp.y - ex), ImVec2(wp.x + ws.x + ex, wp.y + ws.y + ex),
                IM_COL32(0, 0, 0, 25 / (i + 1)), 12.f + ex, 0, 1.f);
        }

        // Sidebar
        draw_sidebar(sidebar_w, ws.y);

        // Content
        ImGui::SetCursorPos(ImVec2(sidebar_w + pad, pad + 6 * s));
        ImGui::BeginChild("##content",
            ImVec2(ws.x - sidebar_w - pad * 2, ws.y - pad * 2 - 6 * s),
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
