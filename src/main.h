#pragma once

#include "includes.h"

namespace ark {




    inline bool input_capture_applied = false;

    inline void release_menu_input_capture(APlayerController* controller) {
        if (!input_capture_applied || !controller) return;
        controller->SetIgnoreLookInput(false);
        controller->SetIgnoreMoveInput(false);
        controller->ResetIgnoreInputFlags();
        input_capture_applied = false;
    }

    void on_render(UGameViewportClient* viewport, UCanvas* canvas) {
        diagnostics::begin_frame();

        if (!canvas) {
            diagnostics::end_frame();
            return;
        }

        auto screen_size = FVector2D(canvas->ClipX(), canvas->ClipY());
        auto center = FVector2D(screen_size.x / 2, screen_size.y / 2);

        render::canvas = canvas;
        render::world = nullptr;
        render::controller = nullptr;
        render::screen_center = center;
        render::screen_size = screen_size;

        render::text(L"Prisme ASA", FVector2D(10.0, 10.0), FLinearColor(0.54f, 0.39f, 0.82f, 1.0f), false, false, true);

        if (render::is_vk_clicked(VK_F1)) {
            render::show_menu = !render::show_menu;
        }

        // Menu is now rendered via ImGui DX12 hook (dx_hook.h)
        // ZeroGUI menu draw removed — ImGui handles it directly on the swapchain

        if (!viewport) {
            diagnostics::end_frame();
            return;
        }

        auto* world = viewport->GetWorld();
        if (!world) {
            diagnostics::inc_guard_world_failure();
            diagnostics::end_frame();
            return;
        }

        auto* game_instance = sdk::GameStatics->GetGameInstance(world);
        if (!game_instance) {
            diagnostics::end_frame();
            return;
        }

        auto local_players = game_instance->GetLocalPlayers();
        if (local_players.empty()) {
            diagnostics::end_frame();
            return;
        }
        auto* local_player = local_players[0];
        if (!local_player) {
            diagnostics::end_frame();
            return;
        }

        auto* controller = sdk::GameStatics->GetPlayerController(world, 0);
        if (!controller) {
            diagnostics::inc_guard_controller_failure();
            diagnostics::end_frame();
            return;
        }

        auto* camera = sdk::GameStatics->GetPlayerCameraManager(world, 0);
        if (!camera) {
            diagnostics::inc_guard_camera_failure();
            release_menu_input_capture(controller);
            diagnostics::end_frame();
            return;
        }

        render::world = world;
        render::controller = controller;

        const bool want_capture = render::show_menu && config::style::capture_game_input_when_menu_open;
        if (want_capture) {
            controller->SetIgnoreLookInput(true);
            controller->SetIgnoreMoveInput(true);
            input_capture_applied = true;
        } else {
            release_menu_input_capture(controller);
        }

        FVector camera_location = camera->GetCameraLocation();
        FRotator camera_rotation = camera->GetCameraRotation();
        APrimalCharacter* local_pawn = controller->GetPawn();
        features::misc::apply_fov_changer(local_pawn);
        float camera_fov = camera->GetFOVAngle();

        if (config::radar::enabled && diagnostics::feature_enabled(diagnostics::FeaturePath::Radar)) {
            __try {
                features::radar::initialize(
                    FVector2D(config::radar::pos_x, config::radar::pos_y),
                    FVector2D(config::radar::size, config::radar::size),
                    camera_location,
                    camera_rotation
                );
                diagnostics::on_feature_success(diagnostics::FeaturePath::Radar);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                diagnostics::inc_guard_exception_failure();
                diagnostics::on_feature_failure(diagnostics::FeaturePath::Radar);
            }
        }

        features::aimbot::draw_fov(center, config::aimbot::fov, camera_fov);
        features::esp::reset_target();
        if (diagnostics::feature_enabled(diagnostics::FeaturePath::PlayersEsp)) {
            __try {
                features::esp::process_players(world, controller, local_pawn, camera_location, center, config::aimbot::fov, camera_fov);
                diagnostics::on_feature_success(diagnostics::FeaturePath::PlayersEsp);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                diagnostics::inc_guard_exception_failure();
                diagnostics::on_feature_failure(diagnostics::FeaturePath::PlayersEsp);
            }
        }
        if (diagnostics::feature_enabled(diagnostics::FeaturePath::DinosEsp)) {
            __try {
                features::esp::process_dinos(world, controller, local_pawn, camera_location, center, config::aimbot::fov, camera_fov);
                diagnostics::on_feature_success(diagnostics::FeaturePath::DinosEsp);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                diagnostics::inc_guard_exception_failure();
                diagnostics::on_feature_failure(diagnostics::FeaturePath::DinosEsp);
            }
        }
        if (diagnostics::feature_enabled(diagnostics::FeaturePath::Aimbot)) {
            __try {
                features::esp::execute_aimbot(controller, center, (float)screen_size.x, (float)screen_size.y, config::aimbot::fov, camera_fov);
                diagnostics::on_feature_success(diagnostics::FeaturePath::Aimbot);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                diagnostics::inc_guard_exception_failure();
                diagnostics::on_feature_failure(diagnostics::FeaturePath::Aimbot);
            }
        }

        // Runtime debug overlay
        if (config::debug::show_info) {
            const auto& d = diagnostics::get_snapshot();
            FVector2D debug_pos(10, 60);
            wchar_t line[128];

            swprintf_s(line, L"Frame ms: %.2f", d.frame_ms);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;
            swprintf_s(line, L"FPS: %.0f", d.fps);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 18;

            swprintf_s(line, L"Players scan ms: %.2f", d.players_scan_ms);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;
            swprintf_s(line, L"Dinos scan ms: %.2f", d.dinos_scan_ms);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;
            swprintf_s(line, L"Aim ms: %.2f", d.aim_ms);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 18;

            swprintf_s(line, L"Players: %d processed / %d skipped", d.players_processed, d.players_skipped);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;
            swprintf_s(line, L"Dinos: %d processed / %d skipped", d.dinos_processed, d.dinos_skipped);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 18;

            swprintf_s(line, L"Player cache: %d entries age %llums", d.players_cached, (unsigned long long)d.players_cache_age_ms);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;
            swprintf_s(line, L"Dino cache: %d entries age %llums", d.dinos_cached, (unsigned long long)d.dinos_cache_age_ms);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 18;

            swprintf_s(line, L"Guard: world %d ctrl %d cam %d proj %d ex %d",
                d.guard_world_failures, d.guard_controller_failures, d.guard_camera_failures,
                d.guard_projection_failures, d.guard_exception_failures);
            render::text(line, debug_pos, d.guard_exception_failures ? FLinearColor::Yellow() : FLinearColor::White(), false, false, true);
            debug_pos.y += 15;

            swprintf_s(line, L"Profile slot: %d | %S", d.active_profile_slot, d.config_status);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;

            auto target = d.has_target ? (d.target_is_player ? L"Target: Player" : L"Target: Dino") : L"Target: None";
            render::text(target, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;

            swprintf_s(line, L"Aim delta: %.0f, %.0f", d.aim_delta_x, d.aim_delta_y);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
            debug_pos.y += 15;

            if (render::show_menu) {
                render::text(input_capture_applied ? L"Menu Focus: Input Captured" : L"Menu Focus: Capture Pending", debug_pos, FLinearColor::White(), false, false, true);
                debug_pos.y += 15;
            }

            auto& fs = d.feature_states;
            swprintf_s(line, L"Disabled events R/P/D/A: %d/%d/%d/%d",
                fs[(int)diagnostics::FeaturePath::Radar].disable_events,
                fs[(int)diagnostics::FeaturePath::PlayersEsp].disable_events,
                fs[(int)diagnostics::FeaturePath::DinosEsp].disable_events,
                fs[(int)diagnostics::FeaturePath::Aimbot].disable_events);
            render::text(line, debug_pos, FLinearColor::White(), false, false, true);
        }

        diagnostics::end_frame();
    }


    namespace hooks {
        using DrawTransition_t = void(*)(UGameViewportClient*, UCanvas*);
        DrawTransition_t DrawTransition_Original = nullptr;

        volatile int    scan_index = -1;
        volatile bool   scan_waiting = true;
        volatile void*  scan_arg1 = nullptr;   // 'this' pointer
        volatile void*  scan_arg2 = nullptr;   // first real argument

        void __cdecl Scan_Probe(void* self, void* arg1) {
            scan_arg1 = self;
            scan_arg2 = arg1;
            scan_waiting = false;
        }

        void DrawTransition_Hook(UGameViewportClient* viewport, UCanvas* canvas) {
            static bool logged_first = false;
            if (!logged_first) {
                dbg::log_ex(dbg::Level::Info, dbg::Hook, "DrawTransition hook active viewport=0x%p canvas=0x%p", viewport, canvas);
                logged_first = true;
            }

            __try {
                on_render(viewport, canvas);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                static int err = 0;
                if (++err <= 5) {
                    dbg::log_ex(dbg::Level::Error, dbg::Render, "Exception in on_render #%d canvas=0x%p font=0x%p", err, canvas, render::font);
                }
            }
            return DrawTransition_Original(viewport, canvas);
        }

        int find_draw_transition_index(std::uintptr_t viewport_addr, int start, int end) {
            auto vtable = *(std::uintptr_t**)viewport_addr;
            if (!vtable) return -1;

            // Get vtable size
            int vtable_size = 0;
            __try {
                while (*(std::uintptr_t*)(std::uintptr_t(vtable) + (vtable_size * 8)))
                    vtable_size++;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}

            dbg::log_ex(dbg::Level::Debug, dbg::Hook, "Scanning vtable %d-%d (size=%d)", start, end, vtable_size);

            for (int idx = start; idx <= end && idx < vtable_size; idx++) {
                auto original_fn = vtable[idx];
                if (!original_fn) continue;

                // Install probe
                scan_waiting = true;
                scan_arg1 = nullptr;
                scan_arg2 = nullptr;
                scan_index = idx;

                // Swap vtable entry with our probe
                DWORD old_protect;
                VirtualProtect(&vtable[idx], sizeof(std::uintptr_t), PAGE_READWRITE, &old_protect);
                vtable[idx] = (std::uintptr_t)&Scan_Probe;
                VirtualProtect(&vtable[idx], sizeof(std::uintptr_t), old_protect, &old_protect);

                // Wait for a call (max ~200ms)
                for (int w = 0; w < 20 && scan_waiting; w++) {
                    Sleep(10);
                }

                // Restore original
                VirtualProtect(&vtable[idx], sizeof(std::uintptr_t), PAGE_READWRITE, &old_protect);
                vtable[idx] = original_fn;
                VirtualProtect(&vtable[idx], sizeof(std::uintptr_t), old_protect, &old_protect);

                if (scan_waiting) {
                    continue;
                }

                // Check if arg2 = UCanvas (ClipX/ClipY)
                void* candidate = (void*)scan_arg2;
                if (!candidate) continue;

                __try {
                    float clipX = *(float*)((std::uintptr_t)candidate + offsets::Canvas_ClipX);
                    float clipY = *(float*)((std::uintptr_t)candidate + offsets::Canvas_ClipY);

                    dbg::log_ex(dbg::Level::Debug, dbg::Hook, "idx=%d arg2=0x%p clipX=%.1f clipY=%.1f", idx, candidate, clipX, clipY);

                    if (clipX > 100.0f && clipX < 8000.0f && clipY > 100.0f && clipY < 5000.0f) {
                        dbg::log_ex(dbg::Level::Info, dbg::Hook, "DrawTransition index=%d (%.0fx%.0f)", idx, clipX, clipY);
                        return idx;
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {

                }
            }
            return -1;
        }
    }


    void initialize() {
        dbg::init();
        dbg::set_min_level(dbg::Level::Info);
        dbg::enable_category(dbg::Render, false);
        dbg::enable_category(dbg::Aimbot, false);
        dbg::enable_category(dbg::Cache, false);
        dbg::enable_category(dbg::Hook, false);
        dbg::enable_category(dbg::SDK, false);
        dbg::log_ex(dbg::Level::Info, dbg::Init, "Initialization start");

        if (config::profiles::initialize()) {
            dbg::log_ex(dbg::Level::Info, dbg::Config, "Loaded profile slot %d", config::profiles::get_active_slot());
        } else {
            dbg::log_ex(dbg::Level::Warn, dbg::Config, "Profile load failed, using in-memory defaults");
        }

        init_status::current_step = "Finding game module";

        dbg::log_ex(dbg::Level::Info, dbg::Init, "Resolving game module");
        game = module::get_module(L"ArkAscended.exe");
        if (!game) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "ArkAscended.exe not found, trying ShooterGame.exe");
            game = module::get_module(L"ShooterGame.exe");
        }
        if (!game) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "ShooterGame.exe not found, trying ShooterGameServer.exe");
            game = module::get_module(L"ShooterGameServer.exe");
        }
        if (!game) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "Could not find any game module");
            init_status::last_error = "Game module not found";
            return;
        }

        init_status::module_found = true;
        dbg::log_ex(dbg::Level::Info, dbg::Init, "Game module resolved at 0x%llX", (unsigned long long)game);

        offsets::validate_offsets(game);

        init_status::current_step = "Initializing SDK";
        dbg::log("Initializing SDK...");
        bool sdk_ok = false;
        __try {
            sdk_ok = sdk::Initialize();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::error("EXCEPTION during SDK::Initialize()! An offset is likely wrong.");
            dbg::error("Check the offset validation above for OUT OF RANGE or NOT READABLE entries.");
            init_status::last_error = "SDK init crashed (exception)";
            return;
        }
        if (!sdk_ok) {
            dbg::error("SDK initialization returned false - check errors above.");
            init_status::last_error = "SDK init failed";
            return;
        }
        init_status::sdk_initialized = true;

        // Wait for the game world to load
        init_status::current_step = "Waiting for game world";
        dbg::log("Waiting for game world to load (max 120s)...");

        UObject* world = nullptr;
        for (int attempt = 0; attempt < 240; attempt++) {
            __try {
                world = UObject::FindObject(L"PersistentLevel", reinterpret_cast<UObject*>(-1));
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                world = nullptr;
            }

            if (world) {
                dbg::success("World found on attempt %d (%.1fs)", attempt + 1, attempt * 0.5f);
                break;
            }

            if (attempt % 20 == 0) {
                dbg::log("Still waiting for world... (attempt %d/%d)", attempt + 1, 240);
            }

            Sleep(500);
        }

        if (!world) {
            dbg::error("FATAL: Could not find game world after 120 seconds!");
            dbg::error("This likely means the offsets are wrong for this game version.");
            dbg::error("StaticFindObject offset: 0x%llX", (unsigned long long)offsets::StaticFindObject);
            init_status::last_error = "World not found after timeout";
            return;
        }
        init_status::world_found = true;

        // Get game instance
        init_status::current_step = "Setting up game instance";
        auto* game_instance = sdk::GameStatics->GetGameInstance(world);
        if (!game_instance) {
            dbg::error("FATAL: GetGameInstance returned null!");
            init_status::last_error = "GameInstance is null";
            return;
        }
        dbg::dump_ptr("GameInstance", game_instance);

        auto local_players = game_instance->GetLocalPlayers();
        if (local_players.empty()) {
            dbg::error("FATAL: No local players found!");
            init_status::last_error = "No local players";
            return;
        }
        dbg::log("Found %d local player(s)", local_players.size());

        auto* local_player = local_players[0];
        if (!local_player) {
            dbg::error("FATAL: LocalPlayer[0] is null!");
            init_status::last_error = "LocalPlayer is null";
            return;
        }
        dbg::dump_ptr("LocalPlayer", local_player);

        // Get viewport
        init_status::current_step = "Getting viewport";
        auto* viewport = local_player->GetViewport();
        if (!viewport) {
            dbg::error("FATAL: Could not get viewport!");
            dbg::error("LocalPlayer->Viewport offset may be wrong (currently 0x%llX)", (unsigned long long)offsets::LocalPlayer_Viewport);
            init_status::last_error = "Viewport is null";
            return;
        }
        init_status::viewport_found = true;
        dbg::dump_ptr("Viewport", viewport);

        // Get font from engine
        init_status::current_step = "Getting engine font";
        auto* engine = (UEngine*)sdk::System->GetOuterObject(game_instance);
        if (engine) {
            render::font = engine->GetFont();
            if (render::font) {
                init_status::font_found = true;
                dbg::success("Engine font found at 0x%p", render::font);
            } else {
                dbg::warn("Engine font is NULL — text rendering may not work");
                dbg::warn("Engine->Font offset may be wrong (currently 0x%llX)", (unsigned long long)offsets::Engine_Font);
            }
        } else {
            dbg::warn("Could not get UEngine from game instance");
        }

        // Scan for the correct DrawTransition vtable index
        init_status::current_step = "Scanning for DrawTransition vtable index";
        dbg::info("Scanning for DrawTransition vtable index (current guess: %d)...", offsets::DrawTransitionVIdx);

        int found_idx = hooks::find_draw_transition_index(std::uintptr_t(viewport), 100, 140);

        if (found_idx < 0) {
            dbg::warn("Could not auto-detect DrawTransition index in 100-140, trying wider range 60-180...");
            found_idx = hooks::find_draw_transition_index(std::uintptr_t(viewport), 60, 99);
            if (found_idx < 0) {
                found_idx = hooks::find_draw_transition_index(std::uintptr_t(viewport), 141, 180);
            }
        }

        // DX12 ImGui hook — independent of DrawTransition, renders ImGui on game's swapchain
        // Must init BEFORE DrawTransition scan since that scan can fail
        if (dx_hook::initialize()) {
            dbg::log_ex(dbg::Level::Info, dbg::Init, "DX12 ImGui hook initialized");
        } else {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "DX12 ImGui hook failed — menu will not be available");
        }

        if (found_idx < 0) {
            dbg::error("Could not find DrawTransition vtable index — ESP/aimbot drawing disabled");
            dbg::error("ImGui menu still available via DX hook (F1)");
            init_status::last_error = "DrawTransition index not found";
        } else {
            dbg::success("DrawTransition vtable index found: %d", found_idx);

            // Now install the real hook at the correct index
            init_status::current_step = "Installing DrawTransition hook";
            dbg::log("Installing DrawTransition hook at vtable index %d...", found_idx);

            hooks::DrawTransition_Original = ::vmt<hooks::DrawTransition_t>(
                std::uintptr_t(viewport),
                std::uintptr_t(hooks::DrawTransition_Hook),
                found_idx
            );

            if (hooks::DrawTransition_Original) {
                init_status::hook_installed = true;
                dbg::success("DrawTransition hook installed! Original=0x%p", hooks::DrawTransition_Original);
            } else {
                dbg::error("DrawTransition hook FAILED!");
                init_status::last_error = "Hook installation failed";
            }
        }

        init_status::current_step = "Complete";
        dbg::log_ex(dbg::Level::Info, dbg::Init, "Initialization complete");
        dbg::log_ex(dbg::Level::Info, dbg::Init, "Press F1 to toggle menu");
    }

}


namespace payson1337 {
    static void init() {
        ark::initialize();
    }
}
