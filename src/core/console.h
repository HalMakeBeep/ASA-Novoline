#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <cstdio>
#include <cstdarg>
#include <cstdint>

namespace dbg {

    inline HANDLE  hConsole = nullptr;
    inline FILE*   fpOut = nullptr;
    inline bool    active = false;
    inline CRITICAL_SECTION cs = {};
    inline bool    cs_initialized = false;

    enum Color : WORD {
        COLOR_WHITE   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        COLOR_RED     = FOREGROUND_RED | FOREGROUND_INTENSITY,
        COLOR_YELLOW  = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        COLOR_GREEN   = FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        COLOR_CYAN    = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        COLOR_GRAY    = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
        COLOR_MAGENTA = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    };

    enum class Level : int {
        Error = 0,
        Warn = 1,
        Info = 2,
        Debug = 3,
    };

    enum Category : std::uint32_t {
        Init      = 1u << 0,
        Render    = 1u << 1,
        Aimbot    = 1u << 2,
        Cache     = 1u << 3,
        Config    = 1u << 4,
        Stability = 1u << 5,
        SDK       = 1u << 6,
        Hook      = 1u << 7,
        Default   = Init,
    };

    inline Level min_level = Level::Info;
    inline std::uint32_t info_debug_categories = Init | Config | Stability;

    inline const char* level_str(Level level) {
        switch (level) {
        case Level::Error: return "ERROR";
        case Level::Warn: return "WARN";
        case Level::Info: return "INFO";
        default: return "DEBUG";
        }
    }

    inline const char* category_str(Category category) {
        switch (category) {
        case Init: return "INIT";
        case Render: return "RENDER";
        case Aimbot: return "AIMBOT";
        case Cache: return "CACHE";
        case Config: return "CONFIG";
        case Stability: return "STABILITY";
        case SDK: return "SDK";
        case Hook: return "HOOK";
        default: return "GEN";
        }
    }

    inline Color level_color(Level level) {
        switch (level) {
        case Level::Error: return COLOR_RED;
        case Level::Warn: return COLOR_YELLOW;
        case Level::Info: return COLOR_CYAN;
        default: return COLOR_GRAY;
        }
    }

    inline void set_min_level(Level level) {
        min_level = level;
    }

    inline void enable_category(Category category, bool enabled) {
        if (enabled) info_debug_categories |= static_cast<std::uint32_t>(category);
        else info_debug_categories &= ~static_cast<std::uint32_t>(category);
    }

    inline bool should_log(Level level, Category category) {
        if (level == Level::Error || level == Level::Warn) return true;
        if (static_cast<int>(level) > static_cast<int>(min_level)) return false;
        return (info_debug_categories & static_cast<std::uint32_t>(category)) != 0;
    }

    inline bool init() {
        if (active) return true;

        if (!cs_initialized) {
            InitializeCriticalSection(&cs);
            cs_initialized = true;
        }

        if (!AllocConsole()) return false;
        freopen_s(&fpOut, "CONOUT$", "w", stdout);

        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!hConsole || hConsole == INVALID_HANDLE_VALUE) return false;

        SetConsoleTitleA("ASA Debug Console");

        COORD bufferSize = { 140, 10000 };
        SetConsoleScreenBufferSize(hConsole, bufferSize);
        SMALL_RECT windowSize = { 0, 0, 139, 39 };
        SetConsoleWindowInfo(hConsole, TRUE, &windowSize);

        active = true;
        return true;
    }

    inline void shutdown() {
        if (!active) return;
        active = false;
        if (fpOut) fclose(fpOut);
        FreeConsole();
        fpOut = nullptr;
        hConsole = nullptr;
    }

    inline void get_timestamp(char* buf, size_t buf_size) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        sprintf_s(buf, buf_size, "[%02d:%02d:%02d.%03d]",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }

    inline void print_ex(Level level, Category category, const char* fmt, va_list args) {
        if (!active || !hConsole) return;
        if (!should_log(level, category)) return;

        EnterCriticalSection(&cs);

        char timestamp[32];
        get_timestamp(timestamp, sizeof(timestamp));

        SetConsoleTextAttribute(hConsole, COLOR_GRAY);
        printf("%s ", timestamp);

        SetConsoleTextAttribute(hConsole, level_color(level));
        printf("[%s] ", level_str(level));

        SetConsoleTextAttribute(hConsole, COLOR_MAGENTA);
        printf("[%s] ", category_str(category));

        SetConsoleTextAttribute(hConsole, COLOR_WHITE);
        vprintf(fmt, args);
        printf("\n");
        fflush(stdout);

        LeaveCriticalSection(&cs);
    }

    inline void log_ex(Level level, Category category, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        print_ex(level, category, fmt, args);
        va_end(args);
    }

    inline void log(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        print_ex(Level::Debug, Category::Init, fmt, args);
        va_end(args);
    }

    inline void warn(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        print_ex(Level::Warn, Category::Init, fmt, args);
        va_end(args);
    }

    inline void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        print_ex(Level::Error, Category::Init, fmt, args);
        va_end(args);
    }

    inline void success(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        print_ex(Level::Debug, Category::Init, fmt, args);
        va_end(args);
    }

    inline void info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        print_ex(Level::Debug, Category::Init, fmt, args);
        va_end(args);
    }

    inline void dump_ptr(const char* label, void* ptr) {
        if (!active) return;
        if (ptr) {
            log_ex(Level::Debug, Category::SDK, "%s = 0x%p", label, ptr);
        }
        else {
            log_ex(Level::Warn, Category::SDK, "%s = NULL", label);
        }
    }

    inline void dump_offset(const char* label, std::uintptr_t base, std::uintptr_t offset) {
        if (!active) return;
        log_ex(Level::Debug, Category::SDK, "%s: base=0x%llX + offset=0x%llX = 0x%llX",
            label,
            (unsigned long long)base,
            (unsigned long long)offset,
            (unsigned long long)(base + offset));
    }

} // namespace dbg
