#pragma once

#include <cstdint>
#include "../core/memory.h"

extern std::uintptr_t game;

namespace offsets {

    // ── Global offsets (resolved via sig scan, fallback to hardcoded) ──
    inline std::uintptr_t GObjects     = 0x0D9C2A40;
    inline std::uintptr_t GNames       = 0x0DD3DC40;
    inline std::uintptr_t GWorld       = 0x0DF93210;
    inline std::uintptr_t ProcessEvent = 0x017D4710;
    inline std::uintptr_t BoneMatrix   = 0x1608A20;

    // ── VTable indices ──
    constexpr int ProcessEventVIdx   = 80;
    constexpr int DrawTransitionVIdx = 121;

    // ── Struct member offsets (hardcoded — rarely change between updates) ──
    constexpr std::uintptr_t UObject_Name                  = 0x18;
    constexpr std::uintptr_t Actor_RootComponent           = 0x390;
    constexpr std::uintptr_t Character_Mesh                = 0x538;
    constexpr std::uintptr_t Character_TargetTeam          = 0x1A0;
    constexpr std::uintptr_t Character_TribeName           = 0xFC8;
    constexpr std::uintptr_t ShooterCharacter_PlayerName   = 0x1CD0;
    constexpr std::uintptr_t CameraComponent_FieldOfView   = 0x258;
    constexpr std::uintptr_t Controller_InputYawScale      = 0x738;
    constexpr std::uintptr_t Controller_InputPitchScale    = 0x73C;
    constexpr std::uintptr_t Canvas_ClipX                  = 0x30;
    constexpr std::uintptr_t Canvas_ClipY                  = 0x34;
    constexpr std::uintptr_t Engine_Font                   = 0x80;
    constexpr std::uintptr_t Viewport_World                = 0x78;
    constexpr std::uintptr_t LocalPlayer_Viewport          = 0x80;
    constexpr std::uintptr_t GameInstance_LocalPlayers      = 0x38;

    // ── Byte signatures (from Payson dump, ARK v82.15) ──
    namespace sigs {
        constexpr const char* ProcessEvent =
            "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? "
            "48 8D 6C 24 ? 48 89 9D ? ? ? ? 48 8B 05 ? ? ? ? "
            "48 33 C5 48 89 85 ? ? ? ? 4D 8B E0";

        constexpr const char* BoneMatrix =
            "48 83 EC ? 65 48 8B 04 25 ? ? ? ? B9 ? ? ? ? "
            "? ? ? ? ? ? 39 05 ? ? ? ? 7F ? "
            "48 8D 05 ? ? ? ? 48 83 C4 ? C3 48 83 C0";

        // GObjects setup function — has LEA RCX,[rip+disp32] at offset +4
        constexpr const char* GObjects =
            "48 83 EC ? 48 8D 0D ? ? ? ? FF 15 ? ? ? ? "
            "BA ? ? ? ? 48 8D 0D ? ? ? ? FF 15 ? ? ? ? "
            "33 C0 48 8D 0D ? ? ? ? "
            "48 89 05 ? ? ? ? 48 89 05 ? ? ? ? "
            "48 89 05 ? ? ? ? 48 89 05 ? ? ? ? "
            "48 89 05 ? ? ? ? 48 89 05";

        // GNames — reference is deep inside function, used for validation only
        constexpr const char* GNames =
            "48 89 5C 24 ? 48 89 4C 24 ? 55 56 57 41 54 41 55 41 56 41 57 "
            "48 8D 6C 24 ? 48 81 EC ? ? ? ? 44 8B 05";

        // GWorld — reference is indirect, used for validation only
        constexpr const char* GWorld =
            "40 53 48 83 EC ? 48 8B 81 ? ? ? ? 48 8B D9 48 85 C0 74 ? 48 8B 98";
    }

    // ── Resolve offsets via signature scanning ──
    inline void resolve_offsets(std::uintptr_t module_base) {
        if (!module_base) return;

        dbg::info("--- Signature Scanning ---");

        int found = 0, total = 0;

        // Helper: scan for a direct function address
        auto scan_func = [&](const char* name, std::uintptr_t& offset, const char* sig) {
            total++;
            auto addr = memory::pattern_scan(module_base, sig);
            if (addr) {
                auto new_offset = addr - module_base;
                dbg::success("  %-15s FOUND at 0x%llX (offset=0x%llX)",
                    name, (unsigned long long)addr, (unsigned long long)new_offset);
                offset = new_offset;
                found++;
            } else {
                dbg::warn("  %-15s NOT FOUND — using hardcoded 0x%llX",
                    name, (unsigned long long)offset);
            }
        };

        // Helper: scan for a function containing LEA reg,[rip+disp32] and extract the global address
        // lea_offset = byte offset from pattern start to the LEA opcode (48 8D 0D / 4C 8D 05 / etc.)
        // The LEA instruction is 7 bytes: 3 opcode + 4 displacement
        auto scan_lea = [&](const char* name, std::uintptr_t& offset, const char* sig, int lea_offset, std::int32_t adjust = 0) {
            total++;
            auto addr = memory::pattern_scan(module_base, sig);
            if (addr) {
                __try {
                    auto lea_addr = addr + lea_offset;
                    auto disp32 = *reinterpret_cast<std::int32_t*>(lea_addr + 3); // skip 3-byte opcode
                    auto global_addr = (lea_addr + 7) + disp32 + adjust; // next instruction + displacement + adjustment
                    auto new_offset = global_addr - module_base;
                    dbg::success("  %-15s FOUND — global at 0x%llX (offset=0x%llX)",
                        name, (unsigned long long)global_addr, (unsigned long long)new_offset);
                    offset = new_offset;
                    found++;
                } __except (EXCEPTION_EXECUTE_HANDLER) {
                    dbg::warn("  %-15s pattern found but LEA extraction failed — using hardcoded 0x%llX",
                        name, (unsigned long long)offset);
                }
            } else {
                dbg::warn("  %-15s NOT FOUND — using hardcoded 0x%llX",
                    name, (unsigned long long)offset);
            }
        };

        // Helper: scan to validate that a hardcoded offset is still correct
        auto scan_validate = [&](const char* name, std::uintptr_t offset, const char* sig) {
            total++;
            auto addr = memory::pattern_scan(module_base, sig);
            if (addr) {
                dbg::success("  %-15s sig validated (function at 0x%llX) — hardcoded offset 0x%llX",
                    name, (unsigned long long)addr, (unsigned long long)offset);
                found++;
            } else {
                dbg::warn("  %-15s sig NOT FOUND — hardcoded 0x%llX may be outdated!",
                    name, (unsigned long long)offset);
            }
        };

        // Scan functions (pattern = function start)
        scan_func("ProcessEvent", ProcessEvent, sigs::ProcessEvent);
        scan_func("BoneMatrix",   BoneMatrix,   sigs::BoneMatrix);

        // Scan GObjects — LEA at byte offset 4 in pattern: 48 83 EC ? [48 8D 0D ...]
        // The LEA references a field 0x20 bytes past the TUObjectArray base
        // (FUObjectArray header: ObjFirstGCIndex, ObjLastNonGCIndex, etc.)
        scan_lea("GObjects", GObjects, sigs::GObjects, 4, -0x20);

        // GNames and GWorld: validate only (global ref is too deep to extract)
        scan_validate("GNames", GNames, sigs::GNames);
        scan_validate("GWorld", GWorld, sigs::GWorld);

        dbg::info("Sig scan complete: %d/%d resolved", found, total);
    }

    // ── Validate all offsets are readable and in-range ──
    inline void validate_offsets(std::uintptr_t module_base) {
        if (!module_base) {
            dbg::error("validate_offsets: module_base is 0!");
            return;
        }

        std::uintptr_t module_size = 0;
        __try {
            auto dos = (IMAGE_DOS_HEADER*)module_base;
            auto nt  = (IMAGE_NT_HEADERS*)(module_base + dos->e_lfanew);
            module_size = nt->OptionalHeader.SizeOfImage;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::warn("Could not read PE header, skipping range checks");
        }

        dbg::info("--- Offset Validation ---");
        dbg::info("Module base: 0x%llX", (unsigned long long)module_base);
        if (module_size > 0) {
            dbg::info("Module size: 0x%llX (%llu MB)",
                (unsigned long long)module_size,
                (unsigned long long)(module_size / (1024 * 1024)));
            dbg::info("Module end:  0x%llX", (unsigned long long)(module_base + module_size));
        }

        auto check_func = [&](const char* name, std::uintptr_t offset) {
            auto addr = module_base + offset;
            bool in_range = (module_size > 0) ? (offset < module_size) : true;
            bool readable = IsBadReadPtr((void*)addr, 1) == 0;

            if (!in_range) {
                dbg::error("  %-25s offset=0x%-10llX addr=0x%-16llX OUT OF RANGE",
                    name, (unsigned long long)offset, (unsigned long long)addr);
            } else if (!readable) {
                dbg::warn("  %-25s offset=0x%-10llX addr=0x%-16llX NOT READABLE",
                    name, (unsigned long long)offset, (unsigned long long)addr);
            } else {
                dbg::success("  %-25s offset=0x%-10llX addr=0x%-16llX OK",
                    name, (unsigned long long)offset, (unsigned long long)addr);
            }
        };

        auto check_struct = [](const char* name, std::uintptr_t offset) {
            dbg::log("  %-25s offset=0x%llX", name, (unsigned long long)offset);
        };

        dbg::info("Function offsets:");
        check_func("GObjects",         GObjects);
        check_func("GNames",           GNames);
        check_func("GWorld",           GWorld);
        check_func("ProcessEvent",     ProcessEvent);
        check_func("BoneMatrix",       BoneMatrix);

        dbg::info("VTable indices:");
        dbg::log("  ProcessEventVIdx   = %d", ProcessEventVIdx);
        dbg::log("  DrawTransitionVIdx = %d", DrawTransitionVIdx);

        dbg::info("Struct offsets:");
        check_struct("UObject_Name",      UObject_Name);
        check_struct("Actor_RootComponent", Actor_RootComponent);
        check_struct("Character_Mesh",    Character_Mesh);
        check_struct("Character_TargetTeam", Character_TargetTeam);
        check_struct("Character_TribeName", Character_TribeName);
        check_struct("ShooterCharacter_PlayerName", ShooterCharacter_PlayerName);
        check_struct("CameraComponent_FieldOfView", CameraComponent_FieldOfView);
        check_struct("Controller_InputYawScale", Controller_InputYawScale);
        check_struct("Controller_InputPitchScale", Controller_InputPitchScale);
        check_struct("Canvas_ClipX",     Canvas_ClipX);
        check_struct("Canvas_ClipY",     Canvas_ClipY);
        check_struct("Engine_Font",      Engine_Font);
        check_struct("Viewport_World",   Viewport_World);
        check_struct("LocalPlayer_Viewport", LocalPlayer_Viewport);
        check_struct("GameInstance_LocalPlayers", GameInstance_LocalPlayers);

        dbg::info("");
    }

}
