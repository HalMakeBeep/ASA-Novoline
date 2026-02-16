#pragma once

#include "../sdk/sdk.h"
#include "../gui/render.h"
#include "../config/settings.h"

namespace features {
namespace radar {

    inline FVector2D position;
    inline FVector2D size;
    inline FVector camera_location;
    inline FRotator camera_rotation;

    inline void clamp_to_range(double* x, double* y, double range) {
        if (std::abs(*x) > range || std::abs(*y) > range) {
            if (*y > *x) {
                if (*y > -*x) {
                    *x = range * (*x) / (*y);
                    *y = range;
                } else {
                    *y = -range * (*y) / (*x);
                    *x = -range;
                }
            } else {
                if (*y > -*x) {
                    *y = range * (*y) / (*x);
                    *x = range;
                } else {
                    *x = -range * (*x) / (*y);
                    *y = -range;
                }
            }
        }
    }

    inline FVector2D world_to_radar(FVector world_pos) {
        double yaw = camera_rotation.yaw * 3.14159265 / 180.0;

        double dx = world_pos.x - camera_location.x;
        double dy = world_pos.y - camera_location.y;

        double sin_yaw = std::sin(yaw);
        double cos_yaw = -std::cos(yaw);

        double x = -(dy * cos_yaw + dx * sin_yaw);
        double y = dx * cos_yaw - dy * sin_yaw;

        double range_value = config::radar::range * 1000.0;
        clamp_to_range(&x, &y, range_value);

        double out_x = position.x + (size.x / 2.0 + x / range_value * size.x);
        double out_y = position.y + (size.y / 2.0 + y / range_value * size.y);

        // Clamp to radar bounds
        out_x = std::max(position.x, std::min(out_x, position.x + size.x - 5));
        out_y = std::max(position.y, std::min(out_y, position.y + size.y - 5));

        return FVector2D(out_x, out_y);
    }

    inline void add_point(FVector world_pos, FLinearColor color) {
        FVector2D screen = world_to_radar(world_pos);
        render::filled_box(screen, FVector2D(4, 4), color);
    }

    inline void initialize(FVector2D pos, FVector2D sz, FVector cam_loc, FRotator cam_rot) {
        position = pos;
        size = sz;
        camera_location = cam_loc;
        camera_rotation = cam_rot;

        // Draw background
        render::filled_box(pos, sz, FLinearColor(0.025f, 0.025f, 0.025f, 0.8f));

        // Draw crosshairs
        render::line(FVector2D(pos.x + sz.x / 2, pos.y), FVector2D(pos.x + sz.x / 2, pos.y + sz.y),
            FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
        render::line(FVector2D(pos.x, pos.y + sz.y / 2), FVector2D(pos.x + sz.x, pos.y + sz.y / 2),
            FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), 1.0f);
    }

} // namespace radar
} // namespace features
