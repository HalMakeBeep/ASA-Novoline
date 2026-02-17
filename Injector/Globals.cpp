// Globals.cpp - Global variables and memory functions
#include "ArkCheat.h"

// ============== SIGNATURES ==============
namespace Signatures {
    // HP write: movss [rax+00000850],xmm2
    // F3 0F 11 90 50 08 00 00
    const char* HP_Write = "\xF3\x0F\x11\x90\x50\x08\x00\x00";
    const char* HP_Write_Mask = "xxxxxxxx";
    const int HP_InstrLen = 8;
}

// ============== GLOBALS ==============
std::atomic<bool> g_Running{true};
std::atomic<uintptr_t> g_PlayerBase{0};
std::atomic<float> g_CurrentHP{0.0f};
std::atomic<float> g_MaxHP{100.0f};

uintptr_t g_HPCodeAddr = 0;
void* g_HPTrampoline = nullptr;
BYTE g_HPOriginalBytes[16] = {0};

bool g_GodMode = false;
bool g_InfiniteStamina = false;
bool g_InfiniteWeight = false;

HWND g_GUIWnd = nullptr;
bool g_GUIVisible = false;

// ============== MEMORY FUNCTIONS ==============
bool IsValidPtr(uintptr_t addr) {
    if (addr < 0x10000 || addr > 0x7FFFFFFFFFFF) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery((void*)addr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    return true;
}

template<typename T>
T Read(uintptr_t addr) {
    if (!IsValidPtr(addr)) return T{};
    return *(T*)addr;
}

template<typename T>
void Write(uintptr_t addr, T value) {
    if (!IsValidPtr(addr)) return;
    DWORD oldProtect;
    VirtualProtect((void*)addr, sizeof(T), PAGE_EXECUTE_READWRITE, &oldProtect);
    *(T*)addr = value;
    VirtualProtect((void*)addr, sizeof(T), oldProtect, &oldProtect);
}

// Explicit instantiations
template float Read<float>(uintptr_t addr);
template void Write<float>(uintptr_t addr, float value);
template uintptr_t Read<uintptr_t>(uintptr_t addr);
template BYTE Read<BYTE>(uintptr_t addr);
template void Write<BYTE>(uintptr_t addr, BYTE value);
template int Read<int>(uintptr_t addr);
template void Write<int>(uintptr_t addr, int value);

// Pattern scanning
uintptr_t FindPattern(const char* module, const char* pattern, const char* mask) {
    HMODULE hModule = GetModuleHandleA(module);
    if (!hModule) return 0;

    MODULEINFO modInfo;
    if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(modInfo))) return 0;

    uintptr_t base = (uintptr_t)modInfo.lpBaseOfDll;
    size_t size = modInfo.SizeOfImage;
    size_t patternLen = strlen(mask);

    for (size_t i = 0; i < size - patternLen; i++) {
        bool found = true;
        for (size_t j = 0; j < patternLen; j++) {
            if (mask[j] == 'x' && pattern[j] != *(char*)(base + i + j)) {
                found = false;
                break;
            }
        }
        if (found) return base + i;
    }
    return 0;
}

// Player functions
float GetHP() {
    uintptr_t player = g_PlayerBase.load();
    if (!player || !IsValidPtr(player)) return 0.0f;
    return Read<float>(player + OFFSET_HP);
}

void SetHP(float value) {
    uintptr_t player = g_PlayerBase.load();
    if (!player || !IsValidPtr(player)) return;
    Write<float>(player + OFFSET_HP, value);
}
