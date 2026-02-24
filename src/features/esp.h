#pragma once

#include "../sdk/sdk.h"
#include "../gui/render.h"
#include "../config/settings.h"
#include "../core/diagnostics.h"
#include "radar.h"
#include "aimbot.h"
#include "chams.h"
#include <cfloat>
#include <cmath>
#include <cwctype>
#include <string>
#include <vector>

namespace features {
    namespace esp {

        struct ScreenBox {
            FVector2D top_left;
            FVector2D bottom_right;
            double width = 0.0;
            double height = 0.0;
            bool valid = false;
        };

        struct AimTarget {
            APrimalCharacter* character = nullptr;
            FVector location;
            double distance_to_crosshair = DBL_MAX;
            double distance_to_camera = DBL_MAX;
            bool is_player = false;
        };

        enum class TribeRelation {
            Friendly,
            Enemy,
            Neutral
        };

        struct PlayerTribeInfo {
            std::wstring tribe_name;
            int tribe_id = 0;
            bool has_valid_team = false;
            TribeRelation relation = TribeRelation::Neutral;
        };

        inline AimTarget best_target;
        inline AimTarget locked_target;
        inline APrimalCharacter* smoothed_target = nullptr;
        inline FVector smoothed_aim_location;
        inline bool has_smoothed_aim_location = false;
        inline APrimalCharacter* last_controller_target = nullptr;
        inline TArray<UObject*> cached_players;
        inline TArray<UObject*> cached_dinos;
        inline ULONGLONG cached_players_tick = 0;
        inline ULONGLONG cached_dinos_tick = 0;
        inline bool players_cache_refreshed = false;
        inline bool dinos_cache_refreshed = false;

        inline int target_mode() {
            return config::aimbot::target_mode == 1 ? 1 : 0;
        }

        inline void clear_locked_target() {
            locked_target = AimTarget();
            smoothed_target = nullptr;
            has_smoothed_aim_location = false;
            last_controller_target = nullptr;
            aimbot::reset_aim_state();
        }

        inline void reset_target() {
            best_target = AimTarget();
        }

        inline bool should_refresh_cache(ULONGLONG last_tick, float interval_ms) {
            if (interval_ms < 1.0f) interval_ms = 1.0f;
            return (GetTickCount64() - last_tick) >= static_cast<ULONGLONG>(interval_ms);
        }

        inline const TArray<UObject*>& get_player_actors(UWorld* world) {
            const bool use_cache = config::style::performance_mode || config::performance::cache_actors;
            players_cache_refreshed = false;
            if (!use_cache || should_refresh_cache(cached_players_tick, config::performance::players_refresh_ms) || cached_players.empty()) {
                cached_players = sdk::GameStatics->GetAllActorsOfClass(world, sdk::PlayerCharacterClass);
                cached_players_tick = GetTickCount64();
                players_cache_refreshed = true;
            }
            diagnostics::set_player_cache_stats(cached_players.size(), GetTickCount64() - cached_players_tick, players_cache_refreshed);
            return cached_players;
        }

        inline const TArray<UObject*>& get_dino_actors(UWorld* world) {
            const bool use_cache = config::style::performance_mode || config::performance::cache_actors;
            dinos_cache_refreshed = false;
            if (!use_cache || should_refresh_cache(cached_dinos_tick, config::performance::dinos_refresh_ms) || cached_dinos.empty()) {
                cached_dinos = sdk::GameStatics->GetAllActorsOfClass(world, sdk::DinoCharacterClass);
                cached_dinos_tick = GetTickCount64();
                dinos_cache_refreshed = true;
            }
            diagnostics::set_dino_cache_stats(cached_dinos.size(), GetTickCount64() - cached_dinos_tick, dinos_cache_refreshed);
            return cached_dinos;
        }

        inline double clamp_double(double value, double min_value, double max_value) {
            if (value < min_value) return min_value;
            if (value > max_value) return max_value;
            return value;
        }

        inline bool is_digits_only(const std::wstring& token) {
            if (token.empty()) return false;
            for (wchar_t c : token) {
                if (!iswdigit(c)) return false;
            }
            return true;
        }

        inline std::wstring title_token(const std::wstring& token) {
            if (token.empty()) return token;

            std::wstring out = token;
            bool has_lower = false;
            for (wchar_t c : out) {
                if (iswlower(c)) {
                    has_lower = true;
                    break;
                }
            }

            if (!has_lower) {
                for (size_t i = 1; i < out.size(); ++i) {
                    out[i] = towlower(out[i]);
                }
            }
            out[0] = towupper(out[0]);
            return out;
        }

        inline std::wstring clean_dino_display_name(const FString& raw_name) {
            std::wstring name = raw_name.valid() ? raw_name.c_str() : L"";
            if (name.empty()) return L"Dino";

            // Remove package/class path if present.
            size_t dot = name.find_last_of(L'.');
            if (dot != std::wstring::npos && dot + 1 < name.size()) {
                name = name.substr(dot + 1);
            }

            // Strip UE instance suffixes: _C_123456, _123456, trailing _C.
            size_t suffix = name.rfind(L"_C_");
            if (suffix != std::wstring::npos) {
                bool numeric_tail = true;
                for (size_t i = suffix + 3; i < name.size(); ++i) {
                    if (!iswdigit(name[i])) {
                        numeric_tail = false;
                        break;
                    }
                }
                if (numeric_tail) {
                    name = name.substr(0, suffix);
                }
            }

            size_t last_us = name.find_last_of(L'_');
            if (last_us != std::wstring::npos && last_us + 1 < name.size()) {
                std::wstring tail = name.substr(last_us + 1);
                if (is_digits_only(tail)) {
                    name = name.substr(0, last_us);
                }
            }

            if (name.size() > 2 && name.substr(name.size() - 2) == L"_C") {
                name = name.substr(0, name.size() - 2);
            }

            std::vector<std::wstring> raw_tokens;
            size_t start = 0;
            while (start < name.size()) {
                size_t next = name.find(L'_', start);
                std::wstring token = (next == std::wstring::npos)
                    ? name.substr(start)
                    : name.substr(start, next - start);

                if (!token.empty()) raw_tokens.push_back(token);
                if (next == std::wstring::npos) break;
                start = next + 1;
            }

            std::vector<std::wstring> display_tokens;
            for (const auto& token : raw_tokens) {
                if (token.empty()) continue;
                if (token == L"Character") continue;
                if (token == L"BP") continue;
                if (token == L"C") continue;
                if (is_digits_only(token)) continue;
                display_tokens.push_back(title_token(token));
            }

            if (display_tokens.empty()) return L"Dino";

            std::wstring result;
            for (size_t i = 0; i < display_tokens.size(); ++i) {
                if (i) result += L" ";
                result += display_tokens[i];
            }
            return result;
        }

        inline std::wstring sanitize_display_text(const FString& raw_text) {
            if (!raw_text.valid() || !raw_text.c_str()) return L"";

            const wchar_t* src = raw_text.c_str();
            std::wstring out;
            out.reserve(64);
            for (size_t i = 0; i < 63 && src[i] != L'\0'; ++i) {
                wchar_t c = src[i];
                if (c == L'\n' || c == L'\r' || c == L'\t') {
                    out.push_back(L' ');
                    continue;
                }
                if (iswcntrl(c)) continue;
                out.push_back(c);
            }

            while (!out.empty() && iswspace(out.front())) out.erase(out.begin());
            while (!out.empty() && iswspace(out.back())) out.pop_back();
            return out;
        }

        inline std::wstring to_lower_copy(const std::wstring& value) {
            std::wstring out = value;
            for (wchar_t& c : out) c = towlower(c);
            return out;
        }

        inline bool contains_ci(const std::wstring& haystack, const wchar_t* needle) {
            if (!needle || !needle[0]) return false;
            return to_lower_copy(haystack).find(to_lower_copy(needle)) != std::wstring::npos;
        }

        inline bool is_dead_player_actor(APrimalCharacter* character) {
            if (!character) return true;
            if (character->IsDead()) return true;

            auto* shooter = reinterpret_cast<AShooterCharacter*>(character);
            if (shooter) {
                std::wstring player_name = sanitize_display_text(shooter->GetPlayerName());
                if (!player_name.empty()) {
                    std::wstring player_name_l = to_lower_copy(player_name);
                    if (player_name_l == L"last death" || player_name_l == L"death cache") {
                        return true;
                    }
                }
            }

            if (sdk::System) {
                std::wstring actor_name = sanitize_display_text(sdk::System->GetObjectName(character));
                if (!actor_name.empty()) {
                    if (contains_ci(actor_name, L"corpse") ||
                        contains_ci(actor_name, L"deadbody") ||
                        contains_ci(actor_name, L"deathcache")) {
                        return true;
                    }
                }
            }
            return false;
        }

        inline PlayerTribeInfo resolve_player_tribe_info(APrimalCharacter* target, APrimalCharacter* local_pawn) {
            PlayerTribeInfo info = {};
            if (!target) return info;

            info.tribe_id = target->GetTargetingTeam();
            info.has_valid_team = info.tribe_id > 0;
            info.tribe_name = sanitize_display_text(target->GetTribeName());

            const int local_team = local_pawn ? local_pawn->GetTargetingTeam() : 0;
            const bool has_local_team = local_team > 0;
            const bool same_team = has_local_team && info.has_valid_team && (local_team == info.tribe_id);

            bool allied = false;
            if (!same_team && has_local_team && info.has_valid_team && local_pawn) {
                allied = local_pawn->IsAlliedWithOtherTeam(info.tribe_id) ||
                    target->IsAlliedWithOtherTeam(local_team);
            }

            if (same_team || allied) {
                info.relation = TribeRelation::Friendly;
            }
            else if (!has_local_team || !info.has_valid_team) {
                info.relation = TribeRelation::Neutral;
            }
            else {
                info.relation = TribeRelation::Enemy;
            }

            return info;
        }

        inline bool should_show_relation(TribeRelation relation) {
            if (relation == TribeRelation::Friendly) return config::player_esp::show_friendly_tribes;
            if (relation == TribeRelation::Enemy) return config::player_esp::show_enemy_tribes;
            return config::player_esp::show_neutral_tribes;
        }

        inline FLinearColor tribe_relation_color(TribeRelation relation) {
            if (!config::player_esp::use_tribe_relation_color) return FLinearColor::White();

            if (relation == TribeRelation::Friendly) {
                return FLinearColor(
                    config::player_esp::tribe_friendly_color[0],
                    config::player_esp::tribe_friendly_color[1],
                    config::player_esp::tribe_friendly_color[2],
                    config::player_esp::tribe_friendly_color[3]
                );
            }
            if (relation == TribeRelation::Enemy) {
                return FLinearColor(
                    config::player_esp::tribe_enemy_color[0],
                    config::player_esp::tribe_enemy_color[1],
                    config::player_esp::tribe_enemy_color[2],
                    config::player_esp::tribe_enemy_color[3]
                );
            }
            return FLinearColor(
                config::player_esp::tribe_neutral_color[0],
                config::player_esp::tribe_neutral_color[1],
                config::player_esp::tribe_neutral_color[2],
                config::player_esp::tribe_neutral_color[3]
            );
        }

        inline std::wstring build_tribe_label(const PlayerTribeInfo& info) {
            if (!info.tribe_name.empty()) return info.tribe_name;
            if (config::player_esp::show_tribe_id && info.has_valid_team) return L"Tribe #" + std::to_wstring(info.tribe_id);
            return info.has_valid_team ? L"No Tribe" : L"Unknown";
        }

        inline FVector get_aim_location(APrimalCharacter* character) {
            if (!character) return FVector();

            FVector origin;
            FVector extent;
            character->GetActorBounds(true, &origin, &extent, false);

            FVector aim_location = character->GetActorLocation();
            if (extent.z <= 1.0) {
                aim_location.z += 50.0;
                return aim_location;
            }

            double bone_factor = 0.80; // head
            if (config::aimbot::bone::neck) bone_factor = 0.55;
            if (config::aimbot::bone::chest) bone_factor = 0.25;
            if (config::aimbot::bone::pelvis) bone_factor = -0.10;

            aim_location = origin;
            aim_location.z += extent.z * bone_factor;
            return aim_location;
        }

        inline bool compute_screen_box(APrimalCharacter* character, APlayerController* controller, ScreenBox* out_box) {
            if (!character || !controller || !out_box) return false;

            // Use onlyColliding=true for stable bounds (collision capsule only).
            // onlyColliding=false includes all mesh/particle/weapon components,
            // causing wildly changing box sizes during animations.
            FVector origin;
            FVector extent;
            character->GetActorBounds(true, &origin, &extent, false);

            // Fallback: if collision bounds are empty, use a fixed capsule estimate
            if (extent.x <= 1.0 && extent.y <= 1.0 && extent.z <= 1.0) {
                origin = character->GetActorLocation();
                extent = FVector(40.0, 40.0, 90.0); // reasonable humanoid capsule
            }

            FVector top_world(origin.x, origin.y, origin.z + extent.z);
            FVector bottom_world(origin.x, origin.y, origin.z - extent.z);
            FVector2D top_screen;
            FVector2D bottom_screen;

            if (!controller->ProjectWorldToScreen(top_world, &top_screen)) return false;
            if (!controller->ProjectWorldToScreen(bottom_world, &bottom_screen)) return false;

            double height = std::fabs(bottom_screen.y - top_screen.y);
            if (height < 4.0) return false;

            double center_x = (top_screen.x + bottom_screen.x) * 0.5;
            double center_y = (top_screen.y + bottom_screen.y) * 0.5;

            // Use a fixed aspect ratio for humanoids (width = ~45% of height)
            // This prevents box width from jumping based on animation pose
            double width = height * 0.45;

            double max_width = render::screen_size.x > 0.0 ? render::screen_size.x * 0.45 : width;
            double max_height = render::screen_size.y > 0.0 ? render::screen_size.y * 0.70 : height;
            width = clamp_double(width, 6.0, max_width);
            height = clamp_double(height, 8.0, max_height);

            out_box->top_left = FVector2D(center_x - (width * 0.5), center_y - (height * 0.5));
            out_box->bottom_right = FVector2D(center_x + (width * 0.5), center_y + (height * 0.5));
            out_box->width = width;
            out_box->height = height;
            out_box->valid = true;
            return true;
        }

        inline void draw_box(const ScreenBox& box, FLinearColor color, bool cornered = false) {
            if (!box.valid) return;
            if (cornered) {
                render::cornered_box(box.top_left, box.bottom_right, color, 2.0f);
            }
            else {
                render::box(box.top_left, box.bottom_right, color, 1.0f);
            }
        }

        // Bone indices for ARK SA humanoid skeleton
        // Adjust these if skeleton lines don't align correctly in-game
        namespace bones {
            constexpr int Head = 9;
            constexpr int Neck = 8;
            constexpr int Spine3 = 7;       // upper chest
            constexpr int Spine1 = 5;       // lower spine
            constexpr int Pelvis = 1;

            constexpr int L_UpperArm = 27;
            constexpr int L_ForeArm = 28;
            constexpr int L_Hand = 29;

            constexpr int R_UpperArm = 51;
            constexpr int R_ForeArm = 52;
            constexpr int R_Hand = 53;

            constexpr int L_Thigh = 67;
            constexpr int L_Calf = 68;
            constexpr int L_Foot = 69;

            constexpr int R_Thigh = 71;
            constexpr int R_Calf = 72;
            constexpr int R_Foot = 73;
        }

        struct BoneConnection { int from; int to; };
        inline constexpr BoneConnection skeleton_connections[] = {
            { bones::Head,       bones::Neck },
            { bones::Neck,       bones::Spine3 },
            { bones::Spine3,     bones::Spine1 },
            { bones::Spine1,     bones::Pelvis },
            // Arms
            { bones::Spine3,     bones::L_UpperArm },
            { bones::L_UpperArm, bones::L_ForeArm },
            { bones::L_ForeArm,  bones::L_Hand },
            { bones::Spine3,     bones::R_UpperArm },
            { bones::R_UpperArm, bones::R_ForeArm },
            { bones::R_ForeArm,  bones::R_Hand },
            // Legs
            { bones::Pelvis,     bones::L_Thigh },
            { bones::L_Thigh,    bones::L_Calf },
            { bones::L_Calf,     bones::L_Foot },
            { bones::Pelvis,     bones::R_Thigh },
            { bones::R_Thigh,    bones::R_Calf },
            { bones::R_Calf,     bones::R_Foot },
        };

        inline void draw_skeleton(APrimalCharacter* character, APlayerController* controller, FLinearColor color) {
            if (!character || !controller) return;

            USkeletalMeshComponent* mesh = nullptr;
            __try {
                mesh = character->GetMesh();
            } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
            if (!mesh) return;

            for (const auto& conn : skeleton_connections) {
                FVector bone_from, bone_to;
                __try {
                    bone_from = mesh->GetBoneLocation(conn.from);
                    bone_to = mesh->GetBoneLocation(conn.to);
                } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }

                // Skip if bone positions seem invalid (at origin)
                if (bone_from.x == 0 && bone_from.y == 0 && bone_from.z == 0) continue;
                if (bone_to.x == 0 && bone_to.y == 0 && bone_to.z == 0) continue;

                FVector2D screen_from, screen_to;
                if (!controller->ProjectWorldToScreen(bone_from, &screen_from)) continue;
                if (!controller->ProjectWorldToScreen(bone_to, &screen_to)) continue;

                render::line(screen_from, screen_to, color, 1.5f);
            }
        }

        inline void draw_health_bar(const ScreenBox& box, APrimalCharacter* character) {
            if (!box.valid || !character) return;

            float health = 0.0f, max_health = 0.0f;
            __try {
                health = character->GetHealth();
                max_health = character->GetMaxHealth();
            } __except (EXCEPTION_EXECUTE_HANDLER) { return; }

            if (max_health <= 0.0f) return;
            float ratio = health / max_health;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;

            // Draw health bar to the left of the ESP box
            float bar_width = 3.0f;
            float bar_height = (float)box.height;
            float bar_x = (float)box.top_left.x - bar_width - 3.0f;
            float bar_y = (float)box.top_left.y;

            // Background
            render::filled_box(FVector2D(bar_x - 1, bar_y - 1),
                FVector2D(bar_width + 2, bar_height + 2),
                FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

            // Health color: green → yellow → red
            FLinearColor bar_color;
            if (ratio > 0.5f) {
                float t = (ratio - 0.5f) * 2.0f;
                bar_color = FLinearColor(1.0f - t, 1.0f, 0.0f, 1.0f); // yellow→green
            } else {
                float t = ratio * 2.0f;
                bar_color = FLinearColor(1.0f, t, 0.0f, 1.0f); // red→yellow
            }

            // Filled portion (bottom-up)
            float fill_height = bar_height * ratio;
            float fill_y = bar_y + bar_height - fill_height;
            render::filled_box(FVector2D(bar_x, fill_y),
                FVector2D(bar_width, fill_height),
                bar_color);
        }

        // Cached FName indices for material parameter names (resolved once)
        inline FName g_fname_emissive_color = {};
        inline FName g_fname_emissive = {};
        inline FName g_fname_tint_color = {};
        inline bool g_highlight_names_resolved = false;

        inline void resolve_highlight_names() {
            if (g_highlight_names_resolved || !sdk::String) return;
            __try {
                g_fname_emissive_color = sdk::String->StringToName(L"EmissiveColor");
                g_fname_emissive = sdk::String->StringToName(L"Emissive");
                g_fname_tint_color = sdk::String->StringToName(L"TintColor");
                g_highlight_names_resolved = true;
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        inline void apply_highlight(APrimalCharacter* character, FLinearColor color, float intensity) {
            if (!character) return;

            USkeletalMeshComponent* mesh = nullptr;
            __try { mesh = character->GetMesh(); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
            if (!mesh) return;

            if (!g_highlight_names_resolved) resolve_highlight_names();

            // Multiply color by intensity for bright glow effect
            FLinearColor emissive(color.r * intensity, color.g * intensity, color.b * intensity, 1.0f);

            __try {
                // Try multiple common parameter names — whichever the material exposes will work
                mesh->SetVectorParamOnMaterials(g_fname_emissive_color, emissive);
                mesh->SetVectorParamOnMaterials(g_fname_emissive, emissive);
                mesh->SetVectorParamOnMaterials(g_fname_tint_color, emissive);

                // Also enable custom depth for potential future post-process outline
                mesh->SetRenderCustomDepth(true);
                mesh->SetCustomDepthStencilValue(255);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        inline void clear_highlight(APrimalCharacter* character) {
            if (!character) return;

            USkeletalMeshComponent* mesh = nullptr;
            __try { mesh = character->GetMesh(); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
            if (!mesh) return;

            if (!g_highlight_names_resolved) return;

            FLinearColor zero(0.0f, 0.0f, 0.0f, 1.0f);
            __try {
                mesh->SetVectorParamOnMaterials(g_fname_emissive_color, zero);
                mesh->SetVectorParamOnMaterials(g_fname_emissive, zero);
                mesh->SetVectorParamOnMaterials(g_fname_tint_color, zero);
                mesh->SetRenderCustomDepth(false);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // SEH-safe health getter (can't mix __try with std::wstring in same function)
        inline float safe_get_health(APrimalCharacter* character) {
            float h = 0.0f;
            __try { h = character->GetHealth(); } __except (EXCEPTION_EXECUTE_HANDLER) {}
            return h;
        }

        inline bool is_better_target(bool is_player, double distance_to_crosshair, double distance_to_camera) {
            if (!best_target.character) return true;

            if (config::aimbot::prioritize_players && is_player != best_target.is_player) {
                return is_player && !best_target.is_player;
            }

            constexpr double epsilon = 0.0001;
            constexpr double crosshair_switch_margin = 3.0;
            constexpr double distance_switch_margin = 1.25;
            if (target_mode() == 0) {
                if (distance_to_crosshair + crosshair_switch_margin + epsilon < best_target.distance_to_crosshair) return true;

                return std::fabs(distance_to_crosshair - best_target.distance_to_crosshair) <= epsilon &&
                    distance_to_camera + epsilon < best_target.distance_to_camera;
            }

            if (distance_to_camera + distance_switch_margin + epsilon < best_target.distance_to_camera) return true;

            return std::fabs(distance_to_camera - best_target.distance_to_camera) <= epsilon &&
                distance_to_crosshair + epsilon < best_target.distance_to_crosshair;
        }

        inline bool target_is_valid(APrimalCharacter* character, bool is_player, APlayerController* controller,
            FVector2D center, float fov, float camera_fov, FVector* out_location, FVector2D* out_screen) {
            if (!character || !controller) return false;
            if (is_player) {
                if (is_dead_player_actor(character)) return false;
            }
            else {
                if (character->IsDead()) return false;
            }

            if (is_player) {
                if (!config::player_esp::enabled) return false;
            }
            else {
                if (!config::dino_esp::enabled || !config::aimbot::target_wild_dinos) return false;
            }

            if (config::aimbot::visible_only && !character->WasRecentlyRendered(0.1f)) return false;

            FVector location = get_aim_location(character);
            FVector2D screen_pos;
            if (!controller->ProjectWorldToScreen(location, &screen_pos)) return false;
            if (!aimbot::in_fov(center, screen_pos, fov, camera_fov)) return false;

            if (out_location) *out_location = location;
            if (out_screen) *out_screen = screen_pos;
            return true;
        }

        inline void consider_target(APrimalCharacter* character, FVector location, FVector2D screen_pos,
            FVector2D center, FVector camera_location, float fov, float camera_fov, bool is_player) {
            if (!config::aimbot::enabled) return;

            if (config::aimbot::visible_only && !character->WasRecentlyRendered(0.1f)) return;

            if (!aimbot::in_fov(center, screen_pos, fov, camera_fov)) return;

            double dist_crosshair = sdk::Math->Distance2D(screen_pos, center);
            double dist_camera = sdk::Math->VectorDistance(location, camera_location) * 0.01;

            if (is_better_target(is_player, dist_crosshair, dist_camera)) {
                best_target.character = character;
                best_target.location = location;
                best_target.distance_to_crosshair = dist_crosshair;
                best_target.distance_to_camera = dist_camera;
                best_target.is_player = is_player;
            }
        }

        inline void process_players(UWorld* world, APlayerController* controller, APrimalCharacter* local_pawn,
            FVector camera_location, FVector2D center, float fov, float camera_fov) {
            diagnostics::ScopedTimer timer(&diagnostics::snap.players_scan_ms);
            if (!config::player_esp::enabled || !sdk::PlayerCharacterClass) return;

            const auto& players = get_player_actors(world);

            // Get local player name to filter out ghost copies (death caches, replicated duplicates)
            std::wstring local_player_name;
            if (local_pawn) {
                auto* local_shooter = reinterpret_cast<AShooterCharacter*>(local_pawn);
                if (local_shooter) {
                    local_player_name = sanitize_display_text(local_shooter->GetPlayerName());
                }
            }

            for (int i = 0; i < players.size(); i++) {
                if (!players.valid(i)) { diagnostics::add_player_skipped(); continue; }

                auto* character = (APrimalCharacter*)players[i];
                if (character == local_pawn || !character) { diagnostics::add_player_skipped(); continue; }
                if (is_dead_player_actor(character)) { diagnostics::add_player_skipped(); continue; }

                // Filter ghost copies: same player name as local player = death cache / duplicate
                if (!local_player_name.empty()) {
                    auto* shooter = reinterpret_cast<AShooterCharacter*>(character);
                    if (shooter) {
                        std::wstring other_name = sanitize_display_text(shooter->GetPlayerName());
                        if (!other_name.empty() && other_name == local_player_name) {
                            diagnostics::add_player_skipped();
                            continue;
                        }
                    }
                }

                FVector location = character->GetActorLocation();
                double distance = sdk::Math->VectorDistance(location, camera_location) * 0.01;
                if (distance > config::player_esp::max_distance) { diagnostics::add_player_skipped(); continue; }

                FVector aim_location = get_aim_location(character);
                FVector2D aim_screen_pos;
                if (!controller->ProjectWorldToScreen(aim_location, &aim_screen_pos)) {
                    diagnostics::inc_guard_projection_failure();
                    diagnostics::add_player_skipped();
                    continue;
                }
                if (!aim_screen_pos) { diagnostics::add_player_skipped(); continue; }

                FVector2D screen_pos;
                if (!controller->ProjectWorldToScreen(location, &screen_pos)) {
                    screen_pos = aim_screen_pos;
                }

                bool visible = character->WasRecentlyRendered(0.1f);
                FLinearColor color = visible ?
                    FLinearColor(config::player_esp::visible_color[0], config::player_esp::visible_color[1],
                        config::player_esp::visible_color[2], config::player_esp::visible_color[3]) :
                    FLinearColor(config::player_esp::color[0], config::player_esp::color[1],
                        config::player_esp::color[2], config::player_esp::color[3]);

                PlayerTribeInfo tribe_info = resolve_player_tribe_info(character, local_pawn);
                if (!should_show_relation(tribe_info.relation)) {
                    diagnostics::add_player_skipped();
                    continue;
                }

                if (config::radar::enabled && config::radar::show_players) {
                    radar::add_point(location, color, radar::EntityType::Player);
                }

                ScreenBox box;
                bool has_box = compute_screen_box(character, controller, &box);

                if ((config::player_esp::box || config::player_esp::cornered_box) && has_box) {
                    draw_box(box, color, config::player_esp::cornered_box);
                }

                if (config::player_esp::skeleton) {
                    draw_skeleton(character, controller, color);
                }

                if (config::player_esp::show_health && has_box) {
                    draw_health_bar(box, character);
                }

                if (config::player_esp::highlight) {
                    apply_highlight(character, color, config::player_esp::highlight_intensity);
                }

                // Material-based chams (through walls)
                if (config::chams::enabled && config::chams::players) {
                    features::chams::apply_player(character);
                }

                if (config::player_esp::snapline) {
                    render::line(FVector2D(center.x, render::screen_size.y), screen_pos, color, 1.0f);
                }

                float text_offset = 0;
                FVector2D text_pos(screen_pos.x, screen_pos.y + 5.0);
                if (has_box) {
                    text_pos = FVector2D((box.top_left.x + box.bottom_right.x) * 0.5, box.bottom_right.y + 5.0);
                }

                auto* shooter = reinterpret_cast<AShooterCharacter*>(character);
                std::wstring player_name = shooter ? sanitize_display_text(shooter->GetPlayerName()) : L"";
                if (player_name.empty()) player_name = L"Player";

                std::wstring tribe_label = config::player_esp::show_tribe ? build_tribe_label(tribe_info) : L"";
                FLinearColor tribe_color = tribe_relation_color(tribe_info.relation);
                int tribe_text_mode = config::player_esp::tribe_text_mode;
                if (tribe_text_mode < 0 || tribe_text_mode > 2) tribe_text_mode = 0;

                if (config::player_esp::show_tribe && !tribe_label.empty()) {
                    if (tribe_text_mode == 2) {
                        render::text(tribe_label.c_str(), text_pos, tribe_color, true, false, config::style::text_outlined);
                        text_offset += 15;
                    }
                    else if (tribe_text_mode == 1) {
                        std::wstring inline_line = config::player_esp::show_name ?
                            (player_name + L" [" + tribe_label + L"]") :
                            (L"[" + tribe_label + L"]");
                        render::text(inline_line.c_str(), text_pos, tribe_color, true, false, config::style::text_outlined);
                        text_offset += 15;
                    }
                    else {
                        if (config::player_esp::show_name) {
                            render::text(player_name.c_str(), text_pos, FLinearColor::White(), true, false, config::style::text_outlined);
                            text_offset += 15;
                        }
                        render::text(tribe_label.c_str(), FVector2D(text_pos.x, text_pos.y + text_offset),
                            tribe_color, true, false, config::style::text_outlined);
                        text_offset += 15;
                    }
                }
                else if (config::player_esp::show_name) {
                    render::text(player_name.c_str(), text_pos, FLinearColor::White(), true, false, config::style::text_outlined);
                    text_offset += 15;
                }

                if (config::player_esp::show_distance) {
                    auto dist_str = sdk::String->BuildStringDouble(L"", L"[", std::round(distance), L"m]");
                    render::text(dist_str.c_str(), FVector2D(text_pos.x, text_pos.y + text_offset),
                        FLinearColor::White(), true, false, config::style::text_outlined);
                }

                consider_target(character, aim_location, aim_screen_pos, center, camera_location, fov, camera_fov, true);
                diagnostics::add_player_processed();
            }
        }

        inline void process_dinos(UWorld* world, APlayerController* controller, APrimalCharacter* local_pawn,
            FVector camera_location, FVector2D center, float fov, float camera_fov) {
            diagnostics::ScopedTimer timer(&diagnostics::snap.dinos_scan_ms);
            if (!config::dino_esp::enabled || !sdk::DinoCharacterClass) return;

            const auto& dinos = get_dino_actors(world);

            for (int i = 0; i < dinos.size(); i++) {
                if (!dinos.valid(i)) { diagnostics::add_dino_skipped(); continue; }

                auto* dino = (APrimalCharacter*)dinos[i];
                if (!dino) { diagnostics::add_dino_skipped(); continue; }
                if (dino->IsDead()) { diagnostics::add_dino_skipped(); continue; }

                // Extra death check: health <= 0 catches dying/ragdoll states IsDead() may miss
                if (safe_get_health(dino) <= 0.0f) { diagnostics::add_dino_skipped(); continue; }

                FVector location = dino->GetActorLocation();
                double distance = sdk::Math->VectorDistance(location, camera_location) * 0.01;
                if (distance > config::dino_esp::max_distance) { diagnostics::add_dino_skipped(); continue; }

                FVector aim_location = get_aim_location(dino);
                FVector2D aim_screen_pos;
                if (!controller->ProjectWorldToScreen(aim_location, &aim_screen_pos)) {
                    diagnostics::inc_guard_projection_failure();
                    diagnostics::add_dino_skipped();
                    continue;
                }
                if (!aim_screen_pos) { diagnostics::add_dino_skipped(); continue; }

                FVector2D screen_pos;
                if (!controller->ProjectWorldToScreen(location, &screen_pos)) {
                    screen_pos = aim_screen_pos;
                }

                // Detect tamed vs wild: tamed dinos have a tribe name, wild don't
                int dino_team = dino->GetTargetingTeam();
                std::wstring dino_tribe = sanitize_display_text(dino->GetTribeName());
                bool is_wild = dino_tribe.empty();

                // Determine if friendly (same team/allied) or enemy tamed
                bool is_friendly = false;
                if (!is_wild && local_pawn) {
                    int local_team = local_pawn->GetTargetingTeam();
                    if (local_team > 0 && dino_team > 0 && dino_team == local_team) {
                        is_friendly = true;
                    } else if (local_team > 0 && dino_team > 0 && local_pawn->IsAlliedWithOtherTeam(dino_team)) {
                        is_friendly = true;
                    }
                }

                // Filter based on settings:
                // Wild → show_wild, Friendly tamed → show_friendly, Enemy tamed → show_tamed
                if (is_wild && !config::dino_esp::show_wild) { diagnostics::add_dino_skipped(); continue; }
                if (!is_wild && is_friendly && !config::dino_esp::show_friendly) { diagnostics::add_dino_skipped(); continue; }
                if (!is_wild && !is_friendly && !config::dino_esp::show_tamed) { diagnostics::add_dino_skipped(); continue; }

                // Pick color based on type
                FLinearColor color;
                if (is_wild) {
                    color = FLinearColor(config::dino_esp::wild_color[0], config::dino_esp::wild_color[1],
                        config::dino_esp::wild_color[2], config::dino_esp::wild_color[3]);
                } else if (is_friendly) {
                    color = FLinearColor(config::dino_esp::friendly_color[0], config::dino_esp::friendly_color[1],
                        config::dino_esp::friendly_color[2], config::dino_esp::friendly_color[3]);
                } else {
                    color = FLinearColor(config::dino_esp::tamed_color[0], config::dino_esp::tamed_color[1],
                        config::dino_esp::tamed_color[2], config::dino_esp::tamed_color[3]);
                }

                if (config::radar::enabled && config::radar::show_dinos) {
                    radar::add_point(location, color, is_wild ? radar::EntityType::WildDino : radar::EntityType::TamedDino);
                }

                ScreenBox box;
                bool has_box = compute_screen_box(dino, controller, &box);

                if (config::dino_esp::box && has_box) {
                    draw_box(box, color, false);
                }

                if (config::dino_esp::highlight) {
                    apply_highlight(dino, color, config::dino_esp::highlight_intensity);
                }

                // Material-based chams (through walls)
                if (config::chams::enabled && config::chams::dinos) {
                    features::chams::apply_dino(dino);
                }

                float text_offset = 0;
                FVector2D text_pos(screen_pos.x, screen_pos.y + 5.0);
                if (has_box) {
                    text_pos = FVector2D((box.top_left.x + box.bottom_right.x) * 0.5, box.bottom_right.y + 5.0);
                }

                if (config::dino_esp::show_name) {
                    auto raw_name = sdk::System->GetObjectName(dino);
                    std::wstring display_name = clean_dino_display_name(raw_name);
                    render::text(display_name.c_str(), text_pos, FLinearColor::White(), true, false, config::style::text_outlined);
                    text_offset += 15;
                }

                if (config::dino_esp::show_distance) {
                    auto dist_str = sdk::String->BuildStringDouble(L"", L"[", std::round(distance), L"m]");
                    render::text(dist_str.c_str(), FVector2D(text_pos.x, text_pos.y + text_offset),
                        FLinearColor::White(), true, false, config::style::text_outlined);
                }

                if ((config::aimbot::target_wild_dinos && is_wild) ||
                    (config::aimbot::target_enemy_tamed && !is_wild && !is_friendly)) {
                    consider_target(dino, aim_location, aim_screen_pos, center, camera_location, fov, camera_fov, false);
                }
                diagnostics::add_dino_processed();
            }
        }

        inline void execute_aimbot(APlayerController* controller, FVector2D center, float width, float height,
            float fov, float camera_fov) {
            diagnostics::ScopedTimer timer(&diagnostics::snap.aim_ms);
            if (!config::aimbot::enabled) {
                clear_locked_target();
                diagnostics::set_target_state(false, false, false, target_mode(), 0.0, 0.0);
                return;
            }

            bool aiming = aimbot::should_aim();
            AimTarget active_target = best_target;

            if (config::aimbot::sticky_lock) {
                if (!aiming) {
                    clear_locked_target();
                }
                else {
                    FVector locked_location;
                    FVector2D locked_screen;

                    if (locked_target.character &&
                        target_is_valid(locked_target.character, locked_target.is_player, controller,
                            center, fov, camera_fov, &locked_location, &locked_screen)) {
                        active_target.character = locked_target.character;
                        active_target.location = locked_location;
                        active_target.distance_to_crosshair = sdk::Math->Distance2D(locked_screen, center);
                        active_target.is_player = locked_target.is_player;
                        locked_target.location = locked_location;
                    }
                    else {
                        clear_locked_target();
                        if (best_target.character) {
                            locked_target = best_target;
                            active_target = best_target;
                        }
                    }
                }
            }

            if (config::aimbot::target_line) {
                FVector2D target_screen;
                if (active_target.character && controller->ProjectWorldToScreen(active_target.location, &target_screen)) {
                    render::line(center, target_screen, FLinearColor::Red(), 1.0f);
                }
            }

            if (aiming && config::aimbot::use_mouse && active_target.character) {
                if (last_controller_target != active_target.character) {
                    last_controller_target = active_target.character;
                    aimbot::reset_aim_state();
                }

                if (!has_smoothed_aim_location || smoothed_target != active_target.character) {
                    smoothed_target = active_target.character;
                    smoothed_aim_location = active_target.location;
                    has_smoothed_aim_location = true;
                }
                else {
                    // Low-pass filter target motion to remove animation jitter from aim corrections.
                    // At smooth=1: alpha=0.65 (fast tracking), at smooth=10: alpha=0.15 (smooth)
                    double alpha = 0.5;
                    double smooth_factor = static_cast<double>(config::aimbot::smoothing);
                    if (smooth_factor > 0.0) {
                        alpha = 1.0 / (smooth_factor * 0.8 + 0.7);
                        if (alpha > 0.85) alpha = 0.85;
                    }

                    smoothed_aim_location.x += (active_target.location.x - smoothed_aim_location.x) * alpha;
                    smoothed_aim_location.y += (active_target.location.y - smoothed_aim_location.y) * alpha;
                    smoothed_aim_location.z += (active_target.location.z - smoothed_aim_location.z) * alpha;
                }

                FVector aim_location = smoothed_aim_location;
                if (aimbot::can_run_aim_update()) {
                    aimbot::aim_at(controller, aim_location, width, height, config::aimbot::smoothing);
                }
            }
            else {
                smoothed_target = nullptr;
                has_smoothed_aim_location = false;
                last_controller_target = nullptr;
                aimbot::reset_aim_state();
            }

            diagnostics::set_target_state(
                active_target.character != nullptr,
                config::aimbot::sticky_lock && locked_target.character != nullptr,
                active_target.is_player,
                target_mode(),
                active_target.distance_to_crosshair == DBL_MAX ? 0.0 : active_target.distance_to_crosshair,
                active_target.distance_to_camera == DBL_MAX ? 0.0 : active_target.distance_to_camera
            );
        }
    }
}
