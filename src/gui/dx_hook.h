#pragma once

// =============================================================
// D3D11On12 Hook — renders ImGui via D3D11 onto the game's DX12 backbuffer.
// D3D11On12 wraps the game's D3D12 device — handles ALL resource state
// transitions internally. No overlay window, no barriers, no cursor issues.
// =============================================================

#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <MinHook.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
// imgui_impl_dx12.h NOT used — we use the DX11 backend through 11On12

#include "render.h"
#include "prisme_theme.h"
#include "imgui_menu.h"
#include "../core/console.h"

namespace dx_hook {

    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using ExecuteCommandListsFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    inline PresentFn             oPresent             = nullptr;
    inline ExecuteCommandListsFn oExecuteCommandLists = nullptr;

    inline bool g_initialized  = false;
    inline bool g_init_failed  = false;
    inline int  g_frame_count  = 0;
    inline bool g_rendering    = false;
    inline HWND g_game_hwnd    = nullptr;
    inline UINT g_buffer_count = 0;

    // Game DX12 objects (captured)
    inline ID3D12Device*       g_d3d12_device  = nullptr;
    inline ID3D12CommandQueue* g_command_queue = nullptr;

    // D3D11On12 objects
    inline ID3D11On12Device*        g_11on12        = nullptr;
    inline ID3D11Device*            g_d3d11_device  = nullptr;
    inline ID3D11DeviceContext*     g_d3d11_context = nullptr;

    // Per-buffer wrapped resources
    inline ID3D11Resource**         g_wrapped_buffers = nullptr;
    inline ID3D11RenderTargetView** g_rtvs            = nullptr;

    // ── ExecuteCommandLists hook — captures game's DIRECT command queue ──
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

        // Virtual mouse: track our own cursor from OS cursor deltas
        if (!g_vmouse_active) {
            // First frame with menu open — start cursor at screen center
            g_vmouse_x = sw / 2.0f;
            g_vmouse_y = sh / 2.0f;
            GetCursorPos(&g_last_raw_cursor);
            g_vmouse_active = true;
        }

        // Un-clip the OS cursor so we get real movement data
        ClipCursor(nullptr);

        POINT raw_cursor;
        if (GetCursorPos(&raw_cursor)) {
            // Compute delta from last known OS cursor position
            float dx = (float)(raw_cursor.x - g_last_raw_cursor.x);
            float dy = (float)(raw_cursor.y - g_last_raw_cursor.y);

            // Apply delta to virtual cursor
            g_vmouse_x += dx;
            g_vmouse_y += dy;

            // Clamp to screen bounds
            if (g_vmouse_x < 0.0f) g_vmouse_x = 0.0f;
            if (g_vmouse_y < 0.0f) g_vmouse_y = 0.0f;
            if (g_vmouse_x > sw) g_vmouse_x = sw;
            if (g_vmouse_y > sh) g_vmouse_y = sh;

            // Re-center OS cursor to prevent it from hitting screen edges
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

    // ── Initialize D3D11On12 on the game's DX12 device ──
    inline bool init_11on12(IDXGISwapChain* swap) {
        DXGI_SWAP_CHAIN_DESC sc_desc;
        if (FAILED(swap->GetDesc(&sc_desc))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] GetDesc failed");
            return false;
        }
        g_game_hwnd    = sc_desc.OutputWindow;
        g_buffer_count = sc_desc.BufferCount;

        if (FAILED(swap->GetDevice(IID_PPV_ARGS(&g_d3d12_device)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] GetDevice failed");
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[11on12] D3D12Device=%p HWND=%p Buffers=%u Format=%u",
            g_d3d12_device, g_game_hwnd, g_buffer_count, sc_desc.BufferDesc.Format);

        // Create D3D11On12 device wrapping the game's DX12 device + queue
        D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0 };
        IUnknown* queues[] = { g_command_queue };

        HRESULT hr = D3D11On12CreateDevice(
            g_d3d12_device,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            feature_levels, 1,
            queues, 1,              // game's command queue as the underlying queue
            0,
            &g_d3d11_device,
            &g_d3d11_context,
            nullptr);

        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] D3D11On12CreateDevice failed: 0x%08X", (unsigned)hr);
            return false;
        }

        if (FAILED(g_d3d11_device->QueryInterface(IID_PPV_ARGS(&g_11on12)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] QueryInterface ID3D11On12Device failed");
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] D3D11On12 device created");

        // Create wrapped render targets for each backbuffer
        g_wrapped_buffers = new ID3D11Resource*[g_buffer_count]();
        g_rtvs            = new ID3D11RenderTargetView*[g_buffer_count]();

        for (UINT i = 0; i < g_buffer_count; i++) {
            ID3D12Resource* backbuffer12 = nullptr;
            if (FAILED(swap->GetBuffer(i, IID_PPV_ARGS(&backbuffer12)))) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] GetBuffer(%u) failed", i);
                return false;
            }

            // Wrap DX12 backbuffer as D3D11 resource
            // InResourceState = PRESENT: backbuffer is in PRESENT state when our Present hook runs
            // OutResourceState = PRESENT: leave it in PRESENT state after we're done (for actual Present)
            D3D11_RESOURCE_FLAGS res_flags = { D3D11_BIND_RENDER_TARGET };
            hr = g_11on12->CreateWrappedResource(
                backbuffer12,
                &res_flags,
                D3D12_RESOURCE_STATE_PRESENT,   // state to transition FROM on AcquireWrappedResources
                D3D12_RESOURCE_STATE_PRESENT,   // state to transition TO on ReleaseWrappedResources
                IID_PPV_ARGS(&g_wrapped_buffers[i]));
            backbuffer12->Release();

            if (FAILED(hr)) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] CreateWrappedResource(%u) failed: 0x%08X", i, (unsigned)hr);
                return false;
            }

            // Create D3D11 render target view
            hr = g_d3d11_device->CreateRenderTargetView(g_wrapped_buffers[i], nullptr, &g_rtvs[i]);
            if (FAILED(hr)) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] CreateRenderTargetView(%u) failed: 0x%08X", i, (unsigned)hr);
                return false;
            }
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Wrapped %u backbuffers", g_buffer_count);

        // Init ImGui with D3D11 backend
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags    |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename     = nullptr;
        io.MouseDrawCursor = true;

        // Detect DPI scale from game resolution
        float dpi_scale = 1.0f;
        {
            DXGI_SWAP_CHAIN_DESC sd;
            if (SUCCEEDED(swap->GetDesc(&sd))) {
                if (sd.BufferDesc.Height >= 2160) dpi_scale = 1.5f;       // 4K
                else if (sd.BufferDesc.Height >= 1440) dpi_scale = 1.25f; // 1440p
            }
        }

        // Load high-quality font with subpixel oversampling
        ImFontConfig font_cfg;
        font_cfg.OversampleH       = 3;    // horizontal oversampling for crisp text
        font_cfg.OversampleV       = 2;    // vertical oversampling
        font_cfg.PixelSnapH        = false; // smoother at non-integer positions
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
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Loaded font: %s @ %.0fpx (dpi=%.2f)", path, font_size, dpi_scale);
                break;
            }
        }

        // Also load a bold/title font (Segoe UI Semibold or Bold)
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
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Loaded title font: %s", path);
                break;
            }
        }

        if (!font_main) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] No system font found, using default");
        }

        // Store fonts for menu use
        imgui_menu::g_font_main  = font_main;
        imgui_menu::g_font_title = font_title;
        imgui_menu::g_dpi_scale  = dpi_scale;

        prisme_theme::apply();

        ImGui_ImplDX11_Init(g_d3d11_device, g_d3d11_context);
        // NO ImGui_ImplWin32_Init — input fed manually

        g_initialized = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Init complete, waiting for F1");
        return true;
    }

    // ── Render one frame ──
    inline void render_frame(IDXGISwapChain* swap) {
        if (!g_command_queue) return;
        if (!render::show_menu) {
            // Reset virtual mouse so it re-centers when menu reopens
            g_vmouse_active = false;
            return;
        }

        static bool first_render = false;
        if (!first_render) {
            first_render = true;
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] First render_frame");
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

        // Safety: clamp index
        if (idx >= g_buffer_count) idx = 0;

        // Acquire the wrapped resource — D3D11On12 transitions it from PRESENT to RENDER_TARGET
        g_11on12->AcquireWrappedResources(&g_wrapped_buffers[idx], 1);

        // Set render target
        g_d3d11_context->OMSetRenderTargets(1, &g_rtvs[idx], nullptr);

        // Feed input + render ImGui
        feed_imgui_input();
        ImGui::GetIO().MouseDrawCursor = true;

        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        imgui_menu::draw();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Release — D3D11On12 transitions backbuffer back to PRESENT state
        g_11on12->ReleaseWrappedResources(&g_wrapped_buffers[idx], 1);

        // Flush D3D11 commands to the underlying D3D12 queue
        g_d3d11_context->Flush();

        static bool first_ok = false;
        if (!first_ok) {
            first_ok = true;
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] First ImGui frame rendered OK!");
        }
    }

    // ── Present hook ──
    inline HRESULT WINAPI hkPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
        if (g_rendering) return oPresent(swap, sync, flags);
        g_rendering = true;

        if (++g_frame_count < 100) {
            if (g_frame_count == 1)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Present hook ALIVE");
            if (g_frame_count == 50)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Warmup 50/100...");
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        // F1 toggle — runs every frame from Present hook (independent of DrawTransition)
        if (render::is_vk_clicked(VK_F1)) {
            render::show_menu = !render::show_menu;
        }

        if (g_init_failed) {
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        __try {
            if (!g_initialized && g_command_queue != nullptr) {
                if (!init_11on12(swap)) {
                    g_init_failed = true;
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] init_11on12 FAILED");
                }
            }

            if (g_initialized) {
                render_frame(swap);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            static int ex = 0;
            if (++ex <= 5)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Exception #%d in Present", ex);
            if (ex >= 5) g_init_failed = true;
        }

        g_rendering = false;
        return oPresent(swap, sync, flags);
    }

    // ── Get vtable addresses from dummy DX12 device ──
    inline bool get_hook_addresses(void** present_out, void** execute_out) {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Creating dummy device for vtable...");

        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0,
                           GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                           L"DXGIDummy12", nullptr };
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                  0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        ID3D12Device* dev = nullptr;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] D3D12CreateDevice failed");
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

        *present_out = swap_vt[8];     // IDXGISwapChain::Present
        *execute_out = queue_vt[10];   // ID3D12CommandQueue::ExecuteCommandLists

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[11on12] Present=%p ExecuteCommandLists=%p", *present_out, *execute_out);

        swap_dummy->Release(); factory->Release(); queue->Release(); dev->Release();
        DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return true;
    }

    // ── Entry point ──
    inline bool initialize() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] === Initialize (D3D11On12) ===");

        auto mh = MH_Initialize();
        if (mh != MH_OK && mh != MH_ERROR_ALREADY_INITIALIZED) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] MH_Initialize failed: %d", mh);
            return false;
        }

        void* present_addr = nullptr;
        void* execute_addr = nullptr;
        if (!get_hook_addresses(&present_addr, &execute_addr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Failed to get hook addresses");
            return false;
        }

        if (MH_CreateHook(present_addr, &hkPresent, reinterpret_cast<void**>(&oPresent)) != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Hook Present failed");
            return false;
        }

        if (MH_CreateHook(execute_addr, &hkExecuteCommandLists, reinterpret_cast<void**>(&oExecuteCommandLists)) != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Hook ExecuteCommandLists failed");
            return false;
        }

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[11on12] Hooks enabled");
        return true;
    }

} // namespace dx_hook
