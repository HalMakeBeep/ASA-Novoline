#pragma once

#include <cstdint>
#include "console.h"

namespace hooks {

    // VMT hook - swaps a vtable entry with our function
    template <typename HookType>
    __declspec(noinline) HookType vmt_hook(std::uintptr_t address, std::uintptr_t hook_function, int index) {
        if (!address) {
            dbg::log_ex(dbg::Level::Error, dbg::Hook, "vmt_hook address is null");
            return HookType(nullptr);
        }

        auto vtable = *(std::uintptr_t**)address;
        if (!vtable) {
            dbg::log_ex(dbg::Level::Error, dbg::Hook, "vtable pointer is null at 0x%llX", (unsigned long long)address);
            return HookType(nullptr);
        }

        dbg::log_ex(dbg::Level::Debug, dbg::Hook, "vtable at 0x%p, hooking index %d", vtable, index);

        // Calculate vtable size by walking until we hit a null entry
        int vtable_size = 0;
        __try {
            do vtable_size += 1;
            while (*(std::uintptr_t*)(std::uintptr_t(vtable) + (vtable_size * 8)));
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::log_ex(dbg::Level::Warn, dbg::Hook, "exception calculating vtable size, using size=%d", vtable_size);
        }

        if (vtable_size <= index) {
            dbg::log_ex(dbg::Level::Error, dbg::Hook, "vtable size (%d) <= index (%d), out of bounds", vtable_size, index);
            return HookType(nullptr);
        }

        dbg::log_ex(dbg::Level::Debug, dbg::Hook, "vtable size=%d entries", vtable_size);

        auto original_function = (void*)vtable[index];
        if (!original_function) {
            dbg::log_ex(dbg::Level::Warn, dbg::Hook, "original function at index %d is null", index);
        }

        // Create fake vtable — allocate vtable_size entries (each is sizeof(uintptr_t) = 8 bytes)
        std::uintptr_t* fake_vtable = new std::uintptr_t[vtable_size];

        for (int i = 0; i < vtable_size; i++) {
            if (i == index) continue;
            fake_vtable[i] = *(std::uintptr_t*)(std::uintptr_t(vtable) + (i * 8));
        }
        fake_vtable[index] = hook_function;

        // Swap vtable pointer
        *(std::uintptr_t**)address = fake_vtable;

        dbg::log_ex(dbg::Level::Info, dbg::Hook, "hooked index %d original=0x%p hook=0x%llX",
            index, original_function, (unsigned long long)hook_function);

        return HookType(original_function);
    }

} // namespace hooks

// Backwards compatibility
template <typename hook_type>
__declspec(noinline) hook_type vmt(std::uintptr_t address, std::uintptr_t hook_function, int index) {
    return hooks::vmt_hook<hook_type>(address, hook_function, index);
}
