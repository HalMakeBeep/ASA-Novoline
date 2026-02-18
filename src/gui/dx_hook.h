#pragma once

// =============================================================
// DX12 Direct Swapchain Hook — v4 (detailed logging + no barriers)
// =============================================================

#include <d3d12.h>
#include <dxgi1_4.h>
#include <MinHook.h>

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"

#include "render.h"
#include "prisme_theme.h"
#include "imgui_menu.h"
#include "../core/console.h"

namespace dx_hook {

    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using ExecuteCommandListsFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    inline PresentFn              oPresent              = nullptr;
    inline ExecuteCommandListsFn  oExecuteCommandLists  = nullptr;

    inline bool   g_initialized   = false;
    inline bool   g_init_failed   = false;
    inline int    g_frame_count   = 0;
    inline bool   g_rendering     = false;
    inline bool   g_imgui_ready   = false;  // true after first successful ImGui frame
    inline bool   g_cmd_list_open = false;  // track command list state
    inline HWND   g_game_hwnd     = nullptr;

    inline ID3D12Device*                g_device        = nullptr;
    inline ID3D12CommandQueue*          g_command_queue = nullptr;
    inline ID3D12DescriptorHeap*        g_srv_heap      = nullptr;
    inline ID3D12DescriptorHeap*        g_rtv_heap      = nullptr;
    inline ID3D12CommandAllocator**     g_allocators    = nullptr;
    inline ID3D12GraphicsCommandList*   g_cmd_list      = nullptr;
    inline UINT                         g_buffer_count  = 0;
    inline DXGI_FORMAT                  g_rtv_format    = DXGI_FORMAT_R8G8B8A8_UNORM;

    inline ID3D12Fence*  g_fence         = nullptr;
    inline HANDLE        g_fence_event   = nullptr;
    inline UINT64*       g_fence_values  = nullptr;
    inline UINT64        g_fence_counter = 0;

    // ── ExecuteCommandLists hook ──
    inline void WINAPI hkExecuteCommandLists(
        ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists)
    {
        if (!g_command_queue && queue) {
            D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
            if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                g_command_queue = queue;
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] Captured DIRECT command queue: %p", queue);
            }
        }
        oExecuteCommandLists(queue, count, lists);
    }

    // ── Manual ImGui input ──
    inline void feed_imgui_input() {
        ImGuiIO& io = ImGui::GetIO();

        RECT r;
        if (GetClientRect(g_game_hwnd, &r)) {
            io.DisplaySize = ImVec2((float)(r.right - r.left), (float)(r.bottom - r.top));
        }

        POINT cursor;
        if (GetCursorPos(&cursor) && ScreenToClient(g_game_hwnd, &cursor)) {
            io.MousePos = ImVec2((float)cursor.x, (float)cursor.y);
        }

        io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

        static LARGE_INTEGER freq = {};
        static LARGE_INTEGER last = {};
        if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        io.DeltaTime = last.QuadPart > 0
            ? (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart
            : 1.0f / 60.0f;
        last = now;
        if (io.DeltaTime <= 0.0f) io.DeltaTime = 1.0f / 60.0f;
    }

    // ── Fence helpers ──
    inline void wait_for_fence(UINT buffer_idx) {
        if (!g_fence || !g_fence_values) return;
        if (buffer_idx >= g_buffer_count) return;
        UINT64 completed = g_fence->GetCompletedValue();
        if (completed < g_fence_values[buffer_idx]) {
            g_fence->SetEventOnCompletion(g_fence_values[buffer_idx], g_fence_event);
            WaitForSingleObject(g_fence_event, 5000);
        }
    }

    inline void signal_fence(UINT buffer_idx) {
        if (!g_fence || !g_fence_values || !g_command_queue) return;
        if (buffer_idx >= g_buffer_count) return;
        g_fence_counter++;
        g_fence_values[buffer_idx] = g_fence_counter;
        g_command_queue->Signal(g_fence, g_fence_counter);
    }

    // ── Initialize ──
    inline bool init_imgui(IDXGISwapChain* swap) {
        DXGI_SWAP_CHAIN_DESC desc;
        if (FAILED(swap->GetDesc(&desc))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetDesc failed");
            return false;
        }

        g_game_hwnd    = desc.OutputWindow;
        g_buffer_count = desc.BufferCount;
        g_rtv_format   = desc.BufferDesc.Format;

        if (FAILED(swap->GetDevice(IID_PPV_ARGS(&g_device)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetDevice failed");
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Device=%p HWND=%p Buffers=%u Format=%u",
            g_device, g_game_hwnd, g_buffer_count, g_rtv_format);

        // SRV heap (ImGui fonts)
        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            d.NumDescriptors = 1;
            d.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_srv_heap)))) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] SRV heap failed");
                return false;
            }
        }

        // RTV heap (1 slot, refreshed each frame)
        {
            D3D12_DESCRIPTOR_HEAP_DESC d = {};
            d.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            d.NumDescriptors = 1;
            if (FAILED(g_device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&g_rtv_heap)))) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] RTV heap failed");
                return false;
            }
        }

        // Command allocators
        g_allocators = new ID3D12CommandAllocator*[g_buffer_count]();
        for (UINT i = 0; i < g_buffer_count; i++) {
            if (FAILED(g_device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_allocators[i])))) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Allocator %u failed", i);
                return false;
            }
        }

        // Command list — create CLOSED via CreateCommandList1 if available, else open+close
        // Use CreateCommandList (creates OPEN) then Close
        if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                g_allocators[0], nullptr, IID_PPV_ARGS(&g_cmd_list)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] CommandList failed");
            return false;
        }
        g_cmd_list->Close();
        g_cmd_list_open = false;

        // Fence
        if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] CreateFence failed");
            return false;
        }
        g_fence_event  = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g_fence_values = new UINT64[g_buffer_count]();
        g_fence_counter = 0;

        // ImGui
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags    |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename     = nullptr;
        io.MouseDrawCursor = true;

        prisme_theme::apply();

        auto srv_cpu = g_srv_heap->GetCPUDescriptorHandleForHeapStart();
        auto srv_gpu = g_srv_heap->GetGPUDescriptorHandleForHeapStart();

        if (!ImGui_ImplDX12_Init(g_device, (int)g_buffer_count,
                g_rtv_format, g_srv_heap, srv_cpu, srv_gpu)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] ImGui_ImplDX12_Init failed");
            return false;
        }

        g_initialized = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Init complete, waiting for F1");
        return true;
    }

    // ── Render one frame ──
    inline void render_frame(IDXGISwapChain* swap) {
        if (!g_command_queue) return;
        if (!render::show_menu) return;

        // Log first render attempt
        static bool first_render_logged = false;
        if (!first_render_logged) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] First render_frame (menu open)");
            first_render_logged = true;
        }

        // Device health
        HRESULT reason = g_device->GetDeviceRemovedReason();
        if (FAILED(reason)) {
            static bool logged = false;
            if (!logged) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Device removed: 0x%08X", (unsigned)reason);
                logged = true;
            }
            return;
        }

        // Get current backbuffer index
        UINT idx = 0;
        {
            IDXGISwapChain3* swap3 = nullptr;
            if (SUCCEEDED(swap->QueryInterface(IID_PPV_ARGS(&swap3)))) {
                idx = swap3->GetCurrentBackBufferIndex();
                swap3->Release();
            }
        }

        // Get backbuffer FRESH (survives ResizeBuffers)
        ID3D12Resource* backbuffer = nullptr;
        HRESULT hr = swap->GetBuffer(idx, IID_PPV_ARGS(&backbuffer));
        if (FAILED(hr) || !backbuffer) {
            static bool logged = false;
            if (!logged) { dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetBuffer(%u) failed: 0x%08X", idx, (unsigned)hr); logged = true; }
            return;
        }

        // Create RTV
        auto rtv_handle = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        g_device->CreateRenderTargetView(backbuffer, nullptr, rtv_handle);

        // Wait for GPU
        wait_for_fence(idx);

        // If command list is stuck open from a previous crash, close it first
        if (g_cmd_list_open) {
            g_cmd_list->Close();
            g_cmd_list_open = false;
        }

        // Reset allocator
        hr = g_allocators[idx]->Reset();
        if (FAILED(hr)) {
            static bool logged = false;
            if (!logged) { dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Allocator Reset failed: 0x%08X", (unsigned)hr); logged = true; }
            backbuffer->Release();
            return;
        }

        // Reset command list
        hr = g_cmd_list->Reset(g_allocators[idx], nullptr);
        if (FAILED(hr)) {
            static bool logged = false;
            if (!logged) { dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] CmdList Reset failed: 0x%08X", (unsigned)hr); logged = true; }
            backbuffer->Release();
            return;
        }
        g_cmd_list_open = true;

        // Feed input
        feed_imgui_input();
        ImGui::GetIO().MouseDrawCursor = true;

        // NO resource barriers — let DX12 handle implicit state promotion
        // (UE5 DX12 may use enhanced barriers or leave buffer in RENDER_TARGET)
        g_cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
        ID3D12DescriptorHeap* heaps[] = { g_srv_heap };
        g_cmd_list->SetDescriptorHeaps(1, heaps);

        // ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui::NewFrame();
        imgui_menu::draw();
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmd_list);

        // Close and execute
        g_cmd_list->Close();
        g_cmd_list_open = false;

        ID3D12CommandList* lists[] = { g_cmd_list };
        oExecuteCommandLists(g_command_queue, 1, lists);

        signal_fence(idx);

        // Log first successful render
        if (!g_imgui_ready) {
            g_imgui_ready = true;
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] First ImGui frame rendered OK!");
        }

        backbuffer->Release();
    }

    // ── Present hook ──
    inline HRESULT WINAPI hkPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
        if (g_rendering) return oPresent(swap, sync, flags);
        g_rendering = true;

        if (++g_frame_count < 100) {
            if (g_frame_count == 1)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Present hook ALIVE");
            if (g_frame_count == 50)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Warmup 50/100...");
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        if (g_init_failed) {
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        __try {
            if (!g_initialized) {
                if (!init_imgui(swap)) {
                    g_init_failed = true;
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] init_imgui FAILED");
                }
            }

            if (g_initialized) {
                render_frame(swap);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            static int ex = 0;
            if (++ex <= 10) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] Exception #%d in Present (show_menu=%d, cmd_open=%d)",
                    ex, (int)render::show_menu, (int)g_cmd_list_open);
            }
            // Try to close the command list if it was left open
            if (g_cmd_list_open) {
                __try { g_cmd_list->Close(); } __except(EXCEPTION_EXECUTE_HANDLER) {}
                g_cmd_list_open = false;
            }
            if (ex >= 10) g_init_failed = true;
        }

        g_rendering = false;
        return oPresent(swap, sync, flags);
    }

    // ── Get vtable addresses ──
    inline bool get_hook_addresses(void** present_out, void** execute_out) {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Creating dummy device for vtable...");

        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0,
                           GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                           L"DXGIDummy12", nullptr };
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                  0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        ID3D12Device* dev = nullptr;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] D3D12CreateDevice failed");
            DestroyWindow(hwnd);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ID3D12CommandQueue* queue = nullptr;
        dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));

        IDXGIFactory4* factory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));

        if (!queue || !factory) {
            if (queue) queue->Release();
            if (factory) factory->Release();
            dev->Release();
            DestroyWindow(hwnd);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.BufferCount      = 2;
        sd.Width            = 100;
        sd.Height           = 100;
        sd.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;

        IDXGISwapChain1* swap = nullptr;
        factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swap);

        if (!swap) {
            factory->Release();
            queue->Release();
            dev->Release();
            DestroyWindow(hwnd);
            UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        void** swap_vtable  = *reinterpret_cast<void***>(swap);
        void** queue_vtable = *reinterpret_cast<void***>(queue);

        *present_out = swap_vtable[8];
        *execute_out = queue_vtable[10];

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Present=%p ExecuteCommandLists=%p", *present_out, *execute_out);

        swap->Release();
        factory->Release();
        queue->Release();
        dev->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return true;
    }

    // ── Entry point ──
    inline bool initialize() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] === Initialize (Direct Swapchain v4) ===");

        auto mh = MH_Initialize();
        if (mh != MH_OK && mh != MH_ERROR_ALREADY_INITIALIZED) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] MH_Initialize failed: %d", mh);
            return false;
        }

        void* present_addr = nullptr;
        void* execute_addr = nullptr;
        if (!get_hook_addresses(&present_addr, &execute_addr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Failed to get hook addresses");
            return false;
        }

        if (MH_CreateHook(present_addr, &hkPresent,
                reinterpret_cast<void**>(&oPresent)) != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hook Present failed");
            return false;
        }

        if (MH_CreateHook(execute_addr, &hkExecuteCommandLists,
                reinterpret_cast<void**>(&oExecuteCommandLists)) != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hook ExecuteCommandLists failed");
            return false;
        }

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Hooks enabled (Present + ExecuteCommandLists)");
        return true;
    }

} // namespace dx_hook
