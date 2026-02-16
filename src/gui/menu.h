#pragma once

#include "render.h"
#include "../config/settings.h"
#include "zerogui.h"

namespace gui {
    namespace menu {

        inline FVector2D position = { 25, 25 };

        inline void draw(UCanvas* canvas) {
            if (!render::show_menu) return;

            ZeroGUI::SetupCanvas(canvas);
            ZeroGUI::Input::Handle();

            float width = canvas->ClipX();
            float height = canvas->ClipY();

            if (ZeroGUI::Window("HalMakeBeep", &position, FVector2D{ 750.0f, 600.0f }, render::show_menu, width, height)) {
                static int tab = 0;
                const FVector2D tab_size{ 140.0f, 25.0f };

                if (ZeroGUI::ButtonTab("Aimbot", tab_size, tab == 0)) tab = 0;
                ZeroGUI::SameLine();
                if (ZeroGUI::ButtonTab("Players", tab_size, tab == 1)) tab = 1;
                ZeroGUI::SameLine();
                if (ZeroGUI::ButtonTab("Dinos", tab_size, tab == 2)) tab = 2;
                ZeroGUI::SameLine();
                if (ZeroGUI::ButtonTab("Misc", tab_size, tab == 3)) tab = 3;
                ZeroGUI::SameLine();
                if (ZeroGUI::ButtonTab("Debug", tab_size, tab == 4)) tab = 4;

                // Reset column anchor so NextColumn aligns with tab content, not the tab bar itself.
                ZeroGUI::ClearFirstPos();

                if (tab == 0) {
                    ZeroGUI::Checkbox("Enable Aimbot", &config::aimbot::enabled);
                    if (config::aimbot::enabled) {
                        // Left column
                        ZeroGUI::Text("Aim Key:");
                        ZeroGUI::Hotkey(" ", FVector2D{ 150, 25 }, &config::aimbot::aim_key);
                        ZeroGUI::Checkbox("Mouse Aim", &config::aimbot::use_mouse);
                        ZeroGUI::SliderFloat("Smoothing", &config::aimbot::smoothing, 1.0f, 25.0f);
                        // ZeroGUI::SliderFloat("Aim Update Hz", &config::performance::aim_update_hz, 30.0f, 240.0f);
                        ZeroGUI::Checkbox("Show FOV Circle", &config::aimbot::show_fov);
                        ZeroGUI::SliderFloat("FOV Size", &config::aimbot::fov, 5.0f, 180.0f);

                        // Right column
                        ZeroGUI::NextColumn(360.0f);
                        ZeroGUI::Checkbox("Target Line", &config::aimbot::target_line);
                        ZeroGUI::Checkbox("Visible Only", &config::aimbot::visible_only);
                        ZeroGUI::Checkbox("Sticky Target Lock", &config::aimbot::sticky_lock);
                        ZeroGUI::Text("Target Mode:");
                        ZeroGUI::Combobox(" ", FVector2D{ 180, 25 }, &config::aimbot::target_mode, "Closest To Center", "Closest Distance", (const char*)nullptr);

                        ZeroGUI::Text("Target Selection:");
                        ZeroGUI::Checkbox("Prioritize Players", &config::aimbot::prioritize_players);
                        ZeroGUI::Checkbox("Target Wild Dinos", &config::aimbot::target_wild_dinos);
                        ZeroGUI::Checkbox("Target Enemy Tamed", &config::aimbot::target_enemy_tamed);

                        ZeroGUI::Text("Aim Bone:");
                        ZeroGUI::Checkbox("Head", &config::aimbot::bone::head);
                        if (config::aimbot::bone::head) {
                            config::aimbot::bone::chest = false;
                            config::aimbot::bone::neck = false;
                            config::aimbot::bone::pelvis = false;
                        }
                        ZeroGUI::Checkbox("Chest", &config::aimbot::bone::chest);
                        if (config::aimbot::bone::chest) {
                            config::aimbot::bone::head = false;
                            config::aimbot::bone::neck = false;
                            config::aimbot::bone::pelvis = false;
                        }
                        ZeroGUI::Checkbox("Neck", &config::aimbot::bone::neck);
                        if (config::aimbot::bone::neck) {
                            config::aimbot::bone::head = false;
                            config::aimbot::bone::chest = false;
                            config::aimbot::bone::pelvis = false;
                        }
                        ZeroGUI::Checkbox("Pelvis", &config::aimbot::bone::pelvis);
                        if (config::aimbot::bone::pelvis) {
                            config::aimbot::bone::head = false;
                            config::aimbot::bone::neck = false;
                            config::aimbot::bone::chest = false;
                        }
                    }
                }
                else if (tab == 1) {
                    // Left column
                    ZeroGUI::Checkbox("Enable Player ESP", &config::player_esp::enabled);
                    ZeroGUI::Checkbox("Box", &config::player_esp::box);
                    ZeroGUI::Checkbox("Cornered Box", &config::player_esp::cornered_box);
                    ZeroGUI::Checkbox("Skeleton", &config::player_esp::skeleton);
                    ZeroGUI::Checkbox("Snapline", &config::player_esp::snapline);
                    ZeroGUI::Checkbox("Show Distance", &config::player_esp::show_distance);
                    ZeroGUI::Checkbox("Show Name", &config::player_esp::show_name);
                    ZeroGUI::SliderFloat("Max Distance", &config::player_esp::max_distance, 50.0f, 1000.0f);

                    // Right column
                    ZeroGUI::NextColumn(360.0f);
                    ZeroGUI::Text("Tribe ESP:");
                    ZeroGUI::Checkbox("Show Tribe", &config::player_esp::show_tribe);
                    ZeroGUI::Checkbox("Show Tribe ID", &config::player_esp::show_tribe_id);
                    ZeroGUI::Combobox("Tribe Text Mode", FVector2D{ 180, 25 }, &config::player_esp::tribe_text_mode,
                        "Separate Line", "Inline Tag", "Tribe Only", (const char*)nullptr);
                    ZeroGUI::Checkbox("Use Relation Colors", &config::player_esp::use_tribe_relation_color);
                    ZeroGUI::Checkbox("Show Friendly Tribes", &config::player_esp::show_friendly_tribes);
                    ZeroGUI::Checkbox("Show Enemy Tribes", &config::player_esp::show_enemy_tribes);
                    ZeroGUI::Checkbox("Show Neutral/Unknown", &config::player_esp::show_neutral_tribes);

                    ZeroGUI::Text("Radar:");
                    ZeroGUI::Checkbox("Enable Radar", &config::radar::enabled);
                    if (config::radar::enabled) {
                        ZeroGUI::SliderFloat("Radar Size", &config::radar::size, 100.0f, 400.0f);
                        ZeroGUI::SliderFloat("Radar X", &config::radar::pos_x, 0.0f, 500.0f);
                        ZeroGUI::SliderFloat("Radar Y", &config::radar::pos_y, 0.0f, 500.0f);
                    }
                }
                else if (tab == 2) {
                    // Left column
                    ZeroGUI::Checkbox("Enable Dino ESP", &config::dino_esp::enabled);
                    ZeroGUI::Checkbox("Show Wild Dinos", &config::dino_esp::show_wild);
                    ZeroGUI::Checkbox("Show Enemy Tamed", &config::dino_esp::show_tamed);
                    ZeroGUI::Checkbox("Show Friendly", &config::dino_esp::show_friendly);
                    ZeroGUI::Checkbox("Box", &config::dino_esp::box);
                    ZeroGUI::Checkbox("Show Distance", &config::dino_esp::show_distance);
                    ZeroGUI::Checkbox("Show Name/Type", &config::dino_esp::show_name);
                    ZeroGUI::SliderFloat("Max Distance", &config::dino_esp::max_distance, 50.0f, 1000.0f);
                }
                else if (tab == 3) {
                    ZeroGUI::Checkbox("Enable FOV Changer", &config::misc::fov_changer);
                    ZeroGUI::SliderFloat("Camera FOV", &config::misc::fov_value, 30.0f, 170.0f);
                    ZeroGUI::Text("Target: UCameraComponent + 0x334");
                }
                else if (tab == 4) {
                    static const char* cfg_status = "Config: Idle";
                    static int profile_idx = 0;
                    int active_slot = config::profiles::get_active_slot();
                    if (active_slot >= 1 && active_slot <= 5) {
                        profile_idx = active_slot - 1;
                    }

                    ZeroGUI::Checkbox("Show Debug Overlay", &config::debug::show_info);
                    ZeroGUI::Checkbox("Show Console Window", &config::debug::console);
                    ZeroGUI::Checkbox("Performance Mode", &config::style::performance_mode);
                    ZeroGUI::Checkbox("Capture Input On Menu", &config::style::capture_game_input_when_menu_open);
                    ZeroGUI::Checkbox("Cache Actor Lists", &config::performance::cache_actors);
                    ZeroGUI::SliderFloat("Aim Update Hz", &config::performance::aim_update_hz, 30.0f, 240.0f);
                    ZeroGUI::SliderFloat("Players Refresh (ms)", &config::performance::players_refresh_ms, 16.0f, 500.0f);
                    ZeroGUI::SliderFloat("Dinos Refresh (ms)", &config::performance::dinos_refresh_ms, 16.0f, 750.0f);

                    ZeroGUI::Text("Profile:");
                    ZeroGUI::Combobox("Slot", FVector2D{ 140, 25 }, &profile_idx,
                        "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", (const char*)nullptr);

                    int selected_slot = profile_idx + 1;
                    if (selected_slot != config::profiles::get_active_slot()) {
                        config::profiles::set_active_slot(selected_slot);
                    }

                    if (ZeroGUI::Button("Load Slot", FVector2D{ 140, 25 })) {
                        cfg_status = config::profiles::load_slot(selected_slot) ? "Profile: Loaded slot" : "Profile: Load failed";
                    }
                    ZeroGUI::SameLine();
                    if (ZeroGUI::Button("Save Slot", FVector2D{ 140, 25 })) {
                        cfg_status = config::profiles::save_slot(selected_slot) ? "Profile: Saved slot" : "Profile: Save failed";
                    }
                    if (ZeroGUI::Button("Import Slot", FVector2D{ 140, 25 })) {
                        cfg_status = config::profiles::import_slot(selected_slot) ? "Profile: Imported slot" : "Profile: Import failed";
                    }
                    ZeroGUI::SameLine();
                    if (ZeroGUI::Button("Export Slot", FVector2D{ 140, 25 })) {
                        cfg_status = config::profiles::export_slot(selected_slot) ? "Profile: Exported slot" : "Profile: Export failed";
                    }
                    ZeroGUI::Text(cfg_status);
                    diagnostics::set_config_status(cfg_status);

                    if (config::debug::console && !dbg::active) {
                        dbg::init();
                    }
                }

                // Always draw a visible cursor while menu is open, even if the game hides hardware cursor.
                ZeroGUI::Draw_Cursor(true);
            }
        }

    }
}
