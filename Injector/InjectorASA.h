#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <fstream>

namespace Injector {

// ─── Process helper ───────────────────────────────────────────────
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

// ─── Get remote module base ───────────────────────────────────────
inline uintptr_t GetRemoteModuleBase(DWORD pid, const char* moduleName) {
    uintptr_t base = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    MODULEENTRY32W me;
    me.dwSize = sizeof(me);

    wchar_t wModName[256];
    MultiByteToWideChar(CP_ACP, 0, moduleName, -1, wModName, 256);

    if (Module32FirstW(snapshot, &me)) {
        do {
            if (_wcsicmp(me.szModule, wModName) == 0) {
                base = (uintptr_t)me.modBaseAddr;
                break;
            }
        } while (Module32NextW(snapshot, &me));
    }
    CloseHandle(snapshot);
    return base;
}

// ─── Resolve import address in target process ─────────────────────
inline uintptr_t ResolveImportAddress(HANDLE hProcess, DWORD pid, const char* dllName, const char* funcName) {
    HMODULE hLocalMod = GetModuleHandleA(dllName);
    if (hLocalMod) {
        FARPROC localAddr = GetProcAddress(hLocalMod, funcName);
        if (localAddr)
            return (uintptr_t)localAddr;
    }

    uintptr_t remoteBase = GetRemoteModuleBase(pid, dllName);
    if (!remoteBase)
        return 0;

    IMAGE_DOS_HEADER dosHeader;
    if (!ReadProcessMemory(hProcess, (LPCVOID)remoteBase, &dosHeader, sizeof(dosHeader), nullptr))
        return 0;

    IMAGE_NT_HEADERS64 ntHeaders;
    if (!ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + dosHeader.e_lfanew), &ntHeaders, sizeof(ntHeaders), nullptr))
        return 0;

    auto& exportDir = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!exportDir.VirtualAddress || !exportDir.Size)
        return 0;

    IMAGE_EXPORT_DIRECTORY exports;
    uintptr_t exportBase = remoteBase + exportDir.VirtualAddress;
    if (!ReadProcessMemory(hProcess, (LPCVOID)exportBase, &exports, sizeof(exports), nullptr))
        return 0;

    std::vector<DWORD> funcRVAs(exports.NumberOfFunctions);
    std::vector<DWORD> nameRVAs(exports.NumberOfNames);
    std::vector<WORD>  ordinals(exports.NumberOfNames);

    ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + exports.AddressOfFunctions),
                      funcRVAs.data(), funcRVAs.size() * sizeof(DWORD), nullptr);
    ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + exports.AddressOfNames),
                      nameRVAs.data(), nameRVAs.size() * sizeof(DWORD), nullptr);
    ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + exports.AddressOfNameOrdinals),
                      ordinals.data(), ordinals.size() * sizeof(WORD), nullptr);

    for (DWORD i = 0; i < exports.NumberOfNames; i++) {
        char name[256] = {};
        ReadProcessMemory(hProcess, (LPCVOID)(remoteBase + nameRVAs[i]), name, sizeof(name) - 1, nullptr);
        if (strcmp(name, funcName) == 0) {
            WORD ord = ordinals[i];
            if (ord < exports.NumberOfFunctions)
                return remoteBase + funcRVAs[ord];
        }
    }
    return 0;
}

// ─── Shellcode data block (written to remote process) ─────────────
// The shellcode reads function pointers and parameters from this struct,
// which is placed right after the code in memory.
#pragma pack(push, 1)
struct ShellcodeData {
    uintptr_t fn_RtlAddFunctionTable;   // ntdll!RtlAddFunctionTable
    uintptr_t fn_EntryPoint;            // _DllMainCRTStartup
    uintptr_t imageBase;                // remote DLL base
    DWORD     pdataRVA;                 // .pdata section RVA
    DWORD     pdataSize;                // .pdata section size
    DWORD     reason;                   // DLL_PROCESS_ATTACH (1)
    DWORD     _pad;
};
#pragma pack(pop)

// ─── Manual Map DLL Injection ─────────────────────────────────────
inline bool ManualMapDLL(DWORD pid, const char* dllPath, char* errorOut = nullptr, size_t errorSize = 0) {
    auto setError = [&](const char* msg) {
        if (errorOut && errorSize > 0)
            snprintf(errorOut, errorSize, "%s (error %lu)", msg, GetLastError());
    };

    // ── Step 1: Read DLL file into local buffer ──
    std::ifstream file(dllPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        setError("Failed to open DLL file");
        return false;
    }
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<BYTE> rawDll(fileSize);
    if (!file.read((char*)rawDll.data(), fileSize)) {
        setError("Failed to read DLL file");
        return false;
    }
    file.close();

    // ── Step 2: Parse PE headers ──
    auto* dosHeader = (IMAGE_DOS_HEADER*)rawDll.data();
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        setError("Invalid DOS signature");
        return false;
    }

    auto* ntHeaders = (IMAGE_NT_HEADERS64*)(rawDll.data() + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        setError("Invalid NT signature");
        return false;
    }

    if (ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        setError("DLL is not x64");
        return false;
    }

    DWORD imageSize = ntHeaders->OptionalHeader.SizeOfImage;
    uintptr_t preferredBase = ntHeaders->OptionalHeader.ImageBase;

    // Find .pdata (exception directory) for SEH registration
    auto& exceptionDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    DWORD pdataRVA  = exceptionDir.VirtualAddress;
    DWORD pdataSize = exceptionDir.Size;

    // ── Step 3: Open target process ──
    HANDLE hProcess = OpenProcess(
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ |
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProcess) {
        setError("OpenProcess failed");
        return false;
    }

    // ── Step 4: Allocate memory in target process ──
    LPVOID remoteBase = VirtualAllocEx(hProcess, nullptr, imageSize,
                                        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteBase) {
        setError("VirtualAllocEx failed");
        CloseHandle(hProcess);
        return false;
    }

    // ── Step 5: Prepare local image — map sections ──
    std::vector<BYTE> localImage(imageSize, 0);

    memcpy(localImage.data(), rawDll.data(), ntHeaders->OptionalHeader.SizeOfHeaders);

    auto* sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (sectionHeader[i].SizeOfRawData > 0) {
            memcpy(localImage.data() + sectionHeader[i].VirtualAddress,
                   rawDll.data() + sectionHeader[i].PointerToRawData,
                   sectionHeader[i].SizeOfRawData);
        }
    }

    // ── Step 6: Process relocations ──
    intptr_t delta = (uintptr_t)remoteBase - preferredBase;
    if (delta != 0) {
        auto& relocDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (relocDir.VirtualAddress && relocDir.Size) {
            auto* relocBlock = (IMAGE_BASE_RELOCATION*)(localImage.data() + relocDir.VirtualAddress);
            DWORD relocProcessed = 0;

            while (relocProcessed < relocDir.Size && relocBlock->VirtualAddress) {
                DWORD numEntries = (relocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* entries = (WORD*)((BYTE*)relocBlock + sizeof(IMAGE_BASE_RELOCATION));

                for (DWORD j = 0; j < numEntries; j++) {
                    WORD type   = entries[j] >> 12;
                    WORD offset = entries[j] & 0xFFF;

                    if (type == IMAGE_REL_BASED_DIR64) {
                        uintptr_t* patchAddr = (uintptr_t*)(localImage.data() +
                                                relocBlock->VirtualAddress + offset);
                        *patchAddr += delta;
                    }
                    else if (type == IMAGE_REL_BASED_HIGHLOW) {
                        DWORD* patchAddr = (DWORD*)(localImage.data() +
                                           relocBlock->VirtualAddress + offset);
                        *patchAddr += (DWORD)delta;
                    }
                }

                relocProcessed += relocBlock->SizeOfBlock;
                relocBlock = (IMAGE_BASE_RELOCATION*)((BYTE*)relocBlock + relocBlock->SizeOfBlock);
            }
        }
    }

    // ── Step 7: Resolve imports ──
    auto& importDir = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importDir.VirtualAddress && importDir.Size) {
        auto* importDesc = (IMAGE_IMPORT_DESCRIPTOR*)(localImage.data() + importDir.VirtualAddress);

        while (importDesc->Name) {
            const char* dllName = (const char*)(localImage.data() + importDesc->Name);

            HMODULE hLocalDll = LoadLibraryA(dllName);

            auto* origThunk = (IMAGE_THUNK_DATA64*)(localImage.data() +
                              (importDesc->OriginalFirstThunk ? importDesc->OriginalFirstThunk : importDesc->FirstThunk));
            auto* iatThunk  = (IMAGE_THUNK_DATA64*)(localImage.data() + importDesc->FirstThunk);

            while (origThunk->u1.AddressOfData) {
                uintptr_t funcAddr = 0;

                if (IMAGE_SNAP_BY_ORDINAL64(origThunk->u1.Ordinal)) {
                    WORD ordinal = (WORD)IMAGE_ORDINAL64(origThunk->u1.Ordinal);
                    if (hLocalDll)
                        funcAddr = (uintptr_t)GetProcAddress(hLocalDll, MAKEINTRESOURCEA(ordinal));
                    if (!funcAddr)
                        funcAddr = ResolveImportAddress(hProcess, pid, dllName, MAKEINTRESOURCEA(ordinal));
                } else {
                    auto* importByName = (IMAGE_IMPORT_BY_NAME*)(localImage.data() + origThunk->u1.AddressOfData);
                    const char* funcName = importByName->Name;

                    if (hLocalDll)
                        funcAddr = (uintptr_t)GetProcAddress(hLocalDll, funcName);
                    if (!funcAddr)
                        funcAddr = ResolveImportAddress(hProcess, pid, dllName, funcName);
                }

                if (!funcAddr) {
                    char errBuf[512];
                    snprintf(errBuf, sizeof(errBuf), "Failed to resolve import: %s", dllName);
                    setError(errBuf);
                    VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
                    CloseHandle(hProcess);
                    return false;
                }

                iatThunk->u1.Function = funcAddr;
                origThunk++;
                iatThunk++;
            }

            importDesc++;
        }
    }

    // ── Step 8: Write mapped image to target ──
    if (!WriteProcessMemory(hProcess, remoteBase, localImage.data(), imageSize, nullptr)) {
        setError("WriteProcessMemory (image) failed");
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // ── Step 9: Build shellcode that registers exceptions + calls entry ──
    // Resolve RtlAddFunctionTable from ntdll (same address in all processes)
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    uintptr_t fnRtlAddFunctionTable = (uintptr_t)GetProcAddress(hNtdll, "RtlAddFunctionTable");
    if (!fnRtlAddFunctionTable) {
        setError("Failed to find RtlAddFunctionTable");
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    uintptr_t entryPoint = (uintptr_t)remoteBase + ntHeaders->OptionalHeader.AddressOfEntryPoint;

    // Calculate number of RUNTIME_FUNCTION entries
    DWORD pdataEntryCount = pdataSize / (DWORD)sizeof(RUNTIME_FUNCTION);

    // Shellcode (x64):
    //   sub  rsp, 0x38              ; shadow space (32) + alignment (8) + stack arg (8)
    //
    //   ; --- RtlAddFunctionTable(pdata_ptr, entry_count, image_base) ---
    //   mov  rcx, <imageBase + pdataRVA>  ; FunctionTable
    //   mov  edx, <pdataEntryCount>       ; EntryCount
    //   mov  r8,  <imageBase>             ; BaseAddress
    //   mov  rax, <fnRtlAddFunctionTable>
    //   call rax
    //
    //   ; --- _DllMainCRTStartup(hModule, DLL_PROCESS_ATTACH, NULL) ---
    //   mov  rcx, <imageBase>       ; hModule
    //   mov  edx, 1                 ; DLL_PROCESS_ATTACH
    //   xor  r8d, r8d               ; lpReserved = NULL
    //   mov  rax, <entryPoint>
    //   call rax
    //
    //   add  rsp, 0x38
    //   ret

    BYTE shellcode[] = {
        // sub rsp, 0x38
        0x48, 0x83, 0xEC, 0x38,

        // --- Register exception handlers ---
        // mov rcx, imm64 (pdata address = imageBase + pdataRVA)
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // +4, patched [6..13]
        // mov edx, imm32 (entry count)
        0xBA, 0x00, 0x00, 0x00, 0x00,                                  // +14, patched [15..18]
        // mov r8, imm64 (image base)
        0x49, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // +19, patched [21..28]
        // mov rax, imm64 (RtlAddFunctionTable)
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // +29, patched [31..38]
        // call rax
        0xFF, 0xD0,                                                    // +39

        // --- Call DllMain CRT entry ---
        // mov rcx, imm64 (hModule = imageBase)
        0x48, 0xB9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // +41, patched [43..50]
        // mov edx, 1 (DLL_PROCESS_ATTACH)
        0xBA, 0x01, 0x00, 0x00, 0x00,                                  // +51
        // xor r8d, r8d (lpReserved = NULL)
        0x45, 0x31, 0xC0,                                              // +56
        // mov rax, imm64 (entry point)
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // +59, patched [61..68]
        // call rax
        0xFF, 0xD0,                                                    // +69

        // add rsp, 0x38
        0x48, 0x83, 0xC4, 0x38,                                       // +71
        // ret
        0xC3                                                           // +75
    };

    // Patch addresses into shellcode
    uintptr_t pdataAddr = (uintptr_t)remoteBase + pdataRVA;

    *(uintptr_t*)(shellcode + 6)  = pdataAddr;              // rcx = pdata ptr
    *(DWORD*)   (shellcode + 15)  = pdataEntryCount;        // edx = entry count
    *(uintptr_t*)(shellcode + 21) = (uintptr_t)remoteBase;  // r8  = image base
    *(uintptr_t*)(shellcode + 31) = fnRtlAddFunctionTable;  // rax = RtlAddFunctionTable
    *(uintptr_t*)(shellcode + 43) = (uintptr_t)remoteBase;  // rcx = hModule
    *(uintptr_t*)(shellcode + 61) = entryPoint;             // rax = entry point

    // Allocate and write shellcode
    LPVOID remoteShellcode = VirtualAllocEx(hProcess, nullptr, sizeof(shellcode),
                                            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!remoteShellcode) {
        setError("VirtualAllocEx (shellcode) failed");
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, remoteShellcode, shellcode, sizeof(shellcode), nullptr)) {
        setError("WriteProcessMemory (shellcode) failed");
        VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Execute the shellcode
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                                         (LPTHREAD_START_ROUTINE)remoteShellcode,
                                         nullptr, 0, nullptr);
    if (!hThread) {
        setError("CreateRemoteThread failed");
        VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, remoteBase, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    // Wait briefly for DllMain to return (it just spawns a thread and returns TRUE).
    // Don't block for long — the real init happens in the spawned thread.
    DWORD waitResult = WaitForSingleObject(hThread, 5000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    // ── Step 10: Cleanup ──
    VirtualFreeEx(hProcess, remoteShellcode, 0, MEM_RELEASE);

    // Erase PE headers from remote memory (anti-scan)
    // Keep .pdata alive since it's registered with RtlAddFunctionTable
    std::vector<BYTE> zeros(0x200, 0); // erase DOS + COFF headers only
    WriteProcessMemory(hProcess, remoteBase, zeros.data(), zeros.size(), nullptr);

    CloseHandle(hProcess);

    if (waitResult == WAIT_TIMEOUT) {
        // DllMain took too long — still might work (init thread is running)
        // Don't report failure, the DLL is mapped and running
        return true;
    }

    if (exitCode == 0) {
        setError("DllMain returned FALSE");
        return false;
    }

    return true;
}

// ─── Public API (unchanged interface) ─────────────────────────────
inline bool InjectToASA() {
    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);

    char* lastSlash = strrchr(dllPath, '\\');
    if (lastSlash)
        strcpy(lastSlash + 1, "ASA-Internal.dll");
    else
        strcpy(dllPath, "ASA-Internal.dll");

    if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL, "ASA-Internal.dll not found!", "Error", MB_ICONERROR);
        return false;
    }

    DWORD pid = GetProcessId(L"ArkAscended.exe");
    if (pid == 0) {
        MessageBoxA(NULL, "ArkAscended.exe not running!\nStart the game first.", "Error", MB_ICONERROR);
        return false;
    }

    char errorDetail[512] = {};
    if (!ManualMapDLL(pid, dllPath, errorDetail, sizeof(errorDetail))) {
        char msg[768];
        snprintf(msg, sizeof(msg), "Failed to inject DLL!\n%s\nTry running as Administrator.", errorDetail);
        MessageBoxA(NULL, msg, "Error", MB_ICONERROR);
        return false;
    }

    return true;
}

} // namespace Injector
