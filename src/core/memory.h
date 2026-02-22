#pragma once

// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <cstdint>

namespace memory {

    inline bool is_bad_ptr(LPVOID ptr, UINT_PTR size = 8) {
        if (ptr == nullptr) return true;
        return IsBadReadPtr(ptr, size) != 0;
    }

    inline bool is_valid_ptr(uintptr_t address) {
        if (address < 0x10000) return false; // Below minimum user-mode address
        return !is_bad_ptr((LPVOID)address, 8);
    }

    template<typename T>
    T read(uintptr_t address, const T& default_value = T()) {
        if (is_valid_ptr(address)) {
            __try {
                return *(T*)(address);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return default_value;
            }
        }
        return default_value;
    }

    template<typename T>
    void write(uintptr_t address, const T& value) {
        if (is_valid_ptr(address)) {
            __try {
                *(T*)(address) = value;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                // Silently fail
            }
        }
    }

    // Pattern scanner — finds byte patterns with wildcards in module memory
    // Pattern format: "48 89 5C 24 ? 48 89 6C 24 ?" where ? is a wildcard
    inline std::uintptr_t pattern_scan(std::uintptr_t module_base, const char* pattern) {
        if (!module_base || !pattern) return 0;

        // Parse PE header for module size
        auto dos = (IMAGE_DOS_HEADER*)module_base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        auto nt = (IMAGE_NT_HEADERS*)(module_base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
        auto size = nt->OptionalHeader.SizeOfImage;

        // Parse pattern string into bytes and mask
        unsigned char bytes[256];
        bool mask[256];
        int len = 0;

        for (const char* p = pattern; *p && len < 256;) {
            while (*p == ' ') p++;
            if (!*p) break;

            if (*p == '?') {
                bytes[len] = 0;
                mask[len] = false; // wildcard
                len++;
                p++;
                if (*p == '?') p++; // skip second ? in ??
            } else {
                // Parse hex byte
                unsigned char b = 0;
                for (int i = 0; i < 2 && *p; i++, p++) {
                    b <<= 4;
                    if (*p >= '0' && *p <= '9') b |= (*p - '0');
                    else if (*p >= 'A' && *p <= 'F') b |= (*p - 'A' + 10);
                    else if (*p >= 'a' && *p <= 'f') b |= (*p - 'a' + 10);
                }
                bytes[len] = b;
                mask[len] = true; // must match
                len++;
            }
        }

        if (len == 0) return 0;

        // Scan module memory
        auto scan_start = (unsigned char*)module_base;
        auto scan_end = scan_start + size - len;

        for (auto current = scan_start; current < scan_end; current++) {
            bool found = true;
            for (int i = 0; i < len; i++) {
                if (mask[i] && current[i] != bytes[i]) {
                    found = false;
                    break;
                }
            }
            if (found) return (std::uintptr_t)current;
        }

        return 0;
    }

} // namespace memory

// Global read function for backwards compatibility
template<typename ReadT>
ReadT read(DWORD_PTR address, const ReadT& def = ReadT()) {
    return memory::read<ReadT>(address, def);
}
