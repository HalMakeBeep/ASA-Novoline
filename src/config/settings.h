#pragma once
/*
* ARK Survival Ascended - Configuration Settings
*/

#include <Windows.h>

namespace config {

//=============================================================================
// Aimbot Settings
//=============================================================================
namespace aimbot {
    inline bool enabled = true;
    inline int aim_key = VK_RBUTTON;
    inline bool use_mouse = true;
    inline bool show_fov = true;
    inline bool visible_only = false;
    inline bool target_line = false;
    inline float smoothing = 5.0f;
    inline float fov = 15.0f;
    inline int target_mode = 0; // 0 = closest to center, 1 = closest distance
    inline bool sticky_lock = true;

    // Target priority
    inline bool prioritize_players = true;
    inline bool target_wild_dinos = true;
    inline bool target_enemy_tamed = false;

    // Bone selection
    namespace bone {
        inline bool head = true;
        inline bool neck = false;
        inline bool chest = false;
        inline bool pelvis = false;
    }
}

//=============================================================================
// Player ESP Settings
//=============================================================================
namespace player_esp {
    inline bool enabled = true;
    inline bool box = true;
    inline bool cornered_box = false;
    inline bool skeleton = false;
    inline bool snapline = false;
    inline bool show_distance = true;
    inline bool show_name = true;
    inline bool show_tribe = true;
    inline bool show_tribe_id = false;
    inline int tribe_text_mode = 0; // 0 = separate line, 1 = inline tag, 2 = tribe only
    inline bool use_tribe_relation_color = true;
    inline bool show_friendly_tribes = true;
    inline bool show_enemy_tribes = true;
    inline bool show_neutral_tribes = true;
    inline bool show_health = false;
    inline float max_distance = 500.0f;

    // Colors (RGBA)
    inline float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };         // Red
    inline float visible_color[4] = { 0.0f, 1.0f, 0.0f, 1.0f };  // Green
    inline float tribe_friendly_color[4] = { 0.20f, 1.00f, 0.20f, 1.00f };
    inline float tribe_enemy_color[4] = { 1.00f, 0.35f, 0.35f, 1.00f };
    inline float tribe_neutral_color[4] = { 1.00f, 1.00f, 1.00f, 1.00f };
}

//=============================================================================
// Dino ESP Settings
//=============================================================================
namespace dino_esp {
    inline bool enabled = true;
    inline bool show_wild = true;
    inline bool show_tamed = true;
    inline bool show_friendly = false;
    inline bool box = true;
    inline bool show_distance = true;
    inline bool show_name = true;
    inline bool show_level = true;
    inline float max_distance = 300.0f;

    // Colors
    inline float wild_color[4] = { 1.0f, 1.0f, 0.0f, 1.0f };     // Yellow
    inline float tamed_color[4] = { 1.0f, 0.5f, 0.0f, 1.0f };    // Orange
    inline float friendly_color[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; // Green
}

//=============================================================================
// Radar Settings
//=============================================================================
namespace radar {
    inline bool enabled = false;
    inline bool show_players = true;
    inline bool show_dinos = true;
    inline float range = 100.0f;
    inline float pos_x = 50.0f;
    inline float pos_y = 50.0f;
    inline float size = 200.0f;
}

//=============================================================================
// Misc Settings
//=============================================================================
namespace misc {
    inline bool fov_changer = false;
    inline float fov_value = 100.0f;
}

//=============================================================================
// Performance Settings
//=============================================================================
namespace performance {
    inline bool cache_actors = true;
    inline float players_refresh_ms = 120.0f;
    inline float dinos_refresh_ms = 180.0f;
    inline float aim_update_hz = 120.0f;
}

//=============================================================================
// Visual Style Settings
//=============================================================================
namespace style {
    inline bool text_outlined = true;
    inline bool performance_mode = false;
    inline bool capture_game_input_when_menu_open = true;
}

//=============================================================================
// Debug Settings
//=============================================================================
namespace debug {
    inline bool show_info = false;
    inline bool log_actors = false;
    inline bool console = true;  // Show debug console window on init
}

} // namespace config

// Backwards compatibility aliases
namespace settings {
    namespace aimbot {
        inline auto& enable = config::aimbot::enabled;
        inline auto& aim_key = config::aimbot::aim_key;
        inline auto& mouse = config::aimbot::use_mouse;
        inline auto& show_fov = config::aimbot::show_fov;
        inline auto& visible_only = config::aimbot::visible_only;
        inline auto& target_line = config::aimbot::target_line;
        inline auto& mouse_speed = config::aimbot::smoothing;
        inline auto& field_of_view = config::aimbot::fov;
        inline auto& target_mode = config::aimbot::target_mode;
        inline auto& sticky_lock = config::aimbot::sticky_lock;
        inline auto& prioritize_players = config::aimbot::prioritize_players;
        inline auto& target_wild_dinos = config::aimbot::target_wild_dinos;
        inline auto& target_tamed_dinos = config::aimbot::target_enemy_tamed;
    }
    namespace bones {
        inline auto& head = config::aimbot::bone::head;
        inline auto& neck = config::aimbot::bone::neck;
        inline auto& torso = config::aimbot::bone::chest;
        inline auto& chest = config::aimbot::bone::chest;
        inline auto& pelvis = config::aimbot::bone::pelvis;
    }
    namespace players {
        inline auto& enable = config::player_esp::enabled;
        inline auto& box = config::player_esp::box;
        inline auto& cornered_box = config::player_esp::cornered_box;
        inline auto& skeleton = config::player_esp::skeleton;
        inline auto& snapline = config::player_esp::snapline;
        inline auto& display_distance = config::player_esp::show_distance;
        inline auto& display_name = config::player_esp::show_name;
        inline auto& show_tribe = config::player_esp::show_tribe;
        inline auto& show_tribe_id = config::player_esp::show_tribe_id;
        inline auto& tribe_text_mode = config::player_esp::tribe_text_mode;
        inline auto& use_tribe_relation_color = config::player_esp::use_tribe_relation_color;
        inline auto& show_friendly_tribes = config::player_esp::show_friendly_tribes;
        inline auto& show_enemy_tribes = config::player_esp::show_enemy_tribes;
        inline auto& show_neutral_tribes = config::player_esp::show_neutral_tribes;
        inline auto& tribe_friendly_color = config::player_esp::tribe_friendly_color;
        inline auto& tribe_enemy_color = config::player_esp::tribe_enemy_color;
        inline auto& tribe_neutral_color = config::player_esp::tribe_neutral_color;
        inline auto& max_distance = config::player_esp::max_distance;
    }
    namespace dinos {
        inline auto& enable = config::dino_esp::enabled;
        inline auto& wild = config::dino_esp::show_wild;
        inline auto& tamed = config::dino_esp::show_tamed;
        inline auto& friendly = config::dino_esp::show_friendly;
        inline auto& box = config::dino_esp::box;
        inline auto& display_distance = config::dino_esp::show_distance;
        inline auto& display_name = config::dino_esp::show_name;
        inline auto& max_distance = config::dino_esp::max_distance;
    }
    namespace radar {
        inline auto& enable = config::radar::enabled;
        inline auto& show_players = config::radar::show_players;
        inline auto& show_dinos = config::radar::show_dinos;
        inline auto& range = config::radar::range;
        inline auto& positionx = config::radar::pos_x;
        inline auto& positiony = config::radar::pos_y;
        inline auto& size = config::radar::size;
    }
    namespace misc {
        inline auto& fov_changer = config::misc::fov_changer;
        inline auto& fov_value = config::misc::fov_value;
    }
    namespace style {
        inline auto& text_outlined = config::style::text_outlined;
        inline auto& performance = config::style::performance_mode;
        inline auto& capture_input = config::style::capture_game_input_when_menu_open;
    }
    namespace performance {
        inline auto& cache_actors = config::performance::cache_actors;
        inline auto& players_refresh_ms = config::performance::players_refresh_ms;
        inline auto& dinos_refresh_ms = config::performance::dinos_refresh_ms;
        inline auto& aim_update_hz = config::performance::aim_update_hz;
    }
    namespace debug {
        inline auto& show_debug_info = config::debug::show_info;
    }
}
