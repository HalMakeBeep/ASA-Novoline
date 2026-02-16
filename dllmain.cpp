

#include "src/includes.h"
#include "src/main.h"

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
