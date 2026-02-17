#pragma once
#include <iostream>
#include <cstdint>
#include <cmath>
#include <cfloat>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "../importer.hpp"

// Debug console (must come before everything else)
#include "core/console.h"
#include "core/diagnostics.h"

// Initialization status tracking (moved here to avoid circular dep with menu.h)
namespace ark {
    namespace init_status {
        inline bool module_found = false;
        inline bool sdk_initialized = false;
        inline bool world_found = false;
        inline bool viewport_found = false;
        inline bool font_found = false;
        inline bool hook_installed = false;
        inline const char* last_error = nullptr;
        inline const char* current_step = "Not started";
    }
}

inline std::uintptr_t game = 0;
#include "core/memory.h"
#include "core/module.h"
#include "core/hooks.h"

#include "sdk/offsets.h"
#include "sdk/math.h"
#include "sdk/ue_types.h"
#include "sdk/sdk.h"

#include "config/settings.h"
#include "config/config_io.h"
#include "config/profile_manager.h"

#include "gui/render.h"
#include "gui/zerogui.h"
#include "gui/menu.h"
#include "gui/dx_hook.h"

#include "features/radar.h"
#include "features/aimbot.h"
#include "features/esp.h"
#include "features/misc.h"
