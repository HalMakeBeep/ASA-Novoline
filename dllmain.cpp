

#include "src/includes.h"
#include "src/main.h"

// Verify that the launcher created our auth token in shared memory.
// Without this, the DLL refuses to initialize — prevents use with third-party injectors.
static bool VerifyLauncherAuth() {
    char authName[64];
    snprintf(authName, sizeof(authName), "Local\\PrismeAuth_%lu", GetCurrentProcessId());

    HANDLE hMapping = OpenFileMappingA(FILE_MAP_READ, FALSE, authName);
    if (!hMapping)
        return false;

    LPVOID pToken = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 32);
    bool valid = false;
    if (pToken) {
        // Check that the token is not all zeros (i.e. launcher actually wrote something)
        const BYTE* token = (const BYTE*)pToken;
        for (int i = 0; i < 32; i++) {
            if (token[i] != 0) { valid = true; break; }
        }
        UnmapViewOfFile(pToken);
    }

    CloseHandle(hMapping);
    return valid;
}

DWORD WINAPI InitThread(LPVOID lpParam) {
    if (!VerifyLauncherAuth()) {
        FreeLibraryAndExitThread((HMODULE)lpParam, 0);
        return 0;
    }

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
