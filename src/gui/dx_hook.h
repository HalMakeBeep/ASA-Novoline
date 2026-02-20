#pragma once

// =============================================================
// Direct DX12 Hook — renders ImGui natively on the game's DX12 swapchain.
// Uses our own command allocators + command list. No D3D11On12 layer.
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

    inline PresentFn             oPresent             = nullptr;
    inline ExecuteCommandListsFn oExecuteCommandLists = nullptr;

    inline bool g_initialized    = false;
    inline bool g_init_failed    = false;
    inline bool g_imgui_inited   = false;  // tracks if ImGui context + backend already created
    inline int  g_frame_count  = 0;
    inline bool g_rendering    = false;
    inline HWND g_game_hwnd    = nullptr;
    inline UINT g_buffer_count = 0;

    // Game DX12 objects (captured)
    inline ID3D12Device*       g_d3d12_device  = nullptr;
    inline ID3D12CommandQueue* g_command_queue  = nullptr;

    // Our own DX12 rendering objects
    inline ID3D12DescriptorHeap*      g_rtv_heap   = nullptr;
    inline ID3D12DescriptorHeap*      g_srv_heap   = nullptr;
    inline ID3D12CommandAllocator**    g_cmd_allocs = nullptr;
    inline ID3D12GraphicsCommandList* g_cmd_list   = nullptr;

    // GPU fence for per-frame sync
    inline ID3D12Fence* g_fence       = nullptr;
    inline HANDLE       g_fence_event = nullptr;
    inline UINT64       g_fence_value = 0;
    inline UINT64*      g_fence_values = nullptr; // per-buffer

    inline DXGI_FORMAT  g_rtv_format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // ── ExecuteCommandLists hook — tracks game's DIRECT command queue ──
    // Always update to the MOST RECENT direct queue, not just the first one.
    // UE5 may create the actual rendering queue after the initial setup queue.
    inline void WINAPI hkExecuteCommandLists(
        ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists)
    {
        if (queue) {
            D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
            if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                if (g_command_queue != queue) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init,
                        "[DX12] Tracking DIRECT command queue: %p", queue);
                }
                g_command_queue = queue;
            }
        }
        oExecuteCommandLists(queue, count, lists);
    }

    // ── Virtual mouse state ──
    inline float g_vmouse_x = 0, g_vmouse_y = 0;
    inline bool  g_vmouse_active = false;
    inline POINT g_last_raw_cursor = {};

    // ── Manual ImGui input with virtual mouse (no WndProc hook) ──
    inline void feed_imgui_input() {
        ImGuiIO& io = ImGui::GetIO();

        RECT r;
        if (GetClientRect(g_game_hwnd, &r))
            io.DisplaySize = ImVec2((float)(r.right - r.left), (float)(r.bottom - r.top));

        float sw = io.DisplaySize.x;
        float sh = io.DisplaySize.y;

        if (!g_vmouse_active) {
            g_vmouse_x = sw / 2.0f;
            g_vmouse_y = sh / 2.0f;
            GetCursorPos(&g_last_raw_cursor);
            g_vmouse_active = true;
        }

        ClipCursor(nullptr);

        POINT raw_cursor;
        if (GetCursorPos(&raw_cursor)) {
            float dx = (float)(raw_cursor.x - g_last_raw_cursor.x);
            float dy = (float)(raw_cursor.y - g_last_raw_cursor.y);

            g_vmouse_x += dx;
            g_vmouse_y += dy;

            if (g_vmouse_x < 0.0f) g_vmouse_x = 0.0f;
            if (g_vmouse_y < 0.0f) g_vmouse_y = 0.0f;
            if (g_vmouse_x > sw) g_vmouse_x = sw;
            if (g_vmouse_y > sh) g_vmouse_y = sh;

            POINT center = { (r.right - r.left) / 2, (r.bottom - r.top) / 2 };
            ClientToScreen(g_game_hwnd, &center);
            SetCursorPos(center.x, center.y);
            g_last_raw_cursor = center;
        }

        io.MousePos = ImVec2(g_vmouse_x, g_vmouse_y);
        io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;

        static LARGE_INTEGER freq = {}, last = {};
        if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        io.DeltaTime = last.QuadPart > 0
            ? (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart
            : 1.0f / 60.0f;
        last = now;
        if (io.DeltaTime <= 0.0f || io.DeltaTime > 0.05f) io.DeltaTime = 1.0f / 60.0f;
    }

    // ── Wait for a specific fence value (with timeout) ──
    inline bool wait_for_fence(UINT64 value, DWORD ms = 5000) {
        if (g_fence->GetCompletedValue() < value) {
            g_fence->SetEventOnCompletion(value, g_fence_event);
            DWORD res = WaitForSingleObject(g_fence_event, ms);
            if (res == WAIT_TIMEOUT) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] Fence wait timeout (%ums) — GPU may be hung", ms);
                return false;
            }
        }
        return true;
    }

    // ── Initialize direct DX12 rendering ──
    inline bool init_dx12(IDXGISwapChain* swap) {
        DXGI_SWAP_CHAIN_DESC sc_desc;
        if (FAILED(swap->GetDesc(&sc_desc))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetDesc failed");
            return false;
        }
        g_game_hwnd    = sc_desc.OutputWindow;
        g_buffer_count = sc_desc.BufferCount;
        g_rtv_format   = sc_desc.BufferDesc.Format;

        if (FAILED(swap->GetDevice(IID_PPV_ARGS(&g_d3d12_device)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetDevice failed");
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Device=%p HWND=%p Buffers=%u Format=%u",
            g_d3d12_device, g_game_hwnd, g_buffer_count, g_rtv_format);

        HRESULT hr;

        // Create RTV descriptor heap (1 slot — we create a fresh RTV each frame)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = g_d3d12_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_rtv_heap));
            if (FAILED(hr)) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] RTV heap failed: 0x%08X", (unsigned)hr);
                return false;
            }
        }

        // Create SRV descriptor heap (for ImGui font texture)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = g_d3d12_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_srv_heap));
            if (FAILED(hr)) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] SRV heap failed: 0x%08X", (unsigned)hr);
                return false;
            }
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] RTV + SRV heaps created");

        // Create command allocators (one per backbuffer for proper GPU sync)
        g_cmd_allocs = new ID3D12CommandAllocator*[g_buffer_count]();
        for (UINT i = 0; i < g_buffer_count; i++) {
            hr = g_d3d12_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_cmd_allocs[i]));
            if (FAILED(hr)) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] CmdAlloc(%u) failed: 0x%08X", i, (unsigned)hr);
                return false;
            }
        }

        // Create command list (initially closed)
        hr = g_d3d12_device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_cmd_allocs[0], nullptr, IID_PPV_ARGS(&g_cmd_list));
        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] CmdList failed: 0x%08X", (unsigned)hr);
            return false;
        }
        g_cmd_list->Close(); // start closed — we Reset it each frame

        // Create fence
        g_fence_values = new UINT64[g_buffer_count]();
        hr = g_d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Fence failed: 0x%08X", (unsigned)hr);
            return false;
        }
        g_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Fence + command objects created");

        // Init ImGui — only once (guard against re-entry after exception)
        if (!g_imgui_inited) {
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags    |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename     = nullptr;
            io.MouseDrawCursor = true;

            // DPI scale
            float dpi_scale = 1.0f;
            if (sc_desc.BufferDesc.Height >= 2160) dpi_scale = 1.5f;
            else if (sc_desc.BufferDesc.Height >= 1440) dpi_scale = 1.25f;

            // Load fonts
            ImFontConfig font_cfg;
            font_cfg.OversampleH       = 3;
            font_cfg.OversampleV       = 2;
            font_cfg.PixelSnapH        = false;
            font_cfg.RasterizerDensity = dpi_scale;
            float font_size = 15.0f * dpi_scale;

            ImFont* font_main = nullptr;
            const char* font_paths[] = {
                "C:\\Windows\\Fonts\\segoeui.ttf",
                "C:\\Windows\\Fonts\\calibri.ttf",
                "C:\\Windows\\Fonts\\arial.ttf",
            };
            for (auto* path : font_paths) {
                font_main = io.Fonts->AddFontFromFileTTF(path, font_size, &font_cfg);
                if (font_main) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Font: %s @ %.0fpx (dpi=%.2f)", path, font_size, dpi_scale);
                    break;
                }
            }

            ImFontConfig title_cfg = font_cfg;
            title_cfg.MergeMode = false;
            float title_size = 20.0f * dpi_scale;

            ImFont* font_title = nullptr;
            const char* bold_paths[] = {
                "C:\\Windows\\Fonts\\seguisb.ttf",
                "C:\\Windows\\Fonts\\segoeuib.ttf",
                "C:\\Windows\\Fonts\\arialbd.ttf",
            };
            for (auto* path : bold_paths) {
                font_title = io.Fonts->AddFontFromFileTTF(path, title_size, &title_cfg);
                if (font_title) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Title font: %s", path);
                    break;
                }
            }

            imgui_menu::g_font_main  = font_main;
            imgui_menu::g_font_title = font_title;
            imgui_menu::g_dpi_scale  = dpi_scale;

            prisme_theme::apply();

            // Init ImGui DX12 backend — use new API to pass CommandQueue
            // (legacy API doesn't set CommandQueue, causing null crash on font upload)
            ImGui_ImplDX12_InitInfo init_info;
            init_info.Device             = g_d3d12_device;
            init_info.CommandQueue       = g_command_queue;
            init_info.NumFramesInFlight  = (int)g_buffer_count;
            init_info.RTVFormat          = g_rtv_format;
            init_info.SrvDescriptorHeap  = g_srv_heap;
            init_info.LegacySingleSrvCpuDescriptor = g_srv_heap->GetCPUDescriptorHandleForHeapStart();
            init_info.LegacySingleSrvGpuDescriptor = g_srv_heap->GetGPUDescriptorHandleForHeapStart();
            ImGui_ImplDX12_Init(&init_info);

            // Force font texture upload NOW (during init, not lazily on first frame).
            // This prevents the internal ExecuteCommandLists + fence wait from
            // happening in the middle of our render_frame, which can conflict
            // with the game's pipeline.
            ImGui_ImplDX12_CreateDeviceObjects();

            g_imgui_inited = true;
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] ImGui initialized (fonts uploaded)");
        }

        g_initialized = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Init complete, waiting for F1");
        return true;
    }

    // ── Render one frame ──
    inline int g_render_count = 0;

    inline void render_frame(IDXGISwapChain* swap) {
        if (!g_command_queue) return;
        if (!render::show_menu) {
            g_vmouse_active = false;
            return;
        }

        g_render_count++;

        // Get current backbuffer index
        UINT idx = 0;
        {
            IDXGISwapChain3* swap3 = nullptr;
            if (SUCCEEDED(swap->QueryInterface(IID_PPV_ARGS(&swap3)))) {
                idx = swap3->GetCurrentBackBufferIndex();
                swap3->Release();
            }
        }
        if (idx >= g_buffer_count) idx = 0;

        // Wait for this buffer's previous frame to finish on GPU
        if (!wait_for_fence(g_fence_values[idx])) {
            g_init_failed = true;
            return;
        }

        // ── Get FRESH backbuffer each frame ──
        // Previously we cached GetBuffer() during init, but the game calls
        // ResizeBuffers between init and first F1 press (loading→gameplay),
        // which invalidates those cached pointers → DEVICE_HUNG crash.
        ID3D12Resource* backbuffer = nullptr;
        HRESULT hr = swap->GetBuffer(idx, IID_PPV_ARGS(&backbuffer));
        if (FAILED(hr) || !backbuffer) {
            if (g_render_count <= 3)
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] GetBuffer(%u) failed: 0x%08X", idx, (unsigned)hr);
            return;
        }

        // Create fresh RTV descriptor for this backbuffer
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtv_heap->GetCPUDescriptorHandleForHeapStart();
        g_d3d12_device->CreateRenderTargetView(backbuffer, nullptr, rtv);

        // Reset command allocator + list for this frame
        hr = g_cmd_allocs[idx]->Reset();
        if (FAILED(hr)) {
            backbuffer->Release();
            return;
        }
        hr = g_cmd_list->Reset(g_cmd_allocs[idx], nullptr);
        if (FAILED(hr)) {
            backbuffer->Release();
            return;
        }

        if (g_render_count <= 3) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] render_frame #%d, idx=%u, buf=%p", g_render_count, idx, backbuffer);
        }

        // Barrier: PRESENT → RENDER_TARGET
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource   = backbuffer;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        g_cmd_list->ResourceBarrier(1, &barrier);

        g_cmd_list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        ID3D12DescriptorHeap* heaps[] = { g_srv_heap };
        g_cmd_list->SetDescriptorHeaps(1, heaps);

        // Viewport + scissor from backbuffer dimensions
        D3D12_RESOURCE_DESC rdesc = backbuffer->GetDesc();
        D3D12_VIEWPORT vp = { 0, 0, (float)rdesc.Width, (float)rdesc.Height, 0, 1.0f };
        D3D12_RECT     sr = { 0, 0, (LONG)rdesc.Width, (LONG)rdesc.Height };
        g_cmd_list->RSSetViewports(1, &vp);
        g_cmd_list->RSSetScissorRects(1, &sr);

        // ImGui frame
        feed_imgui_input();
        ImGui::GetIO().MouseDrawCursor = true;

        ImGui_ImplDX12_NewFrame();
        ImGui::NewFrame();
        imgui_menu::draw();
        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_cmd_list);

        // Barrier: RENDER_TARGET → PRESENT
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        g_cmd_list->ResourceBarrier(1, &barrier);

        hr = g_cmd_list->Close();
        if (SUCCEEDED(hr)) {
            ID3D12CommandList* lists[] = { g_cmd_list };
            g_command_queue->ExecuteCommandLists(1, lists);

            g_fence_value++;
            g_fence_values[idx] = g_fence_value;
            g_command_queue->Signal(g_fence, g_fence_value);
        }

        // Release our reference (GetBuffer adds a ref each call)
        backbuffer->Release();

        if (g_render_count <= 3) {
            HRESULT dr = g_d3d12_device->GetDeviceRemovedReason();
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] Frame #%d device=0x%08X", g_render_count, (unsigned)dr);
        }
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

        // F1 toggle
        if (render::is_vk_clicked(VK_F1)) {
            render::show_menu = !render::show_menu;
        }

        if (g_init_failed) {
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        __try {
            if (!g_initialized && g_command_queue != nullptr) {
                if (!init_dx12(swap)) {
                    g_init_failed = true;
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] init_dx12 FAILED");
                }
            }

            if (g_initialized) {
                render_frame(swap);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            static int ex = 0;
            if (++ex <= 3) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Exception #%d in Present", ex);
            }
            if (ex >= 3) {
                g_init_failed = true;
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Too many exceptions — disabling overlay");
            }
        }

        g_rendering = false;
        return oPresent(swap, sync, flags);
    }

    // ── Get vtable addresses from dummy DX12 device ──
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
            DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
        ID3D12CommandQueue* queue = nullptr;
        dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));

        IDXGIFactory4* factory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));

        if (!queue || !factory) {
            if (queue) queue->Release(); if (factory) factory->Release();
            dev->Release(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.BufferCount = 2; sd.Width = sd.Height = 100;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;

        IDXGISwapChain1* swap_dummy = nullptr;
        factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swap_dummy);

        if (!swap_dummy) {
            factory->Release(); queue->Release(); dev->Release();
            DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        void** swap_vt  = *reinterpret_cast<void***>(swap_dummy);
        void** queue_vt = *reinterpret_cast<void***>(queue);

        *present_out = swap_vt[8];
        *execute_out = queue_vt[10];

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Present=%p ExecuteCommandLists=%p", *present_out, *execute_out);

        swap_dummy->Release(); factory->Release(); queue->Release(); dev->Release();
        DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return true;
    }

    // ── Entry point ──
    inline bool initialize() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] === Initialize ===");

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

        if (MH_CreateHook(present_addr, &hkPresent, reinterpret_cast<void**>(&oPresent)) != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hook Present failed");
            return false;
        }

        if (MH_CreateHook(execute_addr, &hkExecuteCommandLists, reinterpret_cast<void**>(&oExecuteCommandLists)) != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hook ExecuteCommandLists failed");
            return false;
        }

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hooks enabled");
        return true;
    }

} // namespace dx_hook
