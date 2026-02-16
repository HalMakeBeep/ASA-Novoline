#pragma once

#include "../sdk/sdk.h"
#include "../config/settings.h"
#include "../core/memory.h"

namespace features {
    namespace misc {

        inline bool has_original_fov = false;
        inline std::uintptr_t last_camera_component = 0;
        inline float original_fov = 90.0f;

        inline float clamp_float(float value, float min_value, float max_value) {
            if (value < min_value) return min_value;
            if (value > max_value) return max_value;
            return value;
        }

        inline void restore_original_fov_if_needed() {
            if (!has_original_fov || !last_camera_component) return;

            const std::uintptr_t fov_addr = last_camera_component + offsets::CameraComponent_FieldOfView;
            if (memory::is_valid_ptr(fov_addr)) {
                memory::write<float>(fov_addr, original_fov);
            }

            has_original_fov = false;
            last_camera_component = 0;
        }

        inline void apply_fov_changer(APrimalCharacter* local_pawn) {
            if (!config::misc::fov_changer) {
                restore_original_fov_if_needed();
                return;
            }

            if (!local_pawn || !sdk::CameraComponentClass) return;

            auto* camera_component = local_pawn->GetComponentByClass(sdk::CameraComponentClass);
            if (!camera_component) return;

            const std::uintptr_t camera_component_addr = std::uintptr_t(camera_component);
            const std::uintptr_t fov_addr = camera_component_addr + offsets::CameraComponent_FieldOfView;
            if (!memory::is_valid_ptr(fov_addr)) return;

            if (!has_original_fov || last_camera_component != camera_component_addr) {
                if (has_original_fov && last_camera_component && last_camera_component != camera_component_addr) {
                    const std::uintptr_t old_fov_addr = last_camera_component + offsets::CameraComponent_FieldOfView;
                    if (memory::is_valid_ptr(old_fov_addr)) {
                        memory::write<float>(old_fov_addr, original_fov);
                    }
                }
                float current_fov = memory::read<float>(fov_addr, 90.0f);
                if (current_fov < 1.0f || current_fov > 300.0f) current_fov = 90.0f;
                original_fov = current_fov;
                has_original_fov = true;
                last_camera_component = camera_component_addr;
            }

            config::misc::fov_value = clamp_float(config::misc::fov_value, 30.0f, 170.0f);
            memory::write<float>(fov_addr, config::misc::fov_value);
        }
    }
}
