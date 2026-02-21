#pragma once


#include "../sdk/sdk.h"
#include "../config/settings.h"
#include "../core/diagnostics.h"
#include "../gui/render.h"
#include <Windows.h>
#include <cmath>

namespace features {
    namespace aimbot {

        inline double prev_error_x = 0.0;
        inline double prev_error_y = 0.0;
        inline bool controller_state_valid = false;

        inline void reset_aim_state() {
            prev_error_x = 0.0;
            prev_error_y = 0.0;
            controller_state_valid = false;
        }

        inline void aim_at(APlayerController* controller, FVector target_location, float width, float height, float smooth) {
            FVector2D target_screen;
            if (!controller->ProjectWorldToScreen(target_location, &target_screen)) return;
            if (!target_screen) return;

            if (smooth < 1.0f) {
                smooth = 1.0f;
            }

            const double centerX = width * 0.5;
            const double centerY = height * 0.5;
            const double error_x = target_screen.x - centerX;
            const double error_y = target_screen.y - centerY;
            const double distance_to_center = std::sqrt((error_x * error_x) + (error_y * error_y));

            constexpr double pixel_deadzone = 1.5;
            if (std::fabs(error_x) < pixel_deadzone && std::fabs(error_y) < pixel_deadzone) {
                return;
            }

            // PD-style controller in screen space (no integral) for stable convergence without bounce.
            if (!controller_state_valid) {
                prev_error_x = error_x;
                prev_error_y = error_y;
                controller_state_valid = true;
            }

            // At smooth=1: kp=0.70 (very fast snap), at smooth=10: kp=0.12 (slow track)
            double kp = 1.0 / (static_cast<double>(smooth) * 1.2 + 0.2);
            double kd = 0.12;
            if (distance_to_center < 35.0) {
                kd *= 0.4;
            }

            const double d_error_x = error_x - prev_error_x;
            const double d_error_y = error_y - prev_error_y;
            prev_error_x = error_x;
            prev_error_y = error_y;

            double move_x = (kp * error_x) + (kd * d_error_x);
            double move_y = (kp * error_y) + (kd * d_error_y);

            double max_step = 40.0;
            if (smooth >= 5.0f) max_step = 20.0;
            if (move_x > max_step) move_x = max_step;
            if (move_x < -max_step) move_x = -max_step;
            if (move_y > max_step) move_y = max_step;
            if (move_y < -max_step) move_y = -max_step;

            LONG send_x = static_cast<LONG>(std::llround(move_x));
            LONG send_y = static_cast<LONG>(std::llround(move_y));
            diagnostics::set_aim_delta((double)send_x, (double)send_y);

            if (send_x == 0 && send_y == 0) return;

            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dx = send_x;
            input.mi.dy = send_y;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(1, &input, sizeof(INPUT));
        }


    inline bool should_aim() {
        return config::aimbot::enabled && (GetAsyncKeyState(config::aimbot::aim_key) & 0x8000);
    }

    inline bool can_run_aim_update() {
        static ULONGLONG last_tick = 0;
        float hz = config::performance::aim_update_hz;
        if (hz < 1.0f) hz = 1.0f;
        const ULONGLONG interval_ms = static_cast<ULONGLONG>(1000.0f / hz);
        const ULONGLONG now = GetTickCount64();
        if (now - last_tick < interval_ms) return false;
        last_tick = now;
        return true;
    }


        inline void draw_fov(FVector2D center, float fov, float camera_fov) {
            if (!config::aimbot::show_fov) return;

            float radius = (fov * (float)center.x / camera_fov) / 2.0f;
            render::circle(center, (int)radius, 64, FLinearColor(1.0f, 1.0f, 1.0f, 0.5f));
        }


        inline bool in_fov(FVector2D center, FVector2D target, float fov, float camera_fov) {
            float radius = (fov * (float)center.x / camera_fov) / 2.0f;
            return render::in_circle((int)center.x, (int)center.y, (int)radius, (int)target.x, (int)target.y);
        }

    }
}
