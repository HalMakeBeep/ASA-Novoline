/*
 * ARK Survival Ascended - ESP & Aimbot
 * Build as Release x64
 *
 * New structure entry point — spawns init on a new thread.
 */

#include "includes.h"
#include "main.h"

DWORD WINAPI InitThread(LPVOID lpParam) {
    ark::initialize();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        CreateThread(nullptr, 0, InitThread, module, 0, nullptr);
    }
    return TRUE;
}
