#pragma once

#include "settings.h"
#include <fstream>
#include <string>
#include <unordered_map>

namespace config {
namespace io {

    inline const char* default_path() {
        return "asa_config.ini";
    }

    inline bool parse_bool(const std::string& v, bool fallback) {
        if (v == "1" || v == "true" || v == "TRUE") return true;
        if (v == "0" || v == "false" || v == "FALSE") return false;
        return fallback;
    }

    inline int parse_int(const std::string& v, int fallback) {
        try { return std::stoi(v); } catch (...) { return fallback; }
    }

    inline float parse_float(const std::string& v, float fallback) {
        try { return std::stof(v); } catch (...) { return fallback; }
    }

    inline void set_color4(float out[4], const std::string& csv, const float fallback[4]) {
        out[0] = fallback[0]; out[1] = fallback[1]; out[2] = fallback[2]; out[3] = fallback[3];
        size_t p0 = csv.find(',');
        if (p0 == std::string::npos) return;
        size_t p1 = csv.find(',', p0 + 1);
        if (p1 == std::string::npos) return;
        size_t p2 = csv.find(',', p1 + 1);
        if (p2 == std::string::npos) return;
        out[0] = parse_float(csv.substr(0, p0), fallback[0]);
        out[1] = parse_float(csv.substr(p0 + 1, p1 - p0 - 1), fallback[1]);
        out[2] = parse_float(csv.substr(p1 + 1, p2 - p1 - 1), fallback[2]);
        out[3] = parse_float(csv.substr(p2 + 1), fallback[3]);
    }

    inline std::string color4_to_csv(const float in[4]) {
        return std::to_string(in[0]) + "," + std::to_string(in[1]) + "," +
            std::to_string(in[2]) + "," + std::to_string(in[3]);
    }

    inline bool save(const char* path = nullptr) {
        const char* file_path = path ? path : default_path();
        std::ofstream out(file_path, std::ios::trunc);
        if (!out.is_open()) return false;

        auto w_bool = [&](const char* k, bool v) { out << k << "=" << (v ? 1 : 0) << "\n"; };
        auto w_int = [&](const char* k, int v) { out << k << "=" << v << "\n"; };
        auto w_float = [&](const char* k, float v) { out << k << "=" << v << "\n"; };
        auto w_color = [&](const char* k, const float c[4]) { out << k << "=" << color4_to_csv(c) << "\n"; };

        w_bool("aimbot.enabled", aimbot::enabled);
        w_int("aimbot.aim_key", aimbot::aim_key);
        w_bool("aimbot.use_mouse", aimbot::use_mouse);
        w_bool("aimbot.show_fov", aimbot::show_fov);
        w_bool("aimbot.visible_only", aimbot::visible_only);
        w_bool("aimbot.target_line", aimbot::target_line);
        w_float("aimbot.smoothing", aimbot::smoothing);
        w_float("aimbot.fov", aimbot::fov);
        w_int("aimbot.target_mode", aimbot::target_mode);
        w_bool("aimbot.sticky_lock", aimbot::sticky_lock);
        w_bool("aimbot.prioritize_players", aimbot::prioritize_players);
        w_bool("aimbot.target_wild_dinos", aimbot::target_wild_dinos);
        w_bool("aimbot.target_enemy_tamed", aimbot::target_enemy_tamed);
        w_bool("aimbot.bone.head", aimbot::bone::head);
        w_bool("aimbot.bone.neck", aimbot::bone::neck);
        w_bool("aimbot.bone.chest", aimbot::bone::chest);
        w_bool("aimbot.bone.pelvis", aimbot::bone::pelvis);

        w_bool("player.enabled", player_esp::enabled);
        w_bool("player.box", player_esp::box);
        w_bool("player.cornered_box", player_esp::cornered_box);
        w_bool("player.skeleton", player_esp::skeleton);
        w_bool("player.snapline", player_esp::snapline);
        w_bool("player.show_distance", player_esp::show_distance);
        w_bool("player.show_name", player_esp::show_name);
        w_bool("player.show_tribe", player_esp::show_tribe);
        w_bool("player.show_tribe_id", player_esp::show_tribe_id);
        w_int("player.tribe_text_mode", player_esp::tribe_text_mode);
        w_bool("player.use_tribe_relation_color", player_esp::use_tribe_relation_color);
        w_bool("player.show_friendly_tribes", player_esp::show_friendly_tribes);
        w_bool("player.show_enemy_tribes", player_esp::show_enemy_tribes);
        w_bool("player.show_neutral_tribes", player_esp::show_neutral_tribes);
        w_float("player.max_distance", player_esp::max_distance);
        w_color("player.color", player_esp::color);
        w_color("player.visible_color", player_esp::visible_color);
        w_color("player.tribe_friendly_color", player_esp::tribe_friendly_color);
        w_color("player.tribe_enemy_color", player_esp::tribe_enemy_color);
        w_color("player.tribe_neutral_color", player_esp::tribe_neutral_color);

        w_bool("dino.enabled", dino_esp::enabled);
        w_bool("dino.show_wild", dino_esp::show_wild);
        w_bool("dino.show_tamed", dino_esp::show_tamed);
        w_bool("dino.show_friendly", dino_esp::show_friendly);
        w_bool("dino.box", dino_esp::box);
        w_bool("dino.show_distance", dino_esp::show_distance);
        w_bool("dino.show_name", dino_esp::show_name);
        w_float("dino.max_distance", dino_esp::max_distance);
        w_color("dino.wild_color", dino_esp::wild_color);
        w_color("dino.tamed_color", dino_esp::tamed_color);
        w_color("dino.friendly_color", dino_esp::friendly_color);

        w_bool("radar.enabled", radar::enabled);
        w_bool("radar.show_players", radar::show_players);
        w_bool("radar.show_dinos", radar::show_dinos);
        w_bool("radar.show_grid", radar::show_grid);
        w_bool("radar.show_compass", radar::show_compass);
        w_float("radar.range", radar::range);
        w_float("radar.pos_x", radar::pos_x);
        w_float("radar.pos_y", radar::pos_y);
        w_float("radar.size", radar::size);

        w_bool("misc.fov_changer", misc::fov_changer);
        w_float("misc.fov_value", misc::fov_value);

        w_bool("performance.cache_actors", performance::cache_actors);
        w_float("performance.players_refresh_ms", performance::players_refresh_ms);
        w_float("performance.dinos_refresh_ms", performance::dinos_refresh_ms);
        w_float("performance.aim_update_hz", performance::aim_update_hz);
        w_bool("style.text_outlined", style::text_outlined);
        w_bool("style.performance_mode", style::performance_mode);
        w_bool("style.capture_game_input_when_menu_open", style::capture_game_input_when_menu_open);
        w_bool("debug.show_info", debug::show_info);
        w_bool("debug.log_actors", debug::log_actors);
        w_bool("debug.console", debug::console);

        return true;
    }

    inline bool load(const char* path = nullptr) {
        const char* file_path = path ? path : default_path();
        std::ifstream in(file_path);
        if (!in.is_open()) return false;

        std::unordered_map<std::string, std::string> kv;
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos || eq == 0) continue;
            kv[line.substr(0, eq)] = line.substr(eq + 1);
        }

        auto r_bool = [&](const char* k, bool& v) { auto it = kv.find(k); if (it != kv.end()) v = parse_bool(it->second, v); };
        auto r_int = [&](const char* k, int& v) { auto it = kv.find(k); if (it != kv.end()) v = parse_int(it->second, v); };
        auto r_float = [&](const char* k, float& v) { auto it = kv.find(k); if (it != kv.end()) v = parse_float(it->second, v); };
        auto r_color = [&](const char* k, float c[4]) { auto it = kv.find(k); if (it != kv.end()) { const float fallback[4] = { c[0], c[1], c[2], c[3] }; set_color4(c, it->second, fallback); } };

        r_bool("aimbot.enabled", aimbot::enabled);
        r_int("aimbot.aim_key", aimbot::aim_key);
        r_bool("aimbot.use_mouse", aimbot::use_mouse);
        r_bool("aimbot.show_fov", aimbot::show_fov);
        r_bool("aimbot.visible_only", aimbot::visible_only);
        r_bool("aimbot.target_line", aimbot::target_line);
        r_float("aimbot.smoothing", aimbot::smoothing);
        r_float("aimbot.fov", aimbot::fov);
        r_int("aimbot.target_mode", aimbot::target_mode);
        r_bool("aimbot.sticky_lock", aimbot::sticky_lock);
        r_bool("aimbot.prioritize_players", aimbot::prioritize_players);
        r_bool("aimbot.target_wild_dinos", aimbot::target_wild_dinos);
        r_bool("aimbot.target_enemy_tamed", aimbot::target_enemy_tamed);
        r_bool("aimbot.bone.head", aimbot::bone::head);
        r_bool("aimbot.bone.neck", aimbot::bone::neck);
        r_bool("aimbot.bone.chest", aimbot::bone::chest);
        r_bool("aimbot.bone.pelvis", aimbot::bone::pelvis);

        r_bool("player.enabled", player_esp::enabled);
        r_bool("player.box", player_esp::box);
        r_bool("player.cornered_box", player_esp::cornered_box);
        r_bool("player.skeleton", player_esp::skeleton);
        r_bool("player.snapline", player_esp::snapline);
        r_bool("player.show_distance", player_esp::show_distance);
        r_bool("player.show_name", player_esp::show_name);
        r_bool("player.show_tribe", player_esp::show_tribe);
        r_bool("player.show_tribe_id", player_esp::show_tribe_id);
        r_int("player.tribe_text_mode", player_esp::tribe_text_mode);
        r_bool("player.use_tribe_relation_color", player_esp::use_tribe_relation_color);
        r_bool("player.show_friendly_tribes", player_esp::show_friendly_tribes);
        r_bool("player.show_enemy_tribes", player_esp::show_enemy_tribes);
        r_bool("player.show_neutral_tribes", player_esp::show_neutral_tribes);
        r_float("player.max_distance", player_esp::max_distance);
        r_color("player.color", player_esp::color);
        r_color("player.visible_color", player_esp::visible_color);
        r_color("player.tribe_friendly_color", player_esp::tribe_friendly_color);
        r_color("player.tribe_enemy_color", player_esp::tribe_enemy_color);
        r_color("player.tribe_neutral_color", player_esp::tribe_neutral_color);

        r_bool("dino.enabled", dino_esp::enabled);
        r_bool("dino.show_wild", dino_esp::show_wild);
        r_bool("dino.show_tamed", dino_esp::show_tamed);
        r_bool("dino.show_friendly", dino_esp::show_friendly);
        r_bool("dino.box", dino_esp::box);
        r_bool("dino.show_distance", dino_esp::show_distance);
        r_bool("dino.show_name", dino_esp::show_name);
        r_float("dino.max_distance", dino_esp::max_distance);
        r_color("dino.wild_color", dino_esp::wild_color);
        r_color("dino.tamed_color", dino_esp::tamed_color);
        r_color("dino.friendly_color", dino_esp::friendly_color);

        r_bool("radar.enabled", radar::enabled);
        r_bool("radar.show_players", radar::show_players);
        r_bool("radar.show_dinos", radar::show_dinos);
        r_bool("radar.show_grid", radar::show_grid);
        r_bool("radar.show_compass", radar::show_compass);
        r_float("radar.range", radar::range);
        r_float("radar.pos_x", radar::pos_x);
        r_float("radar.pos_y", radar::pos_y);
        r_float("radar.size", radar::size);

        r_bool("misc.fov_changer", misc::fov_changer);
        r_float("misc.fov_value", misc::fov_value);

        r_bool("performance.cache_actors", performance::cache_actors);
        r_float("performance.players_refresh_ms", performance::players_refresh_ms);
        r_float("performance.dinos_refresh_ms", performance::dinos_refresh_ms);
        r_float("performance.aim_update_hz", performance::aim_update_hz);
        r_bool("style.text_outlined", style::text_outlined);
        r_bool("style.performance_mode", style::performance_mode);
        r_bool("style.capture_game_input_when_menu_open", style::capture_game_input_when_menu_open);
        r_bool("debug.show_info", debug::show_info);
        r_bool("debug.log_actors", debug::log_actors);
        r_bool("debug.console", debug::console);

        if (aimbot::target_mode != 1) aimbot::target_mode = 0;
        if (player_esp::tribe_text_mode < 0 || player_esp::tribe_text_mode > 2) player_esp::tribe_text_mode = 0;
        if (misc::fov_value < 30.0f) misc::fov_value = 30.0f;
        if (misc::fov_value > 170.0f) misc::fov_value = 170.0f;
        if (!aimbot::bone::head && !aimbot::bone::neck && !aimbot::bone::chest && !aimbot::bone::pelvis) {
            aimbot::bone::head = true;
        }
        int enabled_bones = (aimbot::bone::head ? 1 : 0) + (aimbot::bone::neck ? 1 : 0) +
            (aimbot::bone::chest ? 1 : 0) + (aimbot::bone::pelvis ? 1 : 0);
        if (enabled_bones > 1) {
            if (aimbot::bone::head) {
                aimbot::bone::neck = false;
                aimbot::bone::chest = false;
                aimbot::bone::pelvis = false;
            }
            else if (aimbot::bone::neck) {
                aimbot::bone::chest = false;
                aimbot::bone::pelvis = false;
            }
            else if (aimbot::bone::chest) {
                aimbot::bone::pelvis = false;
            }
        }
        return true;
    }

} // namespace io
} // namespace config
