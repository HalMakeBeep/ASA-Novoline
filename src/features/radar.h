#pragma once

#include "../sdk/sdk.h"
#include "../gui/render.h"
#include "../config/settings.h"

namespace features {
namespace radar {

    enum class EntityType { Player, TamedDino, WildDino };

    inline FVector2D position;   // top-left of bounding box
    inline FVector2D size;       // width=height (square bounding box)
    inline FVector camera_location;
    inline FRotator camera_rotation;
    inline float yaw_rad = 0.0f;
    inline float center_x = 0.0f;
    inline float center_y = 0.0f;
    inline float radius = 0.0f;

    // Clamp point to circle (instead of square)
    inline void clamp_to_circle(double* x, double* y, double max_r) {
        double dist = std::sqrt((*x) * (*x) + (*y) * (*y));
        if (dist > max_r) {
            *x = *x / dist * max_r;
            *y = *y / dist * max_r;
        }
    }

    inline FVector2D world_to_radar(FVector world_pos) {
        double dx = world_pos.x - camera_location.x;
        double dy = world_pos.y - camera_location.y;

        // UE coordinate system: yaw=0 is +X, +Y is right
        // Rotate so "up" on radar = camera forward direction
        double sin_yaw = std::sin(yaw_rad);
        double cos_yaw = std::cos(yaw_rad);

        // x_rel = right on radar, y_rel = up on radar (forward)
        double x_rel = dy * cos_yaw - dx * sin_yaw;
        double y_rel = -(dx * cos_yaw + dy * sin_yaw);

        double range_value = config::radar::range * 1000.0;
        clamp_to_circle(&x_rel, &y_rel, range_value);

        double usable_r = radius - 6.0;
        double out_x = center_x + x_rel / range_value * usable_r;
        double out_y = center_y + y_rel / range_value * usable_r;

        // Clamp to circle
        double off_x = out_x - center_x;
        double off_y = out_y - center_y;
        double dist = std::sqrt(off_x * off_x + off_y * off_y);
        if (dist > usable_r) {
            out_x = center_x + off_x / dist * usable_r;
            out_y = center_y + off_y / dist * usable_r;
        }

        return FVector2D(out_x, out_y);
    }

    // Draw a small diamond shape (for tamed dinos)
    inline void draw_diamond(FVector2D c, float r, FLinearColor color) {
        FVector2D top(c.x, c.y - r);
        FVector2D right(c.x + r, c.y);
        FVector2D bottom(c.x, c.y + r);
        FVector2D left(c.x - r, c.y);
        render::line(top, right, color, 1.5f);
        render::line(right, bottom, color, 1.5f);
        render::line(bottom, left, color, 1.5f);
        render::line(left, top, color, 1.5f);
    }

    // Draw a small triangle shape (for wild dinos)
    inline void draw_triangle(FVector2D c, float r, FLinearColor color) {
        FVector2D top(c.x, c.y - r);
        FVector2D bl(c.x - r * 0.866f, c.y + r * 0.5f);
        FVector2D br(c.x + r * 0.866f, c.y + r * 0.5f);
        render::line(top, br, color, 1.5f);
        render::line(br, bl, color, 1.5f);
        render::line(bl, top, color, 1.5f);
    }

    // Draw a filled circle dot with outline (for players)
    inline void draw_player_dot(FVector2D c, float r, FLinearColor color) {
        render::circle(c, (int)(r + 1), 12, FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
        render::circle(c, (int)r, 12, color);
        render::circle(c, (int)(r * 0.5f), 8, FLinearColor(1.0f, 1.0f, 1.0f, 0.6f));
    }

    inline void add_point(FVector world_pos, FLinearColor color, EntityType type = EntityType::Player) {
        FVector2D screen = world_to_radar(world_pos);
        float dot_scale = (float)(size.x / 200.0);
        if (dot_scale < 0.8f) dot_scale = 0.8f;
        if (dot_scale > 2.0f) dot_scale = 2.0f;

        switch (type) {
        case EntityType::Player:
            draw_player_dot(screen, 4.0f * dot_scale, color);
            break;
        case EntityType::TamedDino:
            draw_diamond(screen, 4.0f * dot_scale, color);
            break;
        case EntityType::WildDino:
            draw_triangle(screen, 3.5f * dot_scale, color);
            break;
        }
    }

    // Draw filled circle background using scanline fill
    inline void draw_filled_circle(float cx, float cy, float r, FLinearColor color) {
        int ir = (int)r;
        for (int y = -ir; y <= ir; y++) {
            float half_w = std::sqrt(r * r - (float)(y * y));
            render::line(
                FVector2D(cx - half_w, cy + y),
                FVector2D(cx + half_w, cy + y),
                color, 1.0f
            );
        }
    }

    inline void draw_compass() {
        float label_offset = radius - 12.0f;

        // UE: yaw=0 is +X. In ARK, +Y is East, +X is North (standard UE convention)
        // On our radar, "up" = camera forward. Cardinal directions rotate with camera.
        // angle=0 means "up on radar". We offset by the world angle of each cardinal.
        // North = +X in UE = world angle 0 for yaw. The radar rotation already
        // accounts for yaw, so cardinal world angles relative to yaw:
        struct { const wchar_t* label; float world_angle; FLinearColor color; } cardinals[] = {
            { L"N", 0.0f,                FLinearColor(1.0f, 0.35f, 0.35f, 0.95f) },
            { L"E", 3.14159f * 0.5f,     FLinearColor(0.7f, 0.7f, 0.7f, 0.7f) },
            { L"S", 3.14159f,            FLinearColor(0.7f, 0.7f, 0.7f, 0.7f) },
            { L"W", 3.14159f * 1.5f,     FLinearColor(0.7f, 0.7f, 0.7f, 0.7f) },
        };

        for (auto& c : cardinals) {
            // Angle on radar = world_angle - camera_yaw
            // sin/cos give position on the circle edge
            float a = c.world_angle - yaw_rad;
            float lx = center_x + std::sin(a) * label_offset;
            float ly = center_y - std::cos(a) * label_offset;

            // Check if within circular bounds
            float dx = lx - center_x;
            float dy = ly - center_y;
            if (dx * dx + dy * dy < (radius - 4) * (radius - 4)) {
                render::text(c.label, FVector2D(lx, ly), c.color, true, true, true);
            }
        }
    }

    inline void draw_grid_circles() {
        FLinearColor ring_color(0.25f, 0.25f, 0.30f, 0.3f);
        // 3 concentric range rings at 25%, 50%, 75%
        for (int i = 1; i <= 3; i++) {
            float r = radius * (float)i / 4.0f;
            render::circle(FVector2D(center_x, center_y), (int)r, 24, ring_color);
        }
    }

    inline void draw_center_indicator() {
        FLinearColor col(0.4f, 0.9f, 1.0f, 0.9f); // Teal/cyan

        // Crosshair
        render::line(FVector2D(center_x - 4, center_y), FVector2D(center_x + 4, center_y), col, 1.5f);
        render::line(FVector2D(center_x, center_y - 4), FVector2D(center_x, center_y + 4), col, 1.5f);

        // Forward arrow (up)
        float tip_y = center_y - 8.0f;
        render::line(FVector2D(center_x, tip_y), FVector2D(center_x - 3.0f, center_y - 3.0f), col, 1.5f);
        render::line(FVector2D(center_x, tip_y), FVector2D(center_x + 3.0f, center_y - 3.0f), col, 1.5f);
    }

    inline void initialize(FVector2D pos, FVector2D sz, FVector cam_loc, FRotator cam_rot) {
        position = pos;
        size = sz;
        camera_location = cam_loc;
        camera_rotation = cam_rot;
        yaw_rad = (float)(cam_rot.yaw * 3.14159265 / 180.0);

        // Circular radar: center and radius from the square bounding box
        radius = (float)(sz.x / 2.0);
        center_x = (float)pos.x + radius;
        center_y = (float)pos.y + radius;

        // Filled circle background
        draw_filled_circle(center_x, center_y, radius, FLinearColor(0.02f, 0.02f, 0.04f, 0.85f));

        // Outer ring border (2 rings for thickness)
        FLinearColor border_outer(0.45f, 0.45f, 0.55f, 0.9f);
        FLinearColor border_inner(0.30f, 0.30f, 0.40f, 0.7f);
        render::circle(FVector2D(center_x, center_y), (int)radius, 48, border_outer);
        render::circle(FVector2D(center_x, center_y), (int)(radius - 1), 48, border_inner);

        // Grid: concentric range rings
        if (config::radar::show_grid) {
            draw_grid_circles();

            // Crosshair lines (clipped to circle visually by being inside the filled circle)
            FLinearColor cross_color(0.25f, 0.25f, 0.30f, 0.35f);
            render::line(FVector2D(center_x, center_y - radius + 3), FVector2D(center_x, center_y + radius - 3), cross_color, 1.0f);
            render::line(FVector2D(center_x - radius + 3, center_y), FVector2D(center_x + radius - 3, center_y), cross_color, 1.0f);
        }

        // Center indicator
        draw_center_indicator();

        // Compass
        if (config::radar::show_compass) {
            draw_compass();
        }
    }

} // namespace radar
} // namespace features
