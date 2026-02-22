#pragma once

#include <cstdint>
#include <cwchar>
#include "math.h"

struct FName {
    std::uint32_t index;       // ComparisonIndex
    std::uint32_t number = 0;  // Suffix number (0 = no suffix)

    FName() : index(0), number(0) {}
    FName(std::uint32_t _index) : index(_index), number(0) {}
    FName(std::uint32_t _index, std::uint32_t _number) : index(_index), number(_number) {}
};

// ==================== GObjects / GNames structures ====================
// These match the UE5 in-memory layout for ARK Survival Ascended

struct FNameEntryHeader {
    std::uint16_t value;       // bIsWide:1, pad:5, Len:10

    bool IsWide() const { return value & 1; }
    int  GetLen() const { return value >> 6; }
};

struct FNameEntry {
    FNameEntryHeader header;   // 0x0000(0x0002)
    union {
        char     ansi[1024];   // 0x0002 — ANSI name
        wchar_t  wide[512];    // 0x0002 — Wide name
    };

    std::string GetAnsiString() const {
        if (header.IsWide()) {
            // Convert wide to ansi (simple ASCII subset)
            int len = header.GetLen();
            std::string result(len, '\0');
            for (int i = 0; i < len; i++)
                result[i] = (char)wide[i];
            return result;
        }
        return std::string(ansi, header.GetLen());
    }
};

struct FNamePool {
    static constexpr std::uint32_t Stride = 2;
    static constexpr std::uint32_t BlockBits = 16;
    static constexpr std::uint32_t BlockSize = 1 << BlockBits; // 0x10000

    std::uint8_t  pad[0x8];           // 0x0000
    std::uint32_t current_block;      // 0x0008
    std::uint32_t current_cursor;     // 0x000C
    std::uint8_t* blocks[0x2000];     // 0x0010 — 8192 block pointers

    FNameEntry* GetEntry(std::int32_t index) const {
        std::int32_t block = index >> BlockBits;
        std::int32_t offset = index & (BlockSize - 1);
        if (block < 0 || block > (int)current_block) return nullptr;
        if (!blocks[block]) return nullptr;
        return reinterpret_cast<FNameEntry*>(blocks[block] + offset * Stride);
    }
};

struct FUObjectItem {
    void*          object;    // 0x0000 — UObject*
    std::uint8_t   pad[0x10]; // 0x0008
};
static_assert(sizeof(FUObjectItem) == 0x18, "FUObjectItem must be 24 bytes");

struct TUObjectArray {
    static constexpr std::int32_t ElementsPerChunk = 0x10000;

    FUObjectItem** chunks;           // 0x0000
    std::uint8_t   pad[0x8];        // 0x0008
    std::int32_t   max_elements;    // 0x0010
    std::int32_t   num_elements;    // 0x0014
    std::int32_t   max_chunks;      // 0x0018
    std::int32_t   num_chunks;      // 0x001C

    void* GetObjectByIndex(std::int32_t index) const {
        if (index < 0 || index >= num_elements) return nullptr;
        std::int32_t chunk = index / ElementsPerChunk;
        std::int32_t within = index % ElementsPerChunk;
        if (chunk >= num_chunks || !chunks[chunk]) return nullptr;
        return chunks[chunk][within].object;
    }
};

struct alignas(8) FKey {
    FName name;                  // 0x0000(0x0008)
    std::uint8_t details[16];   // 0x0008(0x0010) padding
};
static_assert(sizeof(FKey) == 0x18, "FKey must be 24 bytes");

template <class T>
struct TArray {
    T* data;
    std::int32_t count;
    std::int32_t max;

    TArray() : data(nullptr), count(0), max(0) {}

    T& operator[](int i) { return data[i]; }
    const T& operator[](int i) const { return data[i]; }
    
    int size() const { return count; }
    bool valid(int i) const { return i >= 0 && i < count; }
    bool empty() const { return count == 0; }
};

struct FString : private TArray<wchar_t> {
    FString() {}
    
    FString(const wchar_t* str) {
        if (str && *str) {
            count = max = static_cast<int>(std::wcslen(str)) + 1;
            data = const_cast<wchar_t*>(str);
        }
    }

    wchar_t* c_str() { return data; }
    const wchar_t* c_str() const { return data; }
    bool valid() const { return data != nullptr; }
    int length() const { return count > 0 ? count - 1 : 0; }
};

struct FTextData {
    char pad_0[0x38];
    wchar_t* name;
    std::int32_t length;
};

struct FText {
    FTextData* data;
    char pad_0[0x10];

    wchar_t* c_str() const {
        return data ? data->name : nullptr;
    }
};

using fname = FName;
using fkey = FKey;
using fstring = FString;
using ftext = FText;
using ftextdata = FTextData;

template <class T>
using tarray = TArray<T>;
