#pragma once

/*
 * DX12 Chams — True wallhack via draw call hooking.
 *
 * Hooks are CREATED at startup but NOT ENABLED until the user turns chams on.
 * This avoids intercepting tens of thousands of draw calls when chams is off.
 *
 * When enabled:
 * - CreateGraphicsPipelineState: creates depth-disabled PSO variants
 * - DrawIndexedInstanced: redraws matching meshes with depth=ALWAYS
 * - IASetVertexBuffers / SetPipelineState: tracks current stride + PSO
 */

#include <d3d12.h>
#include <MinHook.h>
#include <unordered_map>
#include <mutex>
#include "../config/settings.h"
#include "../core/console.h"

namespace features {
namespace chams {

    // ── Function pointer types for DX12 hooks ──
    using FnDrawIndexedInstanced = void(STDMETHODCALLTYPE*)(
        ID3D12GraphicsCommandList*, UINT, UINT, UINT, INT, UINT);

    using FnIASetVertexBuffers = void(STDMETHODCALLTYPE*)(
        ID3D12GraphicsCommandList*, UINT, UINT, const D3D12_VERTEX_BUFFER_VIEW*);

    using FnSetPipelineState = void(STDMETHODCALLTYPE*)(
        ID3D12GraphicsCommandList*, ID3D12PipelineState*);

    using FnCreateGfxPSO = HRESULT(STDMETHODCALLTYPE*)(
        ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);

    // ── Original function pointers (filled by MinHook) ──
    inline FnDrawIndexedInstanced oDrawIndexedInstanced = nullptr;
    inline FnIASetVertexBuffers   oIASetVertexBuffers   = nullptr;
    inline FnSetPipelineState     oSetPipelineState     = nullptr;
    inline FnCreateGfxPSO         oCreateGfxPSO         = nullptr;

    // ── Hook state ──
    inline bool g_hooks_created = false;  // MH_CreateHook done (but NOT enabled)
    inline bool g_hooks_active  = false;  // MH_EnableHook done (actively intercepting)

    // Stored addresses for deferred enable/disable
    inline void* g_addr_draw_indexed = nullptr;
    inline void* g_addr_ia_set_vb    = nullptr;
    inline void* g_addr_set_pso      = nullptr;
    inline void* g_addr_create_pso   = nullptr;

    // ── Per-draw state tracking ──
    inline UINT                  g_stride      = 0;
    inline ID3D12PipelineState*  g_current_pso = nullptr;

    // Our own command list pointer — skip chams on our ImGui draws
    inline ID3D12GraphicsCommandList* g_our_cmdlist = nullptr;

    // ── Chams PSO cache: original PSO → depth-disabled variant ──
    inline std::unordered_map<ID3D12PipelineState*, ID3D12PipelineState*> g_chams_map;
    inline std::mutex g_mutex;
    inline int g_chams_pso_count = 0;

    // ── Stride logging (debug) ──
    inline std::unordered_map<UINT, int> g_stride_counts;
    inline int g_frame_counter = 0;

    // ========================================================================
    // Hook: IASetVertexBuffers — track the stride of vertex buffer slot 0
    // ========================================================================
    inline void STDMETHODCALLTYPE hkIASetVertexBuffers(
        ID3D12GraphicsCommandList* list, UINT slot, UINT count,
        const D3D12_VERTEX_BUFFER_VIEW* views)
    {
        if (slot == 0 && count > 0 && views)
            g_stride = views[0].StrideInBytes;

        oIASetVertexBuffers(list, slot, count, views);
    }

    // ========================================================================
    // Hook: SetPipelineState — track the currently bound PSO
    // ========================================================================
    inline void STDMETHODCALLTYPE hkSetPipelineState(
        ID3D12GraphicsCommandList* list, ID3D12PipelineState* pso)
    {
        g_current_pso = pso;
        oSetPipelineState(list, pso);
    }

    // ========================================================================
    // Hook: DrawIndexedInstanced — the main chams logic
    // ========================================================================
    inline void STDMETHODCALLTYPE hkDrawIndexedInstanced(
        ID3D12GraphicsCommandList* list, UINT idx_count, UINT inst_count,
        UINT start_idx, INT base_vtx, UINT start_inst)
    {
        // Always do the normal draw first
        oDrawIndexedInstanced(list, idx_count, inst_count, start_idx, base_vtx, start_inst);

        // Skip our own ImGui command list
        if (list == g_our_cmdlist) return;
        if (!g_current_pso) return;

        // Stride logging mode — collect stride statistics
        if (config::chams::log_strides) {
            g_stride_counts[g_stride]++;
        }

        if (!config::chams::enabled) return;

        // Stride matching
        bool match = false;
        if (config::chams::target_stride > 0) {
            match = (g_stride == (UINT)config::chams::target_stride);
        } else {
            match = (g_stride == 32 || g_stride == 40 || g_stride == 44 || g_stride == 48);
        }
        if (!match) return;

        // Index count filter
        if (idx_count < (UINT)config::chams::min_indices ||
            idx_count > (UINT)config::chams::max_indices) return;

        // Look up the chams PSO
        ID3D12PipelineState* chams_pso = nullptr;
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            auto it = g_chams_map.find(g_current_pso);
            if (it != g_chams_map.end()) chams_pso = it->second;
        }
        if (!chams_pso) return;

        // Second draw pass: depth=ALWAYS → visible through walls
        oSetPipelineState(list, chams_pso);
        oDrawIndexedInstanced(list, idx_count, inst_count, start_idx, base_vtx, start_inst);
        oSetPipelineState(list, g_current_pso); // restore
    }

    // ========================================================================
    // Hook: CreateGraphicsPipelineState — create depth-disabled PSO variants
    // ========================================================================
    inline HRESULT STDMETHODCALLTYPE hkCreateGfxPSO(
        ID3D12Device* dev, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
        REFIID riid, void** out)
    {
        // Create the original PSO
        HRESULT hr = oCreateGfxPSO(dev, desc, riid, out);
        if (FAILED(hr) || !out || !*out) return hr;

        // Skip PSOs without depth buffer (UI, fullscreen quads, etc.)
        if (!desc || desc->DSVFormat == DXGI_FORMAT_UNKNOWN) return hr;

        // Create depth-disabled chams variant
        __try {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC chams_desc = *desc;
            chams_desc.DepthStencilState.DepthEnable    = TRUE;
            chams_desc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_ALWAYS;
            chams_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

            ID3D12PipelineState* chams_pso = nullptr;
            HRESULT hr2 = oCreateGfxPSO(dev, &chams_desc, IID_PPV_ARGS(&chams_pso));
            if (SUCCEEDED(hr2) && chams_pso) {
                g_mutex.lock();
                g_chams_map[static_cast<ID3D12PipelineState*>(*out)] = chams_pso;
                g_chams_pso_count++;
                g_mutex.unlock();
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            // Silently skip — some PSO variants may fail
        }

        return hr;
    }

    // ========================================================================
    // Dump stride statistics (called periodically from render_frame)
    // ========================================================================
    inline void dump_stride_stats() {
        if (!g_hooks_active) return;
        if (!config::chams::log_strides) return;

        g_frame_counter++;
        if (g_frame_counter % 300 != 0) return;

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[Chams] Stride stats (chams PSOs: %d):", g_chams_pso_count);

        int shown = 0;
        for (auto& [stride, count] : g_stride_counts) {
            if (shown >= 15) break;
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "  stride=%u  draws=%d", stride, count);
            shown++;
        }
        g_stride_counts.clear();
    }

    // ========================================================================
    // Extract vtable addresses from a dummy command list + device
    // ========================================================================
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

        *out_draw_indexed = list_vt[13];  // DrawIndexedInstanced
        *out_ia_set_vb    = list_vt[44];  // IASetVertexBuffers
        *out_set_pso      = list_vt[25];  // SetPipelineState
        *out_create_pso   = dev_vt[10];   // CreateGraphicsPipelineState

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[Chams] Vtable: DrawIndexed=%p IASetVB=%p SetPSO=%p CreatePSO=%p",
            *out_draw_indexed, *out_ia_set_vb, *out_set_pso, *out_create_pso);

        list->Release();
        alloc->Release();
        return true;
    }

    // ========================================================================
    // Create + enable chams hooks on demand (called from menu toggle)
    // Hooks are NOT created at startup — only when user activates chams.
    // ========================================================================
    inline bool enable_hooks() {
        if (g_hooks_active) return true;

        // Need vtable addresses
        if (!g_addr_draw_indexed || !g_addr_ia_set_vb ||
            !g_addr_set_pso || !g_addr_create_pso) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] No vtable addresses — cannot create hooks");
            return false;
        }

        // Create hooks if not yet created
        if (!g_hooks_created) {
            bool ok = true;
            if (MH_CreateHook(g_addr_draw_indexed, &hkDrawIndexedInstanced,
                reinterpret_cast<void**>(&oDrawIndexedInstanced)) != MH_OK) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] CreateHook DrawIndexedInstanced failed");
                ok = false;
            }
            if (MH_CreateHook(g_addr_ia_set_vb, &hkIASetVertexBuffers,
                reinterpret_cast<void**>(&oIASetVertexBuffers)) != MH_OK) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] CreateHook IASetVertexBuffers failed");
                ok = false;
            }
            if (MH_CreateHook(g_addr_set_pso, &hkSetPipelineState,
                reinterpret_cast<void**>(&oSetPipelineState)) != MH_OK) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] CreateHook SetPipelineState failed");
                ok = false;
            }
            if (MH_CreateHook(g_addr_create_pso, &hkCreateGfxPSO,
                reinterpret_cast<void**>(&oCreateGfxPSO)) != MH_OK) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] CreateHook CreateGfxPSO failed");
                ok = false;
            }
            if (!ok) return false;
            g_hooks_created = true;
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] All 4 hooks created");
        }

        // Enable
        bool ok = true;
        if (MH_EnableHook(g_addr_draw_indexed) != MH_OK) ok = false;
        if (MH_EnableHook(g_addr_ia_set_vb)    != MH_OK) ok = false;
        if (MH_EnableHook(g_addr_set_pso)       != MH_OK) ok = false;
        if (MH_EnableHook(g_addr_create_pso)    != MH_OK) ok = false;

        if (ok) {
            g_hooks_active = true;
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] Hooks ENABLED — capturing PSOs");
        } else {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] Failed to enable some hooks");
        }
        return ok;
    }

    inline void disable_hooks() {
        if (!g_hooks_active) return;

        MH_DisableHook(g_addr_draw_indexed);
        MH_DisableHook(g_addr_ia_set_vb);
        MH_DisableHook(g_addr_set_pso);
        MH_DisableHook(g_addr_create_pso);

        g_hooks_active = false;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Chams] Hooks DISABLED");
    }

    // ========================================================================
    // Cleanup — release all chams PSOs
    // ========================================================================
    inline void cleanup() {
        disable_hooks();
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto& [k, v] : g_chams_map) {
            if (v) v->Release();
        }
        g_chams_map.clear();
        g_chams_pso_count = 0;
    }

} // namespace chams
} // namespace features
