#pragma once

#include <cstdint>

extern std::uintptr_t game;

namespace offsets {

    constexpr std::uintptr_t GObjects = 0xDE7FA20;
    constexpr std::uintptr_t GNames = 0xE1F1340;
    constexpr std::uintptr_t GWorld = 0xE1B8F98;


    constexpr std::uintptr_t ProcessEvent = 0x1919EF0;
    constexpr std::uintptr_t StaticFindObject = 0x1939440;
    constexpr std::uintptr_t BoneMatrix = 0x1608A20;


    constexpr int ProcessEventVIdx = 80;
    constexpr int DrawTransitionVIdx = 121;


    constexpr std::uintptr_t UObject_Name = 0x18;


    constexpr std::uintptr_t Actor_RootComponent = 0x390;


    constexpr std::uintptr_t Character_Mesh = 0x538;
    constexpr std::uintptr_t Character_TargetTeam = 0x1A0;
    constexpr std::uintptr_t Character_TribeName = 0xFC8;
    constexpr std::uintptr_t ShooterCharacter_PlayerName = 0x1CC0;
    constexpr std::uintptr_t CameraComponent_FieldOfView = 0x334;


    constexpr std::uintptr_t Controller_InputYawScale = 0x738;
    constexpr std::uintptr_t Controller_InputPitchScale = 0x73C;


    constexpr std::uintptr_t Canvas_ClipX = 0x30;
    constexpr std::uintptr_t Canvas_ClipY = 0x34;


    constexpr std::uintptr_t Engine_Font = 0x80;


    constexpr std::uintptr_t Viewport_World = 0x78;


    constexpr std::uintptr_t LocalPlayer_Viewport = 0x80;


    constexpr std::uintptr_t GameInstance_LocalPlayers = 0x38;


    inline void validate_offsets(std::uintptr_t module_base) {
        if (!module_base) {
            dbg::error("validate_offsets: module_base is 0!");
            return;
        }

        // Get module size via PE header
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
                dbg::error("  %-25s offset=0x%-10llX addr=0x%-16llX OUT OF RANGE (module ends at 0x%llX)",
                    name, (unsigned long long)offset, (unsigned long long)addr,
                    (unsigned long long)(module_base + module_size));
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
        check_func("StaticFindObject", StaticFindObject);
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
