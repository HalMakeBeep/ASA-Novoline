#pragma once

#include <cstdint>
#include <cwchar>
#include "math.h"

struct FName {
    std::uint32_t index;
    
    FName() : index(0) {}
    FName(std::uint32_t _index) : index(_index) {}
};

struct FKey {
    FName name;
    std::uint8_t details[20];
};

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
