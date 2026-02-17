// Hooks.cpp - Hook installation for ARK
#include "ArkCheat.h"

// ============== HP HOOK ==============
// Original: movss [rax+00000850],xmm2  (8 bytes)
// We capture RAX (player base) and XMM2 (HP value)

// Storage for captured values
extern "C" {
    volatile uintptr_t g_CapturedRAX = 0;
    volatile float g_CapturedHP = 0.0f;
}

void* CreateHPHookStub(uintptr_t returnAddr) {
    // Find memory near the target
    uintptr_t targetBase = returnAddr & 0xFFFFFFFF00000000ULL;
    void* stubMem = nullptr;
    
    for (uintptr_t addr = targetBase; addr < targetBase + 0x80000000; addr += 0x10000) {
        stubMem = VirtualAlloc((void*)addr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        if (stubMem) break;
    }
    
    if (!stubMem) {
        stubMem = VirtualAlloc(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    
    if (!stubMem) return nullptr;
    
    printf("  HP stub at %p\n", stubMem);
    
    BYTE* code = (BYTE*)stubMem;
    int idx = 0;
    
    // Save registers
    code[idx++] = 0x50;  // push rax
    code[idx++] = 0x51;  // push rcx
    
    // Store RAX (player base) to our global
    // mov rcx, &g_CapturedRAX
    code[idx++] = 0x48; code[idx++] = 0xB9;
    *(uintptr_t*)&code[idx] = (uintptr_t)&g_CapturedRAX;
    idx += 8;
    // mov [rcx], rax
    code[idx++] = 0x48; code[idx++] = 0x89; code[idx++] = 0x01;
    
    // Store XMM2 (HP value) to our global
    // mov rcx, &g_CapturedHP
    code[idx++] = 0x48; code[idx++] = 0xB9;
    *(uintptr_t*)&code[idx] = (uintptr_t)&g_CapturedHP;
    idx += 8;
    // movss [rcx], xmm2
    code[idx++] = 0xF3; code[idx++] = 0x0F; code[idx++] = 0x11; code[idx++] = 0x11;
    
    // Restore registers
    code[idx++] = 0x59;  // pop rcx
    code[idx++] = 0x58;  // pop rax
    
    // Execute original instruction: movss [rax+00000850],xmm2
    code[idx++] = 0xF3; code[idx++] = 0x0F; code[idx++] = 0x11;
    code[idx++] = 0x90; code[idx++] = 0x50; code[idx++] = 0x08; code[idx++] = 0x00; code[idx++] = 0x00;
    
    // Jump back
    code[idx++] = 0xFF; code[idx++] = 0x25; code[idx++] = 0x00; code[idx++] = 0x00; code[idx++] = 0x00; code[idx++] = 0x00;
    *(uintptr_t*)&code[idx] = returnAddr;
    
    return stubMem;
}

bool InstallHook(uintptr_t addr, void* stub, BYTE* originalBytes, int len) {
    // Save original bytes
    memcpy(originalBytes, (void*)addr, len);
    
    // Calculate relative jump
    intptr_t offset = (intptr_t)stub - (intptr_t)addr - 5;
    
    // Check if 32-bit relative jump works
    if (offset > INT32_MAX || offset < INT32_MIN) {
        printf("  Hook too far, using absolute jump\n");
        
        // Use 14-byte absolute jump: FF 25 00 00 00 00 [8-byte addr]
        if (len < 14) {
            printf("  Not enough space for absolute jump!\n");
            return false;
        }
        
        DWORD oldProtect;
        VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
        
        BYTE* code = (BYTE*)addr;
        code[0] = 0xFF;
        code[1] = 0x25;
        code[2] = 0x00;
        code[3] = 0x00;
        code[4] = 0x00;
        code[5] = 0x00;
        *(uintptr_t*)&code[6] = (uintptr_t)stub;
        
        // NOP remaining bytes
        for (int i = 14; i < len; i++) {
            code[i] = 0x90;
        }
        
        VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
        return true;
    }
    
    // 5-byte relative jump
    DWORD oldProtect;
    VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
    
    BYTE* code = (BYTE*)addr;
    code[0] = 0xE9;
    *(int32_t*)&code[1] = (int32_t)offset;
    
    // NOP remaining bytes
    for (int i = 5; i < len; i++) {
        code[i] = 0x90;
    }
    
    VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
    return true;
}

void RemoveHook(uintptr_t addr, BYTE* originalBytes, int len) {
    DWORD oldProtect;
    VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)addr, originalBytes, len);
    VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
}
