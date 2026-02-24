#pragma once

// =============================================================
// External Overlay — transparent D3D11 window on top of game.
// Completely avoids touching the game's DX12 swapchain.
// Hooks Present only for F1 toggle + game HWND detection.
// ImGui renders to our own D3D11 device + swapchain.
// =============================================================

#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dcomp.h>
#include <MinHook.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include "render.h"
#include "prisme_theme.h"
#include "imgui_menu.h"
#include "../core/console.h"
#include "../features/chams.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// Dynamic imports — manual map injector can't resolve static imports
using PFN_D3D11_CREATE_DEVICE = HRESULT(WINAPI*)(
    IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT,
    const D3D_FEATURE_LEVEL*, UINT, UINT,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

using PFN_DCOMP_CREATE_DEVICE = HRESULT(WINAPI*)(
    IDXGIDevice*, REFIID, void**);

namespace dx_hook {

    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);
    using ExecuteCommandListsFn = void(WINAPI*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

    inline PresentFn             oPresent             = nullptr;
    inline ExecuteCommandListsFn oExecuteCommandLists = nullptr;

    inline bool g_initialized    = false;
    inline bool g_init_failed    = false;
    inline bool g_imgui_ready    = false;
    inline int  g_frame_count    = 0;
    inline bool g_rendering      = false;
    inline HWND g_game_hwnd      = nullptr;

    // Game DX12 objects (captured for chams vtable compatibility)
    inline ID3D12CommandQueue* g_command_queue  = nullptr;
    inline ID3D12CommandQueue* g_last_queue     = nullptr;

    // Our overlay window + D3D11 device + DirectComposition
    inline HWND                    g_overlay_hwnd    = nullptr;
    inline ID3D11Device*           g_d3d11_device    = nullptr;
    inline ID3D11DeviceContext*    g_d3d11_context   = nullptr;
    inline IDXGISwapChain1*        g_d3d11_swap      = nullptr;
    inline ID3D11RenderTargetView* g_d3d11_rtv       = nullptr;
    inline IDCompositionDevice*    g_dcomp_device    = nullptr;
    inline IDCompositionTarget*    g_dcomp_target    = nullptr;
    inline IDCompositionVisual*    g_dcomp_visual    = nullptr;

    // ── ExecuteCommandLists hook — tracks queues for chams compatibility ──
    inline void WINAPI hkExecuteCommandLists(
        ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists)
    {
        if (queue) {
            D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
            if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
                if (!g_command_queue) g_command_queue = queue;
                g_last_queue = queue;
            }
        }
        oExecuteCommandLists(queue, count, lists);
    }

    // ── Virtual mouse state ──
    inline float g_vmouse_x = 0, g_vmouse_y = 0;
    inline bool  g_vmouse_active = false;
    inline POINT g_last_raw_cursor = {};

    // ── Manual ImGui input with virtual mouse ──
    inline void feed_imgui_input() {
        ImGuiIO& io = ImGui::GetIO();

        RECT r;
        if (g_game_hwnd && GetClientRect(g_game_hwnd, &r))
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

            if (g_vmouse_x < 0)  g_vmouse_x = 0;
            if (g_vmouse_y < 0)  g_vmouse_y = 0;
            if (g_vmouse_x > sw) g_vmouse_x = sw;
            if (g_vmouse_y > sh) g_vmouse_y = sh;

            g_last_raw_cursor = raw_cursor;
        }

        io.MousePos = ImVec2(g_vmouse_x, g_vmouse_y);
        io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

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

    // ── Overlay window proc ──
    inline LRESULT CALLBACK overlay_wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        // Block any attempt to close/destroy our overlay
        if (msg == WM_CLOSE || msg == WM_DESTROY || msg == WM_QUIT)
            return 0;
        // Block keyboard messages (we handle input ourselves via GetAsyncKeyState)
        if (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
            return 0;
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    // ── Position overlay over game window ──
    inline void sync_overlay_position() {
        if (!g_game_hwnd || !g_overlay_hwnd) return;

        RECT gr;
        if (!GetWindowRect(g_game_hwnd, &gr)) return;

        int gw = gr.right - gr.left;
        int gh = gr.bottom - gr.top;

        // Owned window stays above game automatically.
        // Just keep position and size in sync.
        SetWindowPos(g_overlay_hwnd, HWND_TOPMOST,
            gr.left, gr.top, gw, gh,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }

    // ── Create transparent overlay window + D3D11 device + DirectComposition ──
    inline bool init_overlay() {
        if (g_imgui_ready) return true;
        if (!g_game_hwnd) return false;

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] Creating DirectComposition overlay...");

        // Register overlay window class
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = overlay_wndproc;
        wc.hInstance      = GetModuleHandleW(nullptr);
        wc.lpszClassName  = L"PrismeOverlay";
        RegisterClassExW(&wc);

        // Get game window rect
        RECT gr;
        GetWindowRect(g_game_hwnd, &gr);
        int gw = gr.right - gr.left;
        int gh = gr.bottom - gr.top;

        // Create click-through overlay window (no owner — DirectComposition handles compositing)
        g_overlay_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            L"PrismeOverlay", L"",
            WS_POPUP,
            gr.left, gr.top, gw, gh,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!g_overlay_hwnd) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] CreateWindowExW failed: %u", GetLastError());
            return false;
        }

        ShowWindow(g_overlay_hwnd, SW_SHOWNOACTIVATE);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] Window created: %dx%d", gw, gh);

        // ── Step 1: Create D3D11 device (no swap chain) ──
        HMODULE d3d11_mod = LoadLibraryA("d3d11.dll");
        if (!d3d11_mod) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] LoadLibrary(d3d11.dll) failed");
            return false;
        }
        auto pD3D11CreateDevice = (PFN_D3D11_CREATE_DEVICE)
            GetProcAddress(d3d11_mod, "D3D11CreateDevice");
        if (!pD3D11CreateDevice) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] GetProcAddress(D3D11CreateDevice) failed");
            return false;
        }

        D3D_FEATURE_LEVEL featureLevel;
        HRESULT hr = pD3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION,
            &g_d3d11_device, &featureLevel, &g_d3d11_context);

        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] D3D11CreateDevice failed: 0x%08X", (unsigned)hr);
            return false;
        }
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] D3D11 device created (feature level: 0x%X)", featureLevel);

        // ── Step 2: Get DXGI factory from device ──
        IDXGIDevice* dxgiDevice = nullptr;
        g_d3d11_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        IDXGIAdapter* adapter = nullptr;
        dxgiDevice->GetAdapter(&adapter);
        IDXGIFactory2* factory2 = nullptr;
        adapter->GetParent(IID_PPV_ARGS(&factory2));

        // ── Step 3: Create swap chain for composition (premultiplied alpha) ──
        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width       = gw;
        sd.Height      = gh;
        sd.Format      = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.AlphaMode   = DXGI_ALPHA_MODE_PREMULTIPLIED;

        hr = factory2->CreateSwapChainForComposition(g_d3d11_device, &sd, nullptr, &g_d3d11_swap);
        factory2->Release();
        adapter->Release();

        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] CreateSwapChainForComposition failed: 0x%08X", (unsigned)hr);
            dxgiDevice->Release();
            return false;
        }
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] Composition swap chain created");

        // ── Step 4: DirectComposition — bind swap chain to window ──
        HMODULE dcomp_mod = LoadLibraryA("dcomp.dll");
        if (!dcomp_mod) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] LoadLibrary(dcomp.dll) failed");
            dxgiDevice->Release();
            return false;
        }
        auto pDCompCreate = (PFN_DCOMP_CREATE_DEVICE)
            GetProcAddress(dcomp_mod, "DCompositionCreateDevice");
        if (!pDCompCreate) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] GetProcAddress(DCompositionCreateDevice) failed");
            dxgiDevice->Release();
            return false;
        }

        hr = pDCompCreate(dxgiDevice, IID_PPV_ARGS(&g_dcomp_device));
        dxgiDevice->Release();
        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] DCompositionCreateDevice failed: 0x%08X", (unsigned)hr);
            return false;
        }

        g_dcomp_device->CreateTargetForHwnd(g_overlay_hwnd, TRUE, &g_dcomp_target);
        g_dcomp_device->CreateVisual(&g_dcomp_visual);
        g_dcomp_visual->SetContent(g_d3d11_swap);
        g_dcomp_target->SetRoot(g_dcomp_visual);
        g_dcomp_device->Commit();
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] DirectComposition bound");

        // ── Step 5: Create RTV ──
        ID3D11Texture2D* backbuffer = nullptr;
        g_d3d11_swap->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
        g_d3d11_device->CreateRenderTargetView(backbuffer, nullptr, &g_d3d11_rtv);
        backbuffer->Release();
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] RTV created");

        // Init ImGui
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;  // No keyboard nav — we use virtual mouse
        io.IniFilename  = nullptr;
        io.MouseDrawCursor = true;

        // DPI scale
        float dpi_scale = 1.0f;
        if (gh >= 2160) dpi_scale = 1.5f;
        else if (gh >= 1440) dpi_scale = 1.25f;

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
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] Font: %s @ %.0fpx", path, font_size);
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
            if (font_title) break;
        }

        imgui_menu::g_font_main  = font_main;
        imgui_menu::g_font_title = font_title;
        imgui_menu::g_dpi_scale  = dpi_scale;

        prisme_theme::apply();

        // Init ImGui backends (shutdown first if already initialized — prevents assertion)
        if (ImGui::GetIO().BackendPlatformUserData != nullptr) {
            ImGui_ImplWin32_Shutdown();
            ImGui_ImplDX11_Shutdown();
        }
        ImGui_ImplWin32_Init(g_overlay_hwnd);
        ImGui_ImplDX11_Init(g_d3d11_device, g_d3d11_context);

        g_imgui_ready = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] ImGui initialized — READY");
        return true;
    }

    // ── Render one frame to overlay ──
    inline int g_render_count = 0;

    inline void render_frame() {
        if (!g_imgui_ready) {
            if (g_init_failed) return;
            // Only init overlay when menu is first opened
            if (!render::show_menu) return;
            if (!init_overlay()) {
                g_init_failed = true;
                dbg::log_ex(dbg::Level::Error, dbg::Init, "[Overlay] init_overlay failed — won't retry");
                return;
            }
        }

        // Pump overlay window messages (required for window to display)
        MSG msg;
        while (PeekMessageW(&msg, g_overlay_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Keep overlay positioned over game (every frame, even when hidden)
        sync_overlay_position();

        // Clear to fully transparent — DirectComposition makes empty frame invisible
        float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_d3d11_context->OMSetRenderTargets(1, &g_d3d11_rtv, nullptr);
        g_d3d11_context->ClearRenderTargetView(g_d3d11_rtv, clear_color);

        if (render::show_menu) {
            // Feed input and render ImGui
            feed_imgui_input();

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            imgui_menu::draw();
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        } else {
            g_vmouse_active = false;
        }

        g_render_count++;
        if (g_render_count <= 3) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[Overlay] render_frame #%d", g_render_count);
        }

        // Always present — transparent clear = invisible when menu closed
        g_d3d11_swap->Present(0, 0);
    }

    // ── Present hook — only for F1 toggle + game HWND capture ──
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

        // Capture game HWND once
        if (!g_game_hwnd) {
            DXGI_SWAP_CHAIN_DESC sc_desc;
            if (SUCCEEDED(swap->GetDesc(&sc_desc))) {
                g_game_hwnd = sc_desc.OutputWindow;
                g_initialized = true;
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] Game HWND=%p captured", g_game_hwnd);
            }
        }

        // F1 toggle — robust state machine with logging
        {
            static bool f1_was_down = false;
            static DWORD last_toggle_time = 0;
            static int toggle_count = 0;

            bool f1_down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;

            // Only toggle on key RELEASE (not press) — prevents game interference
            if (f1_was_down && !f1_down) {
                DWORD now = GetTickCount();
                if ((now - last_toggle_time) > 500) {
                    render::show_menu = !render::show_menu;
                    last_toggle_time = now;
                    toggle_count++;
                    if (toggle_count <= 20) {
                        dbg::log_ex(dbg::Level::Warn, dbg::Init,
                            "[F1] Toggle #%d → show_menu=%s (t=%u)",
                            toggle_count, render::show_menu ? "TRUE" : "FALSE", now);
                    }
                }
            }
            f1_was_down = f1_down;
        }

        if (g_init_failed) {
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        __try {
            if (g_initialized) {
                render_frame();
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            static int ex = 0;
            if (++ex <= 5) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] Exception #%d in render", ex);
            }
            if (ex >= 5) {
                g_init_failed = true;
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[Overlay] Too many exceptions — disabling");
            }
        }

        g_rendering = false;
        return oPresent(swap, sync, flags);
    }

    // ── Get vtable addresses from dummy DX12 device ──
    inline bool get_hook_addresses(void** present_out, void** execute_out,
        void** draw_indexed_out = nullptr, void** ia_set_vb_out = nullptr,
        void** set_pso_out = nullptr, void** create_pso_out = nullptr) {
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

        // Extract chams vtable addresses
        if (draw_indexed_out && ia_set_vb_out && set_pso_out && create_pso_out) {
            features::chams::get_vtable_addresses(dev,
                draw_indexed_out, ia_set_vb_out, set_pso_out, create_pso_out);
        }

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
        void* draw_indexed_addr = nullptr;
        void* ia_set_vb_addr = nullptr;
        void* set_pso_addr = nullptr;
        void* create_pso_addr = nullptr;

        if (!get_hook_addresses(&present_addr, &execute_addr,
            &draw_indexed_addr, &ia_set_vb_addr, &set_pso_addr, &create_pso_addr)) {
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

        // Store chams vtable addresses
        if (draw_indexed_addr && ia_set_vb_addr && set_pso_addr && create_pso_addr) {
            features::chams::g_addr_draw_indexed = draw_indexed_addr;
            features::chams::g_addr_ia_set_vb    = ia_set_vb_addr;
            features::chams::g_addr_set_pso      = set_pso_addr;
            features::chams::g_addr_create_pso   = create_pso_addr;
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] Chams vtable addresses saved");
        }

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hooks enabled");
        return true;
    }

} // namespace dx_hook
