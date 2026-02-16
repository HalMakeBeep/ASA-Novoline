#pragma once

#include "config_io.h"
#include "../core/diagnostics.h"
#include <Windows.h>
#include <string>
#include <fstream>

namespace config {
namespace profiles {

    inline int active_slot = 1;

    inline bool is_valid_slot(int slot) {
        return slot >= 1 && slot <= 5;
    }

    inline std::string slot_path(int slot) {
        return "configs/slot" + std::to_string(slot) + ".ini";
    }

    inline std::string export_path(int slot) {
        return "exports/slot" + std::to_string(slot) + "_export.ini";
    }

    inline const char* active_slot_path() {
        return "configs/active_slot.txt";
    }

    inline bool file_exists(const char* path) {
        DWORD attr = GetFileAttributesA(path);
        return (attr != INVALID_FILE_ATTRIBUTES) && !(attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline void ensure_dirs() {
        CreateDirectoryA("configs", nullptr);
        CreateDirectoryA("exports", nullptr);
    }

    inline bool persist_active_slot() {
        std::ofstream out(active_slot_path(), std::ios::trunc);
        if (!out.is_open()) return false;
        out << active_slot;
        return true;
    }

    inline void set_active_slot(int slot) {
        if (!is_valid_slot(slot)) return;
        active_slot = slot;
        diagnostics::set_profile_slot(active_slot);
        persist_active_slot();
    }

    inline int get_active_slot() {
        return active_slot;
    }

    inline bool load_active_slot_from_disk() {
        std::ifstream in(active_slot_path());
        if (!in.is_open()) return false;
        int slot = 1;
        in >> slot;
        if (!is_valid_slot(slot)) return false;
        active_slot = slot;
        diagnostics::set_profile_slot(active_slot);
        return true;
    }

    inline bool save_slot(int slot) {
        if (!is_valid_slot(slot)) return false;
        ensure_dirs();
        if (!io::save(slot_path(slot).c_str())) return false;
        set_active_slot(slot);
        diagnostics::set_config_status("Saved slot");
        return true;
    }

    inline bool load_slot(int slot) {
        if (!is_valid_slot(slot)) return false;
        ensure_dirs();
        if (!io::load(slot_path(slot).c_str())) return false;
        set_active_slot(slot);
        diagnostics::set_config_status("Loaded slot");
        return true;
    }

    inline bool export_slot(int slot) {
        if (!is_valid_slot(slot)) return false;
        ensure_dirs();
        if (!file_exists(slot_path(slot).c_str())) return false;
        BOOL ok = CopyFileA(slot_path(slot).c_str(), export_path(slot).c_str(), FALSE);
        diagnostics::set_config_status(ok ? "Exported slot" : "Export failed");
        return ok == TRUE;
    }

    inline bool import_slot(int slot) {
        if (!is_valid_slot(slot)) return false;
        ensure_dirs();
        if (!file_exists(export_path(slot).c_str())) return false;
        BOOL ok = CopyFileA(export_path(slot).c_str(), slot_path(slot).c_str(), FALSE);
        if (!ok) {
            diagnostics::set_config_status("Import failed");
            return false;
        }
        diagnostics::set_config_status("Imported slot");
        return load_slot(slot);
    }

    inline void migrate_legacy_if_needed() {
        ensure_dirs();
        if (!file_exists(slot_path(1).c_str()) && file_exists("asa_config.ini")) {
            CopyFileA("asa_config.ini", slot_path(1).c_str(), FALSE);
        }
    }

    inline bool initialize() {
        ensure_dirs();
        migrate_legacy_if_needed();
        load_active_slot_from_disk();

        if (!file_exists(slot_path(active_slot).c_str())) {
            if (file_exists(slot_path(1).c_str())) {
                active_slot = 1;
            } else {
                if (!save_slot(active_slot)) {
                    diagnostics::set_config_status("Config init failed");
                    return false;
                }
            }
        }

        bool ok = load_slot(active_slot);
        diagnostics::set_profile_slot(active_slot);
        diagnostics::set_config_status(ok ? "Profile initialized" : "Load failed; defaults");
        return ok;
    }

} // namespace profiles
} // namespace config

