#pragma once

#include <Windows.h>
#include <cstdint>
#include <cstring>
#include "console.h"

namespace diagnostics {

    enum class FeaturePath : int {
        Radar = 0,
        PlayersEsp = 1,
        DinosEsp = 2,
        Aimbot = 3,
        Count = 4
    };

    struct FeatureState {
        bool disabled = false;
        ULONGLONG disabled_until_ms = 0;
        int consecutive_failures = 0;
        int disable_events = 0;
    };

    struct Snapshot {
        double frame_ms = 0.0;
        double fps = 0.0;
        double players_scan_ms = 0.0;
        double dinos_scan_ms = 0.0;
        double aim_ms = 0.0;
        int players_cached = 0;
        int dinos_cached = 0;
        ULONGLONG players_cache_age_ms = 0;
        ULONGLONG dinos_cache_age_ms = 0;
        bool players_cache_refreshed = false;
        bool dinos_cache_refreshed = false;
        int players_processed = 0;
        int players_skipped = 0;
        int dinos_processed = 0;
        int dinos_skipped = 0;
        bool has_target = false;
        bool target_locked = false;
        bool target_is_player = false;
        int target_mode = 0;
        double target_crosshair_dist = 0.0;
        double target_world_dist = 0.0;
        double aim_delta_x = 0.0;
        double aim_delta_y = 0.0;
        int guard_world_failures = 0;
        int guard_controller_failures = 0;
        int guard_camera_failures = 0;
        int guard_projection_failures = 0;
        int guard_exception_failures = 0;
        int active_profile_slot = 1;
        char config_status[96] = "Idle";
        FeatureState feature_states[(int)FeaturePath::Count];
    };

    inline Snapshot snap = {};
    inline LARGE_INTEGER perf_freq = {};
    inline LARGE_INTEGER frame_start = {};
    inline bool perf_ready = false;

    inline void ensure_perf_ready() {
        if (!perf_ready) {
            QueryPerformanceFrequency(&perf_freq);
            perf_ready = true;
        }
    }

    inline double elapsed_ms(LARGE_INTEGER begin, LARGE_INTEGER end) {
        if (!perf_ready || perf_freq.QuadPart == 0) return 0.0;
        return (double)(end.QuadPart - begin.QuadPart) * 1000.0 / (double)perf_freq.QuadPart;
    }

    struct ScopedTimer {
        LARGE_INTEGER start = {};
        double* out_ms = nullptr;
        explicit ScopedTimer(double* out) : out_ms(out) {
            ensure_perf_ready();
            QueryPerformanceCounter(&start);
        }
        ~ScopedTimer() {
            LARGE_INTEGER end = {};
            QueryPerformanceCounter(&end);
            if (out_ms) *out_ms = elapsed_ms(start, end);
        }
    };

    inline void begin_frame() {
        ensure_perf_ready();
        QueryPerformanceCounter(&frame_start);
        snap.players_scan_ms = 0.0;
        snap.dinos_scan_ms = 0.0;
        snap.aim_ms = 0.0;
        snap.players_processed = 0;
        snap.players_skipped = 0;
        snap.dinos_processed = 0;
        snap.dinos_skipped = 0;
        snap.players_cache_refreshed = false;
        snap.dinos_cache_refreshed = false;
    }

    inline void end_frame() {
        LARGE_INTEGER end = {};
        QueryPerformanceCounter(&end);
        snap.frame_ms = elapsed_ms(frame_start, end);
        snap.fps = snap.frame_ms > 0.0 ? (1000.0 / snap.frame_ms) : 0.0;
    }

    inline const Snapshot& get_snapshot() { return snap; }

    inline void set_profile_slot(int slot) { snap.active_profile_slot = slot; }
    inline void set_config_status(const char* status) {
        if (!status) return;
        strncpy_s(snap.config_status, status, sizeof(snap.config_status) - 1);
    }

    inline void set_player_cache_stats(int count, ULONGLONG age_ms, bool refreshed) {
        snap.players_cached = count;
        snap.players_cache_age_ms = age_ms;
        snap.players_cache_refreshed = refreshed;
    }

    inline void set_dino_cache_stats(int count, ULONGLONG age_ms, bool refreshed) {
        snap.dinos_cached = count;
        snap.dinos_cache_age_ms = age_ms;
        snap.dinos_cache_refreshed = refreshed;
    }

    inline void add_player_processed() { snap.players_processed++; }
    inline void add_player_skipped() { snap.players_skipped++; }
    inline void add_dino_processed() { snap.dinos_processed++; }
    inline void add_dino_skipped() { snap.dinos_skipped++; }

    inline void set_target_state(bool has_target, bool locked, bool is_player, int mode, double cross, double world) {
        snap.has_target = has_target;
        snap.target_locked = locked;
        snap.target_is_player = is_player;
        snap.target_mode = mode;
        snap.target_crosshair_dist = cross;
        snap.target_world_dist = world;
    }

    inline void set_aim_delta(double dx, double dy) {
        snap.aim_delta_x = dx;
        snap.aim_delta_y = dy;
    }

    inline void inc_guard_world_failure() { snap.guard_world_failures++; }
    inline void inc_guard_controller_failure() { snap.guard_controller_failures++; }
    inline void inc_guard_camera_failure() { snap.guard_camera_failures++; }
    inline void inc_guard_projection_failure() { snap.guard_projection_failures++; }
    inline void inc_guard_exception_failure() { snap.guard_exception_failures++; }

    inline bool feature_enabled(FeaturePath path) {
        auto& st = snap.feature_states[(int)path];
        if (!st.disabled) return true;
        ULONGLONG now = GetTickCount64();
        if (now >= st.disabled_until_ms) {
            st.disabled = false;
            st.consecutive_failures = 0;
            dbg::log_ex(dbg::Level::Warn, dbg::Stability, "Recovered feature path %d", (int)path);
            return true;
        }
        return false;
    }

    inline void on_feature_success(FeaturePath path) {
        auto& st = snap.feature_states[(int)path];
        st.consecutive_failures = 0;
    }

    inline void on_feature_failure(FeaturePath path, int threshold = 3, ULONGLONG cooldown_ms = 800) {
        auto& st = snap.feature_states[(int)path];
        st.consecutive_failures++;
        if (st.consecutive_failures >= threshold) {
            st.disabled = true;
            st.disabled_until_ms = GetTickCount64() + cooldown_ms;
            st.disable_events++;
            st.consecutive_failures = 0;
            dbg::log_ex(dbg::Level::Warn, dbg::Stability, "Disabled feature path %d for %llums", (int)path, (unsigned long long)cooldown_ms);
        }
    }

} // namespace diagnostics
