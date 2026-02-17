// ArkCheat.h - ARK: Survival Ascended Cheat
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <atomic>

// ============== OFFSETS ==============
#define OFFSET_HP       0x850   // Health

// ============== SIGNATURES ==============
namespace Signatures {
    // HP write: movss [rax+00000850],xmm2
    // F3 0F 11 90 50 08 00 00
    extern const char* HP_Write;
    extern const char* HP_Write_Mask;
    extern const int HP_InstrLen;  // 8 bytes
}

// ============== GLOBALS ==============
extern std::atomic<bool> g_Running;
extern std::atomic<uintptr_t> g_PlayerBase;  // RAX from HP hook
extern std::atomic<float> g_CurrentHP;
extern std::atomic<float> g_MaxHP;

extern uintptr_t g_HPCodeAddr;
extern void* g_HPTrampoline;
extern BYTE g_HPOriginalBytes[16];

// Feature toggles
extern bool g_GodMode;
extern bool g_InfiniteStamina;
extern bool g_InfiniteWeight;

// GUI
extern HWND g_GUIWnd;
extern bool g_GUIVisible;

// ============== FUNCTIONS ==============
// Memory
bool IsValidPtr(uintptr_t addr);
uintptr_t FindPattern(const char* module, const char* pattern, const char* mask);

template<typename T> T Read(uintptr_t addr);
template<typename T> void Write(uintptr_t addr, T value);

// Hooks
void* CreateHPHookStub(uintptr_t returnAddr);
bool InstallHook(uintptr_t addr, void* stub, BYTE* originalBytes, int len);
void RemoveHook(uintptr_t addr, BYTE* originalBytes, int len);

// Player functions
float GetHP();
void SetHP(float value);
