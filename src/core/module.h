#pragma once


#include <cstdint>

// Prevent Windows.h from defining min/max macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace module {

    namespace detail {
        struct list_entry {
            struct list_entry* Flink;
            struct list_entry* Blink;
        };

        struct unicode_string {
            unsigned short Length;
            unsigned short MaximumLength;
            wchar_t* Buffer;
        };

        struct peb_ldr_data {
            unsigned long Length;
            unsigned long Initialized;
            const char* SsHandle;
            list_entry InLoadOrderModuleList;
            list_entry InMemoryOrderModuleList;
            list_entry InInitializationOrderModuleList;
        };

        struct peb {
            unsigned char Reserved1[2];
            unsigned char BeingDebugged;
            unsigned char Reserved2[1];
            const char* Reserved3[2];
            peb_ldr_data* Ldr;
        };

        struct ldr_data_table_entry {
            list_entry InLoadOrderModuleList;
            list_entry InMemoryOrderLinks;
            list_entry InInitializationOrderModuleList;
            void* DllBase;
            void* EntryPoint;
            union {
                unsigned long SizeOfImage;
                const char* _dummy;
            };
            unicode_string FullDllName;
            unicode_string BaseDllName;
        };

        inline int wcslen(const wchar_t* str) {
            int counter = 0;
            if (!str) return 0;
            for (; *str != '\0'; ++str)
                ++counter;
            return counter;
        }

        inline bool wcsicmp_insensitive(const wchar_t* cs, const wchar_t* ct) {
            auto len = wcslen(cs);
            if (len != wcslen(ct))
                return false;

            for (size_t i = 0; i < (size_t)len; i++)
                if ((cs[i] | L' ') != (ct[i] | L' '))
                    return false;

            return true;
        }
    } // namespace detail

    #define CONTAINS_RECORD(address, type, field) \
        ((type*)((char*)(address) - (std::uintptr_t)(&((type*)0)->field)))

    inline std::uintptr_t get_module(const wchar_t* name) {
        const detail::peb* peb = reinterpret_cast<detail::peb*>(__readgsqword(0x60));
        if (!peb) return 0;

        const detail::list_entry head = peb->Ldr->InMemoryOrderModuleList;

        for (auto curr = head; curr.Flink != &peb->Ldr->InMemoryOrderModuleList; curr = *curr.Flink) {
            detail::ldr_data_table_entry* mod = CONTAINS_RECORD(
                curr.Flink, 
                detail::ldr_data_table_entry, 
                InMemoryOrderLinks
            );

            if (mod->BaseDllName.Buffer) {
                if (detail::wcsicmp_insensitive(mod->BaseDllName.Buffer, name))
                    return std::uintptr_t(mod->DllBase);
            }
        }

        return 0;
    }

} // namespace module

// Backwards compatibility
inline std::uintptr_t get_module(const wchar_t* name) {
    return module::get_module(name);
}
