#pragma once

/*
 * UE5 Material-Based Chams
 *
 * Finds EmissiveMeshMaterial from GObjects, creates dynamic instances per-mesh,
 * sets emissive color, then replaces mesh materials with the colored emissive.
 * Also enables custom depth stencil for potential through-wall rendering.
 */

#include <d3d12.h>
#include "../config/settings.h"
#include "../core/console.h"
#include "../sdk/sdk.h"

namespace features {
namespace chams {

    // Stored addresses — kept for DX12 hook compatibility (dx_hook references these)
    inline void* g_addr_draw_indexed = nullptr;
    inline void* g_addr_ia_set_vb    = nullptr;
    inline void* g_addr_set_pso      = nullptr;
    inline void* g_addr_create_pso   = nullptr;

    // Emissive base material found from GObjects
    inline UObject* g_emissive_material = nullptr;
    inline bool     g_mat_init          = false;
    inline bool     g_mat_ok            = false;

    // Track which meshes we've already applied to (persistent, not per-frame)
    inline std::uintptr_t g_applied[512] = {};
    inline int g_applied_count = 0;

    // For menu display
    inline int g_dynamic_count = 0;
    inline bool g_names_resolved = false;

    // Cached FName for parameter setting
    inline FName g_fname_emissive = {};
    inline bool  g_fname_resolved = false;

    // ── Extract vtable addresses (kept for dx_hook compatibility) ──
    inline bool get_vtable_addresses(ID3D12Device* dev,
        void** out_draw_indexed, void** out_ia_set_vb,
        void** out_set_pso, void** out_create_pso)
    {
        ID3D12CommandAllocator* alloc = nullptr;
        HRESULT hr = dev->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
        if (FAILED(hr) || !alloc) return false;

        ID3D12GraphicsCommandList* list = nullptr;
        hr = dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            alloc, nullptr, IID_PPV_ARGS(&list));
        if (FAILED(hr) || !list) {
            alloc->Release();
            return false;
        }

        void** list_vt = *reinterpret_cast<void***>(list);
        void** dev_vt  = *reinterpret_cast<void***>(dev);

        *out_draw_indexed = list_vt[13];
        *out_ia_set_vb    = list_vt[44];
        *out_set_pso      = list_vt[25];
        *out_create_pso   = dev_vt[10];

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[Chams] Vtable: DrawIndexed=%p IASetVB=%p SetPSO=%p CreatePSO=%p",
            *out_draw_indexed, *out_ia_set_vb, *out_set_pso, *out_create_pso);

        list->Release();
        alloc->Release();
        return true;
    }

    // ── Find emissive base material from GObjects ──
    inline void init_material() {
        if (g_mat_init) return;
        g_mat_init = true;

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] Searching for emissive material...");

        // MaterialInstanceConstant entries from GObjects dump
        const wchar_t* candidates[] = {
            L"Glow_Trans_Lit",
            L"Widget3DPassThrough_Opaque",
            L"GlowRing_MIC",
            L"EmissiveMeshMaterial",
        };

        for (auto* name : candidates) {
            __try {
                g_emissive_material = UObject::FindObject(name, nullptr);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                g_emissive_material = nullptr;
            }

            if (g_emissive_material) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[Chams] Found '%ls' at %p", name, g_emissive_material);
                g_mat_ok = true;
                return;
            }
        }

        dbg::log_ex(dbg::Level::Error, dbg::Init, "[Chams] No emissive material found.");
    }

    // ── Resolve FName for EmissiveColor parameter once ──
    inline void resolve_fnames() {
        if (g_fname_resolved) return;
        g_fname_resolved = true;

        if (sdk::String) {
            g_fname_emissive = sdk::String->StringToName(FString(L"EmissiveColor"));
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[Chams] FName 'EmissiveColor' = %u", g_fname_emissive.index);
        }
    }

    // ── Tracking helpers ──
    inline bool already_applied(std::uintptr_t addr) {
        for (int i = 0; i < g_applied_count; i++) {
            if (g_applied[i] == addr) return true;
        }
        return false;
    }

    inline void mark_applied(std::uintptr_t addr) {
        if (g_applied_count < 512) {
            g_applied[g_applied_count++] = addr;
        }
    }

    // No-op — kept for call-site compatibility in main.h
    inline void begin_frame() {}

    inline int g_apply_log_count = 0;

    // ── Apply chams: replace materials with colored emissive ──
    inline void apply(APrimalCharacter* character, const float* color, float intensity) {
        if (!character) return;

        // Init material + FNames on first call
        if (!g_mat_init) init_material();
        if (!g_mat_ok) return;
        if (!g_fname_resolved) resolve_fnames();

        USkeletalMeshComponent* mesh = nullptr;
        __try { mesh = character->GetMesh(); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!mesh) return;

        auto mesh_addr = reinterpret_cast<std::uintptr_t>(mesh);

        __try {
            if (!already_applied(mesh_addr)) {
                int num_mats = mesh->GetNumMaterials();

                if (g_apply_log_count < 5) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init,
                        "[Chams] mesh=%p numMats=%d", mesh, num_mats);
                }

                if (num_mats <= 0 || num_mats > 64) {
                    if (g_apply_log_count < 5) {
                        dbg::log_ex(dbg::Level::Warn, dbg::Init,
                            "[Chams] Skipping — invalid numMats=%d", num_mats);
                        g_apply_log_count++;
                    }
                    return;
                }

                FLinearColor emissive_color(
                    color[0] * intensity,
                    color[1] * intensity,
                    color[2] * intensity,
                    1.0f
                );

                // For each slot: SetMaterial → CreateDynamicMaterialInstance → set params DIRECTLY on MID
                for (int i = 0; i < num_mats; i++) {
                    // Replace with emissive material
                    mesh->SetMaterial(i, g_emissive_material);

                    // Create dynamic instance (MID) from the emissive material
                    UObject* mid = mesh->CreateDynamicMaterial(i);

                    if (g_apply_log_count < 3 && i == 0) {
                        dbg::log_ex(dbg::Level::Warn, dbg::Init,
                            "[Chams] slot=%d SetMaterial+CreateDynamic mid=%p", i, mid);
                    }

                    // Set color parameters DIRECTLY on the MID object
                    // EmissiveMeshMaterial has a parameter called "Color" (confirmed from UE5 source)
                    if (mid && sdk::String) {
                        const wchar_t* vec_names[] = {
                            L"Color", L"EmissiveColor", L"Emissive Color",
                            L"BaseColor", L"Base Color", L"TintColor",
                            L"GlowColor", L"Colour", L"Emissive",
                        };

                        for (auto* pname : vec_names) {
                            FName fn = sdk::String->StringToName(FString(pname));
                            MID_SetVectorParam(mid, fn, emissive_color);
                        }

                        const wchar_t* scalar_names[] = {
                            L"EmissiveScale", L"Intensity", L"EmissiveIntensity",
                            L"GlowIntensity", L"Opacity",
                        };

                        for (auto* sname : scalar_names) {
                            FName fn = sdk::String->StringToName(FString(sname));
                            MID_SetScalarParam(mid, fn, intensity);
                        }

                        if (g_apply_log_count < 3 && i == 0) {
                            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                                "[Chams] MID params set on mid=%p (direct API)", mid);
                        }
                    }
                }

                // Also try through mesh component (backup path)
                if (sdk::String) {
                    const wchar_t* mesh_vec[] = { L"Color", L"EmissiveColor", L"BaseColor" };
                    for (auto* pn : mesh_vec) {
                        FName fn = sdk::String->StringToName(FString(pn));
                        mesh->SetVectorParamOnMaterials(fn, emissive_color);
                    }
                }

                // Enable custom depth for through-wall visibility
                mesh->SetRenderCustomDepth(true);
                mesh->SetCustomDepthStencilValue(255);

                if (g_apply_log_count < 5) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init,
                        "[Chams] Applied to mesh=%p color=(%.1f,%.1f,%.1f)*%.1f",
                        mesh, color[0], color[1], color[2], intensity);
                    g_apply_log_count++;
                }

                mark_applied(mesh_addr);
                g_dynamic_count = g_applied_count;
                g_names_resolved = true;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (g_apply_log_count < 5) {
                dbg::log_ex(dbg::Level::Error, dbg::Init,
                    "[Chams] Exception in apply for mesh=%p", mesh);
                g_apply_log_count++;
            }
        }
    }

    // ── Remove chams from a character ──
    inline void clear(APrimalCharacter* character) {
        if (!character) return;

        USkeletalMeshComponent* mesh = nullptr;
        __try { mesh = character->GetMesh(); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!mesh) return;

        __try {
            mesh->SetRenderCustomDepth(false);
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    // ── Apply chams for a player character ──
    inline void apply_player(APrimalCharacter* character) {
        if (!config::chams::enabled || !config::chams::players) return;
        apply(character, config::chams::player_color, config::chams::intensity);
    }

    // ── Apply chams for a dino character ──
    inline void apply_dino(APrimalCharacter* character) {
        if (!config::chams::enabled || !config::chams::dinos) return;
        apply(character, config::chams::dino_color, config::chams::intensity);
    }

    // Stubs for compatibility
    inline void cleanup() {}

} // namespace chams
} // namespace features
