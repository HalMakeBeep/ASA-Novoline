#pragma once

#include <Windows.h>

#include "../sdk/sdk.h"

namespace render {
    // Global state
    inline bool show_menu = false;
    inline bool draw_cursor = true;
    inline UWorld* world = nullptr;
    inline APlayerController* controller = nullptr;
    inline UCanvas* canvas = nullptr;
    inline UObject* font = nullptr;
    inline FVector2D screen_center;
    inline FVector2D screen_size;

    inline bool is_vk_clicked(int vk) {
        static bool down_already[256] = {};
        if (vk < 0 || vk > 255) return false;
        const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool clicked = false;
        if (down) {
            if (!down_already[vk]) {
                clicked = true;
            }
            down_already[vk] = true;
        } else {
            down_already[vk] = false;
        }
        return clicked;
    }

    // Check if Insert key was clicked (toggle)
    inline bool is_insert_clicked() {
        static bool down_already = false;

        bool down = false;
        if (controller && memory::is_valid_ptr((std::uintptr_t)controller)) {
            down = controller->IsInputKeyDown(sdk::InsertKey);
        } else {
            down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        }

        bool clicked = false;
        if (down) {
            if (!down_already) {
                clicked = true;
            }
            down_already = true;
        } else {
            down_already = false;
        }
        return clicked;
    }

    // Check if point is in circle
    inline bool in_circle(int cx, int cy, int r, int x, int y) {
        int dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);
        return dist <= r * r;
    }

    // Draw text
    inline void text(FString content, FVector2D pos, FLinearColor color, bool centerX = false, bool centerY = false, bool outlined = true) {
        if (canvas && font) {
            canvas->K2_DrawText(font, content, pos, FVector2D(1.0, 1.0), color, 1.0f, 
                FLinearColor(), FVector2D(), centerX, centerY, outlined, FLinearColor(0, 0, 0, 1));
        }
    }

    inline void text(const wchar_t* content, FVector2D pos, FLinearColor color, bool centerX = false, bool centerY = false, bool outlined = true) {
        text(FString(content), pos, color, centerX, centerY, outlined);
    }

    // Get text size
    inline FVector2D text_size(FString content) {
        if (canvas && font) {
            return canvas->K2_TextSize(font, content, FVector2D(1.0, 1.0));
        }
        return FVector2D();
    }

    // Draw line
    inline void line(FVector2D a, FVector2D b, FLinearColor color, float thickness = 1.0f) {
        if (canvas) {
            canvas->K2_DrawLine(a, b, thickness, color);
        }
    }

    // Draw line with black outline
    inline void line_outlined(FVector2D a, FVector2D b, FLinearColor color, float thickness = 1.0f) {
        if (canvas) {
            canvas->K2_DrawLine(a, b, thickness + 1.0f, FLinearColor::Black());
            canvas->K2_DrawLine(a, b, thickness, color);
        }
    }

    // Draw circle
    inline void circle(FVector2D pos, int radius, int segments, FLinearColor color) {
        float step = 3.14159265f * 2.0f / segments;
        for (float a = 0; a < 3.14159265f * 2.0f; a += step) {
            float x1 = radius * cosf(a) + (float)pos.x;
            float y1 = radius * sinf(a) + (float)pos.y;
            float x2 = radius * cosf(a + step) + (float)pos.x;
            float y2 = radius * sinf(a + step) + (float)pos.y;
            line(FVector2D(x1, y1), FVector2D(x2, y2), color, 1.0f);
        }
    }

    // Draw filled box
    inline void filled_box(FVector2D pos, FVector2D size, FLinearColor color) {
        if (canvas) {
            for (int i = 0; i < (int)size.y; i++) {
                canvas->K2_DrawLine(FVector2D(pos.x, pos.y + i), FVector2D(pos.x + size.x, pos.y + i), 1.0f, color);
            }
        }
    }

    // Draw box outline
    inline void box(FVector2D topLeft, FVector2D bottomRight, FLinearColor color, float thickness = 1.0f) {
        line(topLeft, FVector2D(bottomRight.x, topLeft.y), color, thickness);
        line(FVector2D(bottomRight.x, topLeft.y), bottomRight, color, thickness);
        line(bottomRight, FVector2D(topLeft.x, bottomRight.y), color, thickness);
        line(FVector2D(topLeft.x, bottomRight.y), topLeft, color, thickness);
    }

    // Draw cornered box
    inline void cornered_box(FVector2D topLeft, FVector2D bottomRight, FLinearColor color, float thickness = 2.0f) {
        float width = (float)(bottomRight.x - topLeft.x);
        float height = (float)(bottomRight.y - topLeft.y);
        float cornerW = width / 4.0f;
        float cornerH = height / 4.0f;

        // Top left
        line(topLeft, FVector2D(topLeft.x + cornerW, topLeft.y), color, thickness);
        line(topLeft, FVector2D(topLeft.x, topLeft.y + cornerH), color, thickness);

        // Top right
        line(FVector2D(bottomRight.x, topLeft.y), FVector2D(bottomRight.x - cornerW, topLeft.y), color, thickness);
        line(FVector2D(bottomRight.x, topLeft.y), FVector2D(bottomRight.x, topLeft.y + cornerH), color, thickness);

        // Bottom left
        line(FVector2D(topLeft.x, bottomRight.y), FVector2D(topLeft.x + cornerW, bottomRight.y), color, thickness);
        line(FVector2D(topLeft.x, bottomRight.y), FVector2D(topLeft.x, bottomRight.y - cornerH), color, thickness);

        // Bottom right
        line(bottomRight, FVector2D(bottomRight.x - cornerW, bottomRight.y), color, thickness);
        line(bottomRight, FVector2D(bottomRight.x, bottomRight.y - cornerH), color, thickness);
    }

} // namespace render
