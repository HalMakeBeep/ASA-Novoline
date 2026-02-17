#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

namespace Injector {

inline DWORD GetProcessId(const wchar_t* processName) {
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry;
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (_wcsicmp(entry.szExeFile, processName) == 0) {
                    pid = entry.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return pid;
}

inline bool InjectDLL(DWORD pid, const char* dllPath, char* errorOut = nullptr, size_t errorSize = 0) {
    auto setError = [&](const char* step, DWORD err) {
        if (errorOut && errorSize > 0)
            snprintf(errorOut, errorSize, "%s failed (error %lu)", step, err);
    };

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) { setError("OpenProcess", GetLastError()); return false; }

    // Allocate memory for DLL path
    size_t pathLen = strlen(dllPath) + 1;
    LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, pathLen,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteMem) {
        setError("VirtualAllocEx", GetLastError());
        CloseHandle(hProcess);
        return false;
    }

    // Write DLL path
    if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, NULL)) {
        setError("WriteProcessMemory", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Get LoadLibraryA address
    LPVOID loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (!loadLibAddr) {
        setError("GetProcAddress(LoadLibraryA)", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Create remote thread
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
                                         (LPTHREAD_START_ROUTINE)loadLibAddr,
                                         remoteMem, 0, NULL);
    if (!hThread) {
        setError("CreateRemoteThread", GetLastError());
        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Wait for injection
    WaitForSingleObject(hThread, 10000);

    // Check if LoadLibrary succeeded (return value = module handle, 0 = failure)
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    // Cleanup
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (exitCode == 0) {
        setError("LoadLibraryA in remote process", 0);
        return false;
    }

    return true;
}

inline bool InjectToASA() {
    // Get DLL path (same folder as exe)
    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);

    // Replace exe name with dll name
    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash) {
        strcpy(lastSlash + 1, "ASA-Internal.dll");
    } else {
        strcpy(dllPath, "ASA-Internal.dll");
    }

    // Check if DLL exists
    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL, "ASA-Internal.dll not found!", "Error", MB_ICONERROR);
        return false;
    }

    // Find ASA game process
    DWORD pid = GetProcessId(L"ArkAscended.exe");
    if (pid == 0) {
        MessageBoxA(NULL, "ArkAscended.exe not running!\nStart the game first.", "Error", MB_ICONERROR);
        return false;
    }

    // Inject
    char errorDetail[256] = {};
    if (!InjectDLL(pid, dllPath, errorDetail, sizeof(errorDetail))) {
        char msg[512];
        snprintf(msg, sizeof(msg), "Failed to inject DLL!\n%s\nTry running as Administrator.", errorDetail);
        MessageBoxA(NULL, msg, "Error", MB_ICONERROR);
        return false;
    }

    return true;
}

} // namespace Injector
