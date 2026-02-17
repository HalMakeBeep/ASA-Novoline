#pragma once

#include "imgui.h"
#include "../config/settings.h"
#include "../config/profile_manager.h"
#include "../core/diagnostics.h"
#include "../core/console.h"

namespace imgui_menu {

    // Helper: key name for hotkey display
    inline const char* key_name(int vk) {
        switch (vk) {
            case VK_LBUTTON:  return "LMB";
            case VK_RBUTTON:  return "RMB";
            case VK_MBUTTON:  return "MMB";
            case VK_XBUTTON1: return "Mouse4";
            case VK_XBUTTON2: return "Mouse5";
            case VK_SHIFT:    return "Shift";
            case VK_CONTROL:  return "Ctrl";
            case VK_MENU:     return "Alt";
            case VK_CAPITAL:  return "CapsLock";
            case VK_TAB:      return "Tab";
            case VK_SPACE:    return "Space";
            default:
                if (vk >= 0x30 && vk <= 0x39) {
                    static char buf[2];
                    buf[0] = (char)vk; buf[1] = 0;
                    return buf;
                }
                if (vk >= 0x41 && vk <= 0x5A) {
                    static char buf[2];
                    buf[0] = (char)vk; buf[1] = 0;
                    return buf;
                }
                if (vk >= VK_F1 && vk <= VK_F12) {
                    static char buf[4];
                    snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
                    return buf;
                }
                return "???";
        }
    }

    // Hotkey widget: click to rebind, then press any key
    inline bool hotkey_widget(const char* label, int* key) {
        static int* active_key = nullptr;

        char btn_label[64];
        if (active_key == key) {
            snprintf(btn_label, sizeof(btn_label), "[Press a key]##%s", label);
        } else {
            snprintf(btn_label, sizeof(btn_label), "%s##%s", key_name(*key), label);
        }

        bool changed = false;
        ImGui::PushStyleColor(ImGuiCol_Button, active_key == key
            ? ImVec4(0.54f, 0.39f, 0.82f, 0.8f)
            : ImVec4(0.25f, 0.25f, 0.28f, 1.0f));

        if (ImGui::Button(btn_label, ImVec2(100, 0))) {
            active_key = key;
        }

        ImGui::PopStyleColor();

        if (active_key == key) {
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                // ignore - that's the click to activate
            } else if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
                *key = VK_RBUTTON; active_key = nullptr; changed = true;
            } else if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) {
                *key = VK_MBUTTON; active_key = nullptr; changed = true;
            } else if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) {
                *key = VK_XBUTTON1; active_key = nullptr; changed = true;
            } else if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) {
                *key = VK_XBUTTON2; active_key = nullptr; changed = true;
            } else if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
                active_key = nullptr;
            } else {
                for (int vk = 0x08; vk < 0xFF; vk++) {
                    if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON ||
                        vk == VK_XBUTTON1 || vk == VK_XBUTTON2 || vk == VK_ESCAPE)
                        continue;
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        *key = vk;
                        active_key = nullptr;
                        changed = true;
                        break;
                    }
                }
            }
        }

        if (label[0] != '#') {
            ImGui::SameLine();
            ImGui::TextUnformatted(label);
        }

        return changed;
    }

    // Radio-style bone selector (only one active at a time)
    inline void bone_radio(const char* label, bool* target, bool* others[], int count) {
        if (ImGui::Checkbox(label, target)) {
            if (*target) {
                for (int i = 0; i < count; i++)
                    *others[i] = false;
            }
        }
    }

    inline void draw() {
        ImGui::SetNextWindowSize(ImVec2(750, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
        if (!ImGui::Begin("PRISME ASA", nullptr, flags))
        {
            ImGui::End();
            return;
        }

        if (ImGui::BeginTabBar("MainTabs")) {

            // ===================== AIMBOT TAB =====================
            if (ImGui::BeginTabItem("Aimbot")) {
                ImGui::Checkbox("Enable Aimbot", &config::aimbot::enabled);

                if (config::aimbot::enabled) {
                    ImGui::Separator();
                    ImGui::Columns(2, "aim_cols", false);

                    // Left column
                    ImGui::Text("Aim Key:");
                    hotkey_widget("##aimkey", &config::aimbot::aim_key);
                    ImGui::Checkbox("Mouse Aim", &config::aimbot::use_mouse);
                    ImGui::SliderFloat("Smoothing", &config::aimbot::smoothing, 1.0f, 25.0f, "%.1f");
                    ImGui::Checkbox("Show FOV Circle", &config::aimbot::show_fov);
                    ImGui::SliderFloat("FOV Size", &config::aimbot::fov, 5.0f, 180.0f, "%.0f");

                    // Right column
                    ImGui::NextColumn();
                    ImGui::Checkbox("Target Line", &config::aimbot::target_line);
                    ImGui::Checkbox("Visible Only", &config::aimbot::visible_only);
                    ImGui::Checkbox("Sticky Target Lock", &config::aimbot::sticky_lock);

                    const char* target_modes[] = { "Closest To Center", "Closest Distance" };
                    ImGui::Combo("Target Mode", &config::aimbot::target_mode, target_modes, 2);

                    ImGui::Spacing();
                    ImGui::Text("Target Selection:");
                    ImGui::Checkbox("Prioritize Players", &config::aimbot::prioritize_players);
                    ImGui::Checkbox("Target Wild Dinos", &config::aimbot::target_wild_dinos);
                    ImGui::Checkbox("Target Enemy Tamed", &config::aimbot::target_enemy_tamed);

                    ImGui::Spacing();
                    ImGui::Text("Aim Bone:");

                    // Bone radio buttons
                    bool* head_others[] = { &config::aimbot::bone::chest, &config::aimbot::bone::neck, &config::aimbot::bone::pelvis };
                    bone_radio("Head", &config::aimbot::bone::head, head_others, 3);

                    bool* chest_others[] = { &config::aimbot::bone::head, &config::aimbot::bone::neck, &config::aimbot::bone::pelvis };
                    bone_radio("Chest", &config::aimbot::bone::chest, chest_others, 3);

                    bool* neck_others[] = { &config::aimbot::bone::head, &config::aimbot::bone::chest, &config::aimbot::bone::pelvis };
                    bone_radio("Neck", &config::aimbot::bone::neck, neck_others, 3);

                    bool* pelvis_others[] = { &config::aimbot::bone::head, &config::aimbot::bone::chest, &config::aimbot::bone::neck };
                    bone_radio("Pelvis", &config::aimbot::bone::pelvis, pelvis_others, 3);

                    ImGui::Columns(1);
                }

                ImGui::EndTabItem();
            }

            // ===================== PLAYERS TAB =====================
            if (ImGui::BeginTabItem("Players")) {
                ImGui::Columns(2, "player_cols", false);

                // Left column
                ImGui::Checkbox("Enable Player ESP", &config::player_esp::enabled);
                ImGui::Checkbox("Box", &config::player_esp::box);
                ImGui::Checkbox("Cornered Box", &config::player_esp::cornered_box);
                ImGui::Checkbox("Skeleton", &config::player_esp::skeleton);
                ImGui::Checkbox("Snapline", &config::player_esp::snapline);
                ImGui::Checkbox("Show Distance", &config::player_esp::show_distance);
                ImGui::Checkbox("Show Name", &config::player_esp::show_name);
                ImGui::SliderFloat("Max Distance", &config::player_esp::max_distance, 50.0f, 1000.0f, "%.0f");

                // Right column
                ImGui::NextColumn();
                ImGui::Text("Tribe ESP:");
                ImGui::Checkbox("Show Tribe", &config::player_esp::show_tribe);
                ImGui::Checkbox("Show Tribe ID", &config::player_esp::show_tribe_id);

                const char* tribe_modes[] = { "Separate Line", "Inline Tag", "Tribe Only" };
                ImGui::Combo("Tribe Text Mode", &config::player_esp::tribe_text_mode, tribe_modes, 3);

                ImGui::Checkbox("Use Relation Colors", &config::player_esp::use_tribe_relation_color);
                ImGui::Checkbox("Show Friendly Tribes", &config::player_esp::show_friendly_tribes);
                ImGui::Checkbox("Show Enemy Tribes", &config::player_esp::show_enemy_tribes);
                ImGui::Checkbox("Show Neutral/Unknown", &config::player_esp::show_neutral_tribes);

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Radar:");
                ImGui::Checkbox("Enable Radar", &config::radar::enabled);
                if (config::radar::enabled) {
                    ImGui::SliderFloat("Radar Size", &config::radar::size, 100.0f, 400.0f, "%.0f");
                    ImGui::SliderFloat("Radar X", &config::radar::pos_x, 0.0f, 500.0f, "%.0f");
                    ImGui::SliderFloat("Radar Y", &config::radar::pos_y, 0.0f, 500.0f, "%.0f");
                }

                ImGui::Columns(1);
                ImGui::EndTabItem();
            }

            // ===================== DINOS TAB =====================
            if (ImGui::BeginTabItem("Dinos")) {
                ImGui::Checkbox("Enable Dino ESP", &config::dino_esp::enabled);
                ImGui::Checkbox("Show Wild Dinos", &config::dino_esp::show_wild);
                ImGui::Checkbox("Show Enemy Tamed", &config::dino_esp::show_tamed);
                ImGui::Checkbox("Show Friendly", &config::dino_esp::show_friendly);
                ImGui::Checkbox("Box", &config::dino_esp::box);
                ImGui::Checkbox("Show Distance", &config::dino_esp::show_distance);
                ImGui::Checkbox("Show Name/Type", &config::dino_esp::show_name);
                ImGui::SliderFloat("Max Distance", &config::dino_esp::max_distance, 50.0f, 1000.0f, "%.0f");

                ImGui::EndTabItem();
            }

            // ===================== MISC TAB =====================
            if (ImGui::BeginTabItem("Misc")) {
                ImGui::Checkbox("Enable FOV Changer", &config::misc::fov_changer);
                ImGui::SliderFloat("Camera FOV", &config::misc::fov_value, 30.0f, 170.0f, "%.0f");
                ImGui::TextDisabled("Target: UCameraComponent + 0x334");

                ImGui::EndTabItem();
            }

            // ===================== DEBUG TAB =====================
            if (ImGui::BeginTabItem("Debug")) {
                static const char* cfg_status = "Config: Idle";
                static int profile_idx = 0;
                int active_slot = config::profiles::get_active_slot();
                if (active_slot >= 1 && active_slot <= 5) {
                    profile_idx = active_slot - 1;
                }

                ImGui::Checkbox("Show Debug Overlay", &config::debug::show_info);
                ImGui::Checkbox("Show Console Window", &config::debug::console);
                ImGui::Checkbox("Performance Mode", &config::style::performance_mode);
                ImGui::Checkbox("Capture Input On Menu", &config::style::capture_game_input_when_menu_open);
                ImGui::Checkbox("Cache Actor Lists", &config::performance::cache_actors);
                ImGui::SliderFloat("Aim Update Hz", &config::performance::aim_update_hz, 30.0f, 240.0f, "%.0f");
                ImGui::SliderFloat("Players Refresh (ms)", &config::performance::players_refresh_ms, 16.0f, 500.0f, "%.0f");
                ImGui::SliderFloat("Dinos Refresh (ms)", &config::performance::dinos_refresh_ms, 16.0f, 750.0f, "%.0f");

                ImGui::Separator();
                ImGui::Text("Profile:");

                const char* slots[] = { "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5" };
                ImGui::Combo("Slot", &profile_idx, slots, 5);

                int selected_slot = profile_idx + 1;
                if (selected_slot != config::profiles::get_active_slot()) {
                    config::profiles::set_active_slot(selected_slot);
                }

                if (ImGui::Button("Load Slot", ImVec2(120, 0))) {
                    cfg_status = config::profiles::load_slot(selected_slot) ? "Profile: Loaded slot" : "Profile: Load failed";
                }
                ImGui::SameLine();
                if (ImGui::Button("Save Slot", ImVec2(120, 0))) {
                    cfg_status = config::profiles::save_slot(selected_slot) ? "Profile: Saved slot" : "Profile: Save failed";
                }

                if (ImGui::Button("Import Slot", ImVec2(120, 0))) {
                    cfg_status = config::profiles::import_slot(selected_slot) ? "Profile: Imported slot" : "Profile: Import failed";
                }
                ImGui::SameLine();
                if (ImGui::Button("Export Slot", ImVec2(120, 0))) {
                    cfg_status = config::profiles::export_slot(selected_slot) ? "Profile: Exported slot" : "Profile: Export failed";
                }

                ImGui::TextColored(ImVec4(0.25f, 0.75f, 0.70f, 1.0f), "%s", cfg_status);
                diagnostics::set_config_status(cfg_status);

                if (config::debug::console && !dbg::active) {
                    dbg::init();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

} // namespace imgui_menu
