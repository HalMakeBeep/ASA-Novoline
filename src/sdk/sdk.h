#pragma once

#include <cstdint>
#include "../core/memory.h"
#include "../core/console.h"
#include "offsets.h"
#include "ue_types.h"
#include "math.h"

// fdec for game base
extern std::uintptr_t game;

class UObject {
public:
    FName GetName() {
        return read<FName>(std::uintptr_t(this) + offsets::UObject_Name);
    }

    static UObject* FindObject(const wchar_t* name, UObject* outer = nullptr) {
        return StaticFindObject(nullptr, outer, name, false);
    }

    static UObject* StaticFindObject(UObject* klass, UObject* outer, const wchar_t* name, bool exact = false) {
        if (offsets::StaticFindObject == 0) {
            dbg::error("StaticFindObject: offset is 0!");
            return nullptr;
        }

        auto func_addr = game + offsets::StaticFindObject;

        if (!memory::is_valid_ptr(func_addr)) {
            dbg::error("StaticFindObject: computed address 0x%llX is not readable (game=0x%llX + offset=0x%llX)",
                (unsigned long long)func_addr, (unsigned long long)game, (unsigned long long)offsets::StaticFindObject);
            return nullptr;
        }

        __try {
            return reinterpret_cast<UObject* (*)(UObject*, UObject*, const wchar_t*, bool)>
                (func_addr)(klass, outer, name, exact);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            dbg::error("StaticFindObject: EXCEPTION calling 0x%llX with name '%ls'",
                (unsigned long long)func_addr, name ? name : L"(null)");
            return nullptr;
        }
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"SkinnedMeshComponent.GetNumBones", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    FVector GetBoneLocation(std::int32_t index) {
        if (offsets::BoneMatrix == 0) return FVector();

        FMatrix matrix = {};
        reinterpret_cast<FMatrix* (*)(USkeletalMeshComponent*, FMatrix*, std::int32_t)>
            (game + offsets::BoneMatrix)(this, &matrix, index);

        return FVector(matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]);
    }
};


class AActor : public UObject {
public:
    FVector GetActorLocation() {
        struct { FVector ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Actor.K2_GetActorLocation", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    FRotator GetActorRotation() {
        struct { FRotator ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Actor.K2_GetActorRotation", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    bool WasRecentlyRendered(float tolerance = 0.1f) {
        struct { float tolerance; bool ret; } params = { tolerance };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Actor.WasRecentlyRendered", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    bool SetActorLocation(FVector location, bool sweep = false) {
        struct { FVector loc; bool sweep; uint8_t hit; bool teleport; bool ret; } params = { location, sweep, 0, true };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Actor.K2_SetActorLocation", false);
        if (func) ProcessEvent(func, &params);
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Actor.GetActorBounds", false);
        if (func) ProcessEvent(func, &params);
        if (origin) *origin = params.Origin;
        if (extent) *extent = params.BoxExtent;
    }

    UObject* GetComponentByClass(UObject* component_class) {
        struct {
            UObject* ComponentClass;
            UObject* ReturnValue;
        } params = { component_class, nullptr };

        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Actor.GetComponentByClass", false);
        if (!func) return nullptr;

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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PrimalCharacter.IsDead", false);
        if (func) {
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PrimalCharacter.IsAlliedWithOtherTeam", false);
        if (!func) return false;
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Pawn.GetHealth", false);
        if (func) {
            __try { ProcessEvent(func, &params); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
            return params.ret;
        }
        return 0.0f;
    }

    float GetMaxHealth() {
        struct { float ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Pawn.GetMaxHealth", false);
        if (func) {
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerController.IsInputKeyDown", false);
        if (func) ProcessEvent(func, &params);
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerController.ProjectWorldLocationToScreen", false);
        if (func) ProcessEvent(func, &params);
        *screen = params.ScreenLocation;
        return params.ReturnValue;
    }

    APrimalCharacter* GetPawn() {
        struct { APrimalCharacter* ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Controller.K2_GetPawn", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    void AddPitchInput(float val) {
        struct { float val; } params = { val };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerController.AddPitchInput", false);
        if (func) ProcessEvent(func, &params);
    }

    void AddYawInput(float val) {
        struct { float val; } params = { val };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerController.AddYawInput", false);
        if (func) ProcessEvent(func, &params);
    }

    void SetIgnoreLookInput(bool value) {
        struct { bool bNewLookInput; } params = { value };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Controller.SetIgnoreLookInput", false);
        if (!func) return;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void SetIgnoreMoveInput(bool value) {
        struct { bool bNewMoveInput; } params = { value };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Controller.SetIgnoreMoveInput", false);
        if (!func) return;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    void ResetIgnoreInputFlags() {
        struct {} params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Controller.ResetIgnoreInputFlags", false);
        if (!func) return;
        __try {
            ProcessEvent(func, &params);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }
    }

    FVector2D GetMousePosition() {
        struct { float x; float y; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerController.GetMousePosition", false);
        if (func) ProcessEvent(func, &params);
        return FVector2D(params.x, params.y);
    }
};

class APlayerCameraManager : public UObject {
public:
    FVector GetCameraLocation() {
        struct { FVector ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerCameraManager.GetCameraLocation", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    FRotator GetCameraRotation() {
        struct { FRotator ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerCameraManager.GetCameraRotation", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    float GetFOVAngle() {
        struct { float ret; } params = {};
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"PlayerCameraManager.GetFOVAngle", false);
        if (func) ProcessEvent(func, &params);
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Canvas.K2_TextSize", false);
        if (func) ProcessEvent(func, &params);
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Canvas.K2_DrawText", false);
        if (func) ProcessEvent(func, &params);
    }

    void K2_DrawLine(FVector2D a, FVector2D b, float thickness, FLinearColor color) {
        struct { FVector2D a; FVector2D b; float thickness; FLinearColor color; } params = { a, b, thickness, color };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"Canvas.K2_DrawLine", false);
        if (func) ProcessEvent(func, &params);
    }

    // Lowercase aliases for ZeroGUI compatibility
    void k2_draw_line(FVector2D a, FVector2D b, float thickness, FLinearColor color) {
        K2_DrawLine(a, b, thickness, color);
    }
    void k2_draw_text(UObject* font, FString text, FVector2D pos, FVector2D scale, FLinearColor color,
                      float kerning, FLinearColor shadow, FVector2D shadowOffset,
                      bool centerX, bool centerY, bool outlined, FLinearColor outlineColor) {
        K2_DrawText(font, text, pos, scale, color, kerning, shadow, shadowOffset, centerX, centerY, outlined, outlineColor);
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
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"GameplayStatics.GetAllActorsOfClass", false);
        if (func) ProcessEvent(func, &params);
        return params.actors;
    }

    UGameInstance* GetGameInstance(UObject* world) {
        struct { UObject* world; UGameInstance* ret; } params = { world };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"GameplayStatics.GetGameInstance", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    APlayerController* GetPlayerController(UObject* world, int index) {
        struct { UObject* world; int index; APlayerController* ret; } params = { world, index };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"GameplayStatics.GetPlayerController", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    APlayerCameraManager* GetPlayerCameraManager(UObject* world, int index) {
        struct { UObject* world; int index; APlayerCameraManager* ret; } params = { world, index };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"GameplayStatics.GetPlayerCameraManager", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    double GetWorldDeltaSeconds(UObject* world) {
        struct { UObject* world; double ret; } params = { world };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"GameplayStatics.GetWorldDeltaSeconds", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }
};

class SystemLibrary : public UObject {
public:
    FString GetObjectName(UObject* object) {
        struct { UObject* obj; FString ret; } params = { object };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetSystemLibrary.GetObjectName", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    UObject* GetOuterObject(UObject* object) {
        struct { UObject* obj; UObject* ret; } params = { object };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetSystemLibrary.GetOuterObject", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }
};

class StringLibrary : public UObject {
public:
    FName StringToName(FString str) {
        struct { FString str; FName ret; } params = { str };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetStringLibrary.Conv_StringToName", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    FString BuildStringDouble(FString append, FString prefix, double value, FString suffix) {
        struct { FString a; FString p; double v; FString s; FString ret; } params = { append, prefix, value, suffix };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetStringLibrary.BuildString_Double", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    bool Contains(FString searchIn, FString subString, bool useCase = false, bool searchFromEnd = false) {
        struct { FString a; FString b; bool c; bool d; bool ret; } params = { searchIn, subString, useCase, searchFromEnd };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetStringLibrary.Contains", false);
        if (func) ProcessEvent(func, &params);
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
    // Lowercase aliases for ZeroGUI compatibility
    double sin(double v) { return std::sin(v); }
    double cos(double v) { return std::cos(v); }

    FRotator FindLookAtRotation(FVector start, FVector target) {
        struct { FVector start; FVector target; FRotator ret; } params = { start, target };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetMathLibrary.FindLookAtRotation", false);
        if (func) ProcessEvent(func, &params);
        return params.ret;
    }

    FRotator RInterpTo(FRotator current, FRotator target, float deltaTime, float speed) {
        struct { FRotator c; FRotator t; float d; float s; FRotator ret; } params = { current, target, deltaTime, speed };
        static UObject* func = nullptr;
        if (!func) func = UObject::StaticFindObject(nullptr, nullptr, L"KismetMathLibrary.RInterpTo", false);
        if (func) ProcessEvent(func, &params);
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
        dbg::info("StaticFindObject address: 0x%llX (game=0x%llX + 0x%llX)",
            (unsigned long long)(game + offsets::StaticFindObject),
            (unsigned long long)game,
            (unsigned long long)offsets::StaticFindObject);

        dbg::log("[1/10] Finding GameplayStatics...");
        GameStatics = (kismet::GameplayStatics*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__GameplayStatics", false);
        dbg::dump_ptr("GameStatics", GameStatics);

        dbg::log("[2/10] Finding KismetSystemLibrary...");
        System = (kismet::SystemLibrary*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__KismetSystemLibrary", false);
        dbg::dump_ptr("System", System);

        dbg::log("[3/10] Finding KismetStringLibrary...");
        String = (kismet::StringLibrary*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__KismetStringLibrary", false);
        dbg::dump_ptr("String", String);

        dbg::log("[4/10] Finding KismetMathLibrary...");
        Math = (kismet::MathLibrary*)UObject::StaticFindObject(nullptr, nullptr, L"Engine.Default__KismetMathLibrary", false);
        dbg::dump_ptr("Math", Math);

        dbg::log("[5/10] Finding PrimalCharacter class...");
        PrimalCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.PrimalCharacter", false);
        if (!PrimalCharacterClass) {
            dbg::warn("ShooterGame.PrimalCharacter not found, trying fallback...");
            PrimalCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"PrimalCharacter", false);
        }
        dbg::dump_ptr("PrimalCharacterClass", PrimalCharacterClass);

        dbg::log("[6/10] Finding PrimalDinoCharacter class...");
        DinoCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.PrimalDinoCharacter", false);
        if (!DinoCharacterClass) {
            dbg::warn("ShooterGame.PrimalDinoCharacter not found, trying fallback...");
            DinoCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"PrimalDinoCharacter", false);
        }
        dbg::dump_ptr("DinoCharacterClass", DinoCharacterClass);

        dbg::log("[7/10] Finding ShooterCharacter class...");
        PlayerCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.ShooterCharacter", false);
        if (!PlayerCharacterClass) {
            dbg::warn("ShooterGame.ShooterCharacter not found, trying fallback...");
            PlayerCharacterClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterCharacter", false);
        }
        dbg::dump_ptr("PlayerCharacterClass", PlayerCharacterClass);

        dbg::log("[8/10] Finding DroppedItem and PrimalStructure...");
        DroppedItemClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.DroppedItem", false);
        StructureClass = UObject::StaticFindObject(nullptr, nullptr, L"ShooterGame.PrimalStructure", false);
        dbg::dump_ptr("DroppedItemClass", DroppedItemClass);
        dbg::dump_ptr("StructureClass", StructureClass);

        dbg::log("[9/10] Finding CameraComponent class...");
        CameraComponentClass = UObject::StaticFindObject(nullptr, nullptr, L"Engine.CameraComponent", false);
        if (!CameraComponentClass) {
            dbg::warn("Engine.CameraComponent not found, trying fallback...");
            CameraComponentClass = UObject::StaticFindObject(nullptr, nullptr, L"CameraComponent", false);
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
            dbg::error("SDK init has %d critical failure(s). StaticFindObject offset is likely WRONG.", failures);
            dbg::error("Current StaticFindObject offset: 0x%llX", (unsigned long long)offsets::StaticFindObject);
            dbg::error("You need to update offsets.h with values from a fresh SDK dump.");
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
