#pragma once

#include <cstdint>
#include "../core/memory.h"
#include "../core/console.h"
#include "offsets.h"
#include "ue_types.h"
#include "math.h"

// fdec for game base
extern std::uintptr_t game;

// Global pointers to GObjects and GNames — initialized in sdk::Initialize()
inline TUObjectArray*  g_objects = nullptr;
inline FNamePool*      g_names  = nullptr;

// Resolve an FName index to a char buffer (no std::string — SEH safe)
// Returns length written, 0 on failure
inline int GetNameToBuffer(std::int32_t index, char* buf, int buf_size) {
    if (!g_names || !buf || buf_size <= 0) { if (buf) buf[0] = '\0'; return 0; }
    buf[0] = '\0';

    auto* entry = g_names->GetEntry(index);
    if (!entry) return 0;

    int len = entry->header.GetLen();
    if (len <= 0 || len >= buf_size) return 0;

    if (entry->header.IsWide()) {
        for (int i = 0; i < len; i++)
            buf[i] = (char)entry->wide[i];
    } else {
        for (int i = 0; i < len; i++)
            buf[i] = entry->ansi[i];
    }
    buf[len] = '\0';
    return len;
}

// Build "Outer.Name" path into buffer (SEH safe)
inline int GetObjectPathToBuffer(void* obj_ptr, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    if (!obj_ptr || !memory::is_valid_ptr((std::uintptr_t)obj_ptr)) return 0;

    auto obj_name = read<FName>((std::uintptr_t)obj_ptr + offsets::UObject_Name);
    char name_buf[256];
    int name_len = GetNameToBuffer(obj_name.index, name_buf, sizeof(name_buf));
    if (name_len == 0) return 0;

    auto* outer_ptr = read<void*>((std::uintptr_t)obj_ptr + 0x20);
    if (outer_ptr && memory::is_valid_ptr((std::uintptr_t)outer_ptr)) {
        auto outer_name = read<FName>((std::uintptr_t)outer_ptr + offsets::UObject_Name);
        char outer_buf[256];
        int outer_len = GetNameToBuffer(outer_name.index, outer_buf, sizeof(outer_buf));
        if (outer_len > 0 && (outer_len + 1 + name_len) < buf_size) {
            for (int i = 0; i < outer_len; i++) buf[i] = outer_buf[i];
            buf[outer_len] = '.';
            for (int i = 0; i < name_len; i++) buf[outer_len + 1 + i] = name_buf[i];
            buf[outer_len + 1 + name_len] = '\0';
            return outer_len + 1 + name_len;
        }
    }

    for (int i = 0; i <= name_len; i++) buf[i] = name_buf[i]; // includes null
    return name_len;
}

// Case-insensitive string compare (plain C)
inline bool streqi(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return false;
        a++; b++;
    }
    return *a == *b;
}

// Forward declare for sentinel
class UObject;

// Sentinel value: means "we already tried looking this up and it failed"
inline UObject* FUNC_NOT_FOUND = reinterpret_cast<UObject*>(1);

// Cached function lookup — only scans GObjects once per function
#define FIND_FUNC_CACHED(var, search_name) \
    do { if (!(var)) { (var) = UObject::StaticFindObject(nullptr, nullptr, search_name); \
         if (!(var)) (var) = FUNC_NOT_FOUND; } } while(0)
#define FUNC_VALID(var) ((var) && (var) != FUNC_NOT_FOUND)

class UObject {
public:
    FName GetName() {
        return read<FName>(std::uintptr_t(this) + offsets::UObject_Name);
    }

    // Find object by iterating GObjects and comparing names
    static UObject* FindObject(const wchar_t* name, UObject* outer = nullptr) {
        return StaticFindObject(nullptr, outer, name);
    }

    // Helper: check if 'haystack' ends with 'needle' (case-insensitive)
    static bool str_ends_with_i(const char* haystack, const char* needle) {
        int hlen = 0, nlen = 0;
        while (haystack[hlen]) hlen++;
        while (needle[nlen]) nlen++;
        if (nlen > hlen) return false;
        return streqi(haystack + (hlen - nlen), needle);
    }

    // Helper: check if 'haystack' contains 'needle' (case-insensitive)
    static bool str_contains_i(const char* haystack, const char* needle) {
        int nlen = 0;
        while (needle[nlen]) nlen++;
        for (int i = 0; haystack[i]; i++) {
            bool match = true;
            for (int j = 0; j < nlen; j++) {
                if (!haystack[i + j]) { match = false; break; }
                char a = haystack[i + j]; if (a >= 'A' && a <= 'Z') a += 32;
                char b = needle[j]; if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) { match = false; break; }
            }
            if (match) return true;
        }
        return false;
    }

    static UObject* StaticFindObject(UObject* klass, UObject* outer, const wchar_t* name, bool exact = false) {
        if (!g_objects || !g_names) {
            dbg::error("StaticFindObject: GObjects or GNames not initialized!");
            return nullptr;
        }

        // Convert wide name to ANSI for comparison
        char target[512];
        int target_len = 0;
        if (name) {
            for (int i = 0; name[i] && i < 511; i++) {
                target[i] = (char)name[i];
                target_len++;
            }
        }
        target[target_len] = '\0';

        // Split into outer_part and name_part on the last '.'
        const char* dot = nullptr;
        for (int i = target_len - 1; i >= 0; i--) {
            if (target[i] == '.') { dot = &target[i]; break; }
        }

        const char* search_outer = nullptr;
        const char* search_name = target;
        char outer_search_buf[256] = {};

        if (dot) {
            int outer_len = (int)(dot - target);
            for (int i = 0; i < outer_len && i < 255; i++)
                outer_search_buf[i] = target[i];
            outer_search_buf[outer_len] = '\0';
            search_outer = outer_search_buf;
            search_name = dot + 1;
        }

        // Iterate GObjects
        __try {
            int count = g_objects->num_elements;
            for (int i = 0; i < count; i++) {
                void* obj = g_objects->GetObjectByIndex(i);
                if (!obj) continue;

                // Read object name
                auto obj_name = read<FName>((std::uintptr_t)obj + offsets::UObject_Name);
                char obj_name_buf[256];
                if (GetNameToBuffer(obj_name.index, obj_name_buf, sizeof(obj_name_buf)) == 0) continue;

                if (!streqi(obj_name_buf, search_name)) continue;

                // If we need to match outer name
                if (search_outer) {
                    auto* obj_outer = read<void*>((std::uintptr_t)obj + 0x20);
                    if (!obj_outer || !memory::is_valid_ptr((std::uintptr_t)obj_outer)) continue;

                    auto outer_fname = read<FName>((std::uintptr_t)obj_outer + offsets::UObject_Name);
                    char outer_name_buf[256];
                    if (GetNameToBuffer(outer_fname.index, outer_name_buf, sizeof(outer_name_buf)) == 0) continue;

                    // Match: exact, ends-with (for /Script/Engine → Engine), or contains
                    if (!streqi(outer_name_buf, search_outer) &&
                        !str_ends_with_i(outer_name_buf, search_outer) &&
                        !str_contains_i(outer_name_buf, search_outer)) {
                        continue;
                    }
                }

                // If specific outer pointer was passed
                if (outer && outer != reinterpret_cast<UObject*>(-1)) {
                    auto* obj_outer = read<void*>((std::uintptr_t)obj + 0x20);
                    if (obj_outer != outer) continue;
                }

                return (UObject*)obj;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::error("StaticFindObject: exception during GObjects iteration for '%s'", target);
        }

        return nullptr;
    }

    void ProcessEvent(UObject* function, void* params) {
        auto vtable = *reinterpret_cast<void***>(this);
        reinterpret_cast<void(*)(void*, void*, void*)>(vtable[offsets::ProcessEventVIdx])(this, function, params);
    }
};

class USkeletalMeshComponent : public UObject {
public:
    std::int32_t GetNumBones() {
        struct { std::int32_t ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"SkinnedMeshComponent.GetNumBones");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    FVector GetBoneLocation(std::int32_t index) {
        if (offsets::BoneMatrix == 0) return FVector();

        FMatrix matrix = {};
        reinterpret_cast<FMatrix* (*)(USkeletalMeshComponent*, FMatrix*, std::int32_t)>
            (game + offsets::BoneMatrix)(this, &matrix, index);

        return FVector(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
    }

    void SetRenderCustomDepth(bool value) {
        struct { bool bValue; } params = { value };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimitiveComponent.SetRenderCustomDepth");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void SetCustomDepthStencilValue(std::int32_t value) {
        struct { std::int32_t Value; } params = { value };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimitiveComponent.SetCustomDepthStencilValue");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void SetVectorParamOnMaterials(FName param_name, FLinearColor value) {
        struct { FName name; FLinearColor val; } params = { param_name, value };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"MeshComponent.SetVectorParameterValueOnMaterials");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void SetScalarParamOnMaterials(FName param_name, float value) {
        struct { FName name; float val; } params = { param_name, value };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"MeshComponent.SetScalarParameterValueOnMaterials");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void SetOverlayMaterial(UObject* material) {
        struct { UObject* mat; } params = { material };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"MeshComponent.SetOverlayMaterial");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    UObject* CreateDynamicMaterial(std::int32_t slot = 0) {
        struct { std::int32_t idx; UObject* src; FName name; UObject* ret; } params = { slot, nullptr, FName(), nullptr };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimitiveComponent.CreateDynamicMaterialInstance");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }
};


class AActor : public UObject {
public:
    FVector GetActorLocation() {
        struct { FVector ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Actor.K2_GetActorLocation");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    FRotator GetActorRotation() {
        struct { FRotator ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Actor.K2_GetActorRotation");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    bool WasRecentlyRendered(float tolerance = 0.1f) {
        struct { float tolerance; bool ret; } params = { tolerance };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Actor.WasRecentlyRendered");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    bool SetActorLocation(FVector location, bool sweep = false) {
        struct { FVector loc; bool sweep; uint8_t hit; bool teleport; bool ret; } params = { location, sweep, 0, true };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Actor.K2_SetActorLocation");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    void GetActorBounds(bool onlyColliding, FVector* origin, FVector* extent, bool includeChildren = false) {
        struct {
            bool bOnlyCollidingComponents;   // 0x00
            uint8_t pad1[7];                 // 0x01
            FVector Origin;                  // 0x08
            FVector BoxExtent;               // 0x20
            bool bIncludeFromChildActors;    // 0x38
        } params = {};
        params.bOnlyCollidingComponents = onlyColliding;
        params.bIncludeFromChildActors = includeChildren;
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Actor.GetActorBounds");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        if (origin) *origin = params.Origin;
        if (extent) *extent = params.BoxExtent;
    }

    UObject* GetComponentByClass(UObject* component_class) {
        struct {
            UObject* ComponentClass;
            UObject* ReturnValue;
        } params = { component_class, nullptr };

        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Actor.GetComponentByClass");
        if (!FUNC_VALID(func)) return nullptr;

        __try {
            ProcessEvent(func, &params);
            return params.ReturnValue;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return nullptr;
        }
    }
};

class APrimalCharacter : public AActor {
public:
    USkeletalMeshComponent* GetMesh() {
        return read<USkeletalMeshComponent*>(std::uintptr_t(this) + offsets::Character_Mesh);
    }

    bool IsDead() {
        struct { bool ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimalActor.IsDead");
        if (FUNC_VALID(func)) {
            ProcessEvent(func, &params);
            return params.ret;
        }
        return false;
    }

    int GetTargetingTeam() {
        return read<int>(std::uintptr_t(this) + offsets::Character_TargetTeam);
    }

    FString GetTribeName() {
        return read<FString>(std::uintptr_t(this) + offsets::Character_TribeName);
    }

    bool IsAlliedWithOtherTeam(int other_team) {
        struct { int otherTeamID; bool ret; } params = { other_team, false };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimalCharacter.IsAlliedWithOtherTeam");
        if (!FUNC_VALID(func)) return false;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return params.ret;
    }

    float GetHealth() {
        struct { float ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimalCharacter.GetHealth");
        if (FUNC_VALID(func)) {
            __try { ProcessEvent(func, &params); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
            return params.ret;
        }
        return 0.0f;
    }

    float GetMaxHealth() {
        struct { float ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PrimalCharacter.GetMaxHealth");
        if (FUNC_VALID(func)) {
            __try { ProcessEvent(func, &params); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
            return params.ret;
        }
        return 0.0f;
    }
};

class AShooterCharacter : public APrimalCharacter {
public:
    FString GetPlayerName() {
        return read<FString>(std::uintptr_t(this) + offsets::ShooterCharacter_PlayerName);
    }
};

class APrimalDinoCharacter : public APrimalCharacter {
public:
    bool IsTamed() {
        // TODO: Find offset from SDK dump
        return false;
    }

    FString GetTamedName() {
        // TODO: Find offset from SDK dump
        return FString();
    }
};

class APlayerController : public UObject {
public:
    float GetInputPitchScale() {
        return read<float>(std::uintptr_t(this) + offsets::Controller_InputPitchScale);
    }

    float GetInputYawScale() {
        return read<float>(std::uintptr_t(this) + offsets::Controller_InputYawScale);
    }

    bool IsInputKeyDown(FKey key) {
        struct { FKey key; bool ret; } params = { key };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerController.IsInputKeyDown");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    bool ProjectWorldToScreen(FVector world, FVector2D* screen) {
        struct {
            FVector WorldLocation;              // 0x00 (0x18)
            FVector2D ScreenLocation;           // 0x18 (0x10)
            bool bPlayerViewportRelative;       // 0x28
            bool ReturnValue;                   // 0x29
        } params = {};
        params.WorldLocation = world;
        params.bPlayerViewportRelative = true;
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerController.ProjectWorldLocationToScreen");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        *screen = params.ScreenLocation;
        return params.ReturnValue;
    }

    APrimalCharacter* GetPawn() {
        struct { APrimalCharacter* ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Controller.K2_GetPawn");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    void AddPitchInput(float val) {
        struct { float val; } params = { val };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerController.AddPitchInput");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void AddYawInput(float val) {
        struct { float val; } params = { val };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerController.AddYawInput");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void SetIgnoreLookInput(bool value) {
        struct { bool bNewLookInput; } params = { value };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Controller.SetIgnoreLookInput");
        if (!FUNC_VALID(func)) return;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void SetIgnoreMoveInput(bool value) {
        struct { bool bNewMoveInput; } params = { value };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Controller.SetIgnoreMoveInput");
        if (!FUNC_VALID(func)) return;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void ResetIgnoreInputFlags() {
        struct {} params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Controller.ResetIgnoreInputFlags");
        if (!FUNC_VALID(func)) return;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    FVector2D GetMousePosition() {
        struct { float x; float y; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerController.GetMousePosition");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return FVector2D(params.x, params.y);
    }
};

class APlayerCameraManager : public UObject {
public:
    FVector GetCameraLocation() {
        struct { FVector ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerCameraManager.GetCameraLocation");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    FRotator GetCameraRotation() {
        struct { FRotator ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerCameraManager.GetCameraRotation");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    float GetFOVAngle() {
        struct { float ret; } params = {};
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"PlayerCameraManager.GetFOVAngle");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }
};

class UCanvas : public UObject {
public:
    float ClipX() { return read<float>(std::uintptr_t(this) + offsets::Canvas_ClipX); }
    float ClipY() { return read<float>(std::uintptr_t(this) + offsets::Canvas_ClipY); }

    FVector2D K2_TextSize(UObject* font, FString text, FVector2D scale) {
        struct { UObject* font; FString text; FVector2D scale; FVector2D ret; } params = { font, text, scale };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Canvas.K2_TextSize");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    void K2_DrawText(UObject* font, FString text, FVector2D pos, FVector2D scale, FLinearColor color,
                     float kerning, FLinearColor shadow, FVector2D shadowOffset,
                     bool centerX, bool centerY, bool outlined, FLinearColor outlineColor) {
        struct {
            UObject* font; FString text; FVector2D pos; FVector2D scale;
            FLinearColor color; float kerning; FLinearColor shadow; FVector2D shadowOffset;
            bool centerX; bool centerY; bool outlined; FLinearColor outlineColor;
        } params = { font, text, pos, scale, color, kerning, shadow, shadowOffset, centerX, centerY, outlined, outlineColor };

        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Canvas.K2_DrawText");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

    void K2_DrawLine(FVector2D a, FVector2D b, float thickness, FLinearColor color) {
        struct { FVector2D a; FVector2D b; float thickness; FLinearColor color; } params = { a, b, thickness, color };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"Canvas.K2_DrawLine");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
    }

};

class UWorld : public UObject {};

class UEngine : public UObject {
public:
    UObject* GetFont() { return read<UObject*>(std::uintptr_t(this) + offsets::Engine_Font); }
};

class UGameViewportClient : public UObject {
public:
    UWorld* GetWorld() { return read<UWorld*>(std::uintptr_t(this) + offsets::Viewport_World); }
};

class ULocalPlayer : public UObject {
public:
    UGameViewportClient* GetViewport() { return read<UGameViewportClient*>(std::uintptr_t(this) + offsets::LocalPlayer_Viewport); }
};

class UGameInstance : public UObject {
public:
    TArray<ULocalPlayer*> GetLocalPlayers() { return read<TArray<ULocalPlayer*>>(std::uintptr_t(this) + offsets::GameInstance_LocalPlayers); }
};

namespace kismet {

class GameplayStatics : public UObject {
public:
    TArray<UObject*> GetAllActorsOfClass(UObject* world, UObject* actorClass) {
        struct { UObject* world; UObject* actorClass; TArray<UObject*> actors; } params = { world, actorClass };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"GameplayStatics.GetAllActorsOfClass");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.actors;
    }

    UGameInstance* GetGameInstance(UObject* world) {
        struct { UObject* world; UGameInstance* ret; } params = { world };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"GameplayStatics.GetGameInstance");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    APlayerController* GetPlayerController(UObject* world, int index) {
        struct { UObject* world; int index; APlayerController* ret; } params = { world, index };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"GameplayStatics.GetPlayerController");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    APlayerCameraManager* GetPlayerCameraManager(UObject* world, int index) {
        struct { UObject* world; int index; APlayerCameraManager* ret; } params = { world, index };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"GameplayStatics.GetPlayerCameraManager");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    double GetWorldDeltaSeconds(UObject* world) {
        struct { UObject* world; double ret; } params = { world };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"GameplayStatics.GetWorldDeltaSeconds");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }
};

class SystemLibrary : public UObject {
public:
    FString GetObjectName(UObject* object) {
        struct { UObject* obj; FString ret; } params = { object };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetSystemLibrary.GetObjectName");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    UObject* GetOuterObject(UObject* object) {
        struct { UObject* obj; UObject* ret; } params = { object };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetSystemLibrary.GetOuterObject");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }
};

class StringLibrary : public UObject {
public:
    FName StringToName(FString str) {
        struct { FString str; FName ret; } params = { str };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetStringLibrary.Conv_StringToName");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    FString BuildStringDouble(FString append, FString prefix, double value, FString suffix) {
        struct { FString a; FString p; double v; FString s; FString ret; } params = { append, prefix, value, suffix };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetStringLibrary.BuildString_Double");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    bool Contains(FString searchIn, FString subString, bool useCase = false, bool searchFromEnd = false) {
        struct { FString a; FString b; bool c; bool d; bool ret; } params = { searchIn, subString, useCase, searchFromEnd };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetStringLibrary.Contains");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }
};

class MathLibrary : public UObject {
public:
    double VectorDistance(FVector a, FVector b) { return a.distance(b); }
    double Distance2D(FVector2D a, FVector2D b) { return a.distance(b); }
    double Round(double v) { return std::round(v); }
    double Abs(double v) { return std::abs(v); }
    double Sin(double v) { return std::sin(v); }
    double Cos(double v) { return std::cos(v); }

    FRotator FindLookAtRotation(FVector start, FVector target) {
        struct { FVector start; FVector target; FRotator ret; } params = { start, target };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetMathLibrary.FindLookAtRotation");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }

    FRotator RInterpTo(FRotator current, FRotator target, float deltaTime, float speed) {
        struct { FRotator c; FRotator t; float d; float s; FRotator ret; } params = { current, target, deltaTime, speed };
        static UObject* func = nullptr;
        FIND_FUNC_CACHED(func, L"KismetMathLibrary.RInterpTo");
        if (FUNC_VALID(func)) ProcessEvent(func, &params);
        return params.ret;
    }
};

} // namespace kismet

namespace sdk {
    inline kismet::GameplayStatics* GameStatics = nullptr;
    inline kismet::SystemLibrary* System = nullptr;
    inline kismet::StringLibrary* String = nullptr;
    inline kismet::MathLibrary* Math = nullptr;

    // Actor classes for GetAllActorsOfClass
    inline UObject* PrimalCharacterClass = nullptr;
    inline UObject* DinoCharacterClass = nullptr;
    inline UObject* PlayerCharacterClass = nullptr;
    inline UObject* DroppedItemClass = nullptr;
    inline UObject* StructureClass = nullptr;
    inline UObject* CameraComponentClass = nullptr;

    // Input keys
    inline FKey InsertKey = {};
    inline FKey SpaceKey = {};
    inline FKey LeftMouseButton = {};
    inline FKey RightMouseButton = {};

    // Track init status for debug display
    inline bool init_complete = false;

    inline bool Initialize() {
        dbg::info("SDK::Initialize() starting...");

        // Initialize GObjects and GNames for our own FindObject implementation
        g_objects = reinterpret_cast<TUObjectArray*>(game + offsets::GObjects);
        g_names  = reinterpret_cast<FNamePool*>(game + offsets::GNames);

        // Validate GObjects
        __try {
            dbg::info("GObjects: %d elements, %d chunks",
                g_objects->num_elements, g_objects->num_chunks);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::error("Cannot read GObjects structure at offset 0x%llX!", (unsigned long long)offsets::GObjects);
            g_objects = nullptr; g_names = nullptr;
            return false;
        }

        if (g_objects->num_elements <= 0 || g_objects->num_elements > 5000000) {
            dbg::warn("GObjects num_elements=%d, trying +0x10...", g_objects->num_elements);
            g_objects = reinterpret_cast<TUObjectArray*>(game + offsets::GObjects + 0x10);
            __try {
                if (g_objects->num_elements <= 0 || g_objects->num_elements > 5000000) {
                    dbg::error("GObjects invalid after +0x10 (num=%d)", g_objects->num_elements);
                    g_objects = nullptr; g_names = nullptr;
                    return false;
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                dbg::error("GObjects+0x10 also failed!");
                g_objects = nullptr; g_names = nullptr;
                return false;
            }
        }

        // Validate GNames
        __try {
            if (g_names->current_block > 8192 || !g_names->blocks[0]) {
                dbg::warn("GNames looks invalid, trying +0x10...");
                g_names = reinterpret_cast<FNamePool*>(game + offsets::GNames + 0x10);
            }
            dbg::info("GNames: block=%d, blocks[0]=0x%llX",
                g_names->current_block, (unsigned long long)g_names->blocks[0]);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::error("Cannot read GNames structure!");
            g_names = nullptr;
            return false;
        }

        dbg::log("[1/10] Finding GameplayStatics...");
        GameStatics = (kismet::GameplayStatics*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__GameplayStatics");
        dbg::dump_ptr("GameStatics", GameStatics);

        dbg::log("[2/10] Finding KismetSystemLibrary...");
        System = (kismet::SystemLibrary*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__KismetSystemLibrary");
        dbg::dump_ptr("System", System);

        dbg::log("[3/10] Finding KismetStringLibrary...");
        String = (kismet::StringLibrary*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__KismetStringLibrary");
        dbg::dump_ptr("String", String);

        dbg::log("[4/10] Finding KismetMathLibrary...");
        Math = (kismet::MathLibrary*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__KismetMathLibrary");
        dbg::dump_ptr("Math", Math);

        dbg::log("[5/10] Finding PrimalCharacter class...");
        PrimalCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.PrimalCharacter");
        if (!PrimalCharacterClass) {
            dbg::warn("ShooterGame.PrimalCharacter not found, trying fallback...");
            PrimalCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"PrimalCharacter");
        }
        dbg::dump_ptr("PrimalCharacterClass", PrimalCharacterClass);

        dbg::log("[6/10] Finding PrimalDinoCharacter class...");
        DinoCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.PrimalDinoCharacter");
        if (!DinoCharacterClass) {
            dbg::warn("ShooterGame.PrimalDinoCharacter not found, trying fallback...");
            DinoCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"PrimalDinoCharacter");
        }
        dbg::dump_ptr("DinoCharacterClass", DinoCharacterClass);

        dbg::log("[7/10] Finding ShooterCharacter class...");
        PlayerCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.ShooterCharacter");
        if (!PlayerCharacterClass) {
            dbg::warn("ShooterGame.ShooterCharacter not found, trying fallback...");
            PlayerCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterCharacter");
        }
        dbg::dump_ptr("PlayerCharacterClass", PlayerCharacterClass);

        dbg::log("[8/10] Finding DroppedItem and PrimalStructure...");
        DroppedItemClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.DroppedItem");
        StructureClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.PrimalStructure");
        dbg::dump_ptr("DroppedItemClass", DroppedItemClass);
        dbg::dump_ptr("StructureClass", StructureClass);

        dbg::log("[9/10] Finding CameraComponent class...");
        CameraComponentClass = UObject::StaticFindObject(nullptr, nullptr, L"Engine.CameraComponent");
        if (!CameraComponentClass) {
            dbg::warn("Engine.CameraComponent not found, trying fallback...");
            CameraComponentClass = UObject::StaticFindObject(nullptr, nullptr, L"CameraComponent");
        }
        dbg::dump_ptr("CameraComponentClass", CameraComponentClass);

        dbg::log("[10/10] Initializing input keys...");
        if (String) {
            __try {
                InsertKey = FKey{ String->StringToName(L"Insert"), {} };
                SpaceKey = FKey{ String->StringToName(L"SpaceBar"), {} };
                LeftMouseButton = FKey{ String->StringToName(L"LeftMouseButton"), {} };
                RightMouseButton = FKey{ String->StringToName(L"RightMouseButton"), {} };
                dbg::success("Input keys initialized");
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                dbg::error("EXCEPTION initializing input keys! ProcessEvent offset may be wrong.");
                dbg::error("ProcessEvent VIdx = %d", offsets::ProcessEventVIdx);
            }
        } else {
            dbg::error("String library is NULL - cannot initialize input keys");
        }

        dbg::log("[11/11] Validating critical objects...");
        int failures = 0;
        if (!GameStatics) { dbg::error("CRITICAL: GameStatics is NULL"); failures++; }
        if (!System)      { dbg::warn("System library is NULL - GetObjectName won't work"); }
        if (!String)      { dbg::warn("String library is NULL - input keys won't work"); }
        if (!Math)        { dbg::warn("Math library is NULL - some calculations won't work"); }
        if (!CameraComponentClass) { dbg::warn("CameraComponentClass is NULL - misc fov changer won't work"); }

        if (failures > 0) {
            dbg::error("SDK init has %d critical failure(s). GObjects/GNames offsets may be wrong.", failures);
            dbg::error("GObjects=0x%llX GNames=0x%llX", (unsigned long long)offsets::GObjects, (unsigned long long)offsets::GNames);
            return false;
        }

        init_complete = true;
        dbg::success("SDK::Initialize() complete - all critical objects found");
        return true;
    }
}

// Backwards compatibility aliases
using uobject = UObject;
using ucanvas = UCanvas;
using uworld = UWorld;
using uengine = UEngine;
using ugameviewportclient = UGameViewportClient;
using ulocalplayer = ULocalPlayer;
using ugameinstance = UGameInstance;
using mesh = USkeletalMeshComponent;
using actor = AActor;
using primal_character = APrimalCharacter;
using player_character = AShooterCharacter;
using dino_character = APrimalDinoCharacter;
using aplayercontroller = APlayerController;
using camera_manager = APlayerCameraManager;

namespace defines {
    inline auto& game_statics = sdk::GameStatics;
    inline auto& system = sdk::System;
    inline auto& string = sdk::String;
    inline auto& math = sdk::Math;
    inline auto& actor_primal_character_class = sdk::PrimalCharacterClass;
    inline auto& actor_dino_character_class = sdk::DinoCharacterClass;
    inline auto& actor_player_character_class = sdk::PlayerCharacterClass;
    inline auto& insert = sdk::InsertKey;
    inline auto& left_mouse_button = sdk::LeftMouseButton;
    inline auto& right_mouse_button = sdk::RightMouseButton;

    inline bool init() { return sdk::Initialize(); }
}
