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

} // namespace memory

// Global read function for backwards compatibility
template<typename ReadT>
ReadT read(DWORD_PTR address, const ReadT& def = ReadT()) {
    return memory::read<ReadT>(address, def);
}
