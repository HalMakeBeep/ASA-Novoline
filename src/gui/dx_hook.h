#pragma once

// =============================================================
// Transparent overlay approach: creates a SEPARATE transparent
// window with its own DX11 device. ZERO interaction with the
// game's DX12 rendering pipeline. Cannot cause GPU crashes.
//
// - Present hook: only reads game HWND + provides frame timing
// - WndProc hook: feeds input to ImGui when menu is open
// - Overlay window: our own DX11, renders ImGui independently
// =============================================================

#include <d3d11.h>
#include <d3d12.h>      // only for dummy device to read DXGI vtable
#include <dxgi1_4.h>
#include <dwmapi.h>
#include <MinHook.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

#include "render.h"
#include "prisme_theme.h"
#include "imgui_menu.h"
#include "../core/console.h"

#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace dx_hook {

    using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

    // ── Hook state ──
    inline PresentFn oPresent           = nullptr;
    inline HWND      g_game_hwnd        = nullptr;
    inline WNDPROC   g_game_wndproc     = nullptr;
    inline bool      g_initialized      = false;
    inline bool      g_init_failed      = false;
    inline int       g_frame_count      = 0;
    inline bool      g_rendering        = false;   // re-entrancy guard

    // ── Our own overlay (completely independent from game) ──
    inline HWND                   g_overlay_hwnd = nullptr;
    inline ID3D11Device*          g_device       = nullptr;
    inline ID3D11DeviceContext*   g_context      = nullptr;
    inline IDXGISwapChain*        g_swapchain    = nullptr;
    inline ID3D11RenderTargetView* g_rtv         = nullptr;

    // ── Game WndProc hook (input handling) ──
    inline LRESULT WINAPI hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        __try {
            if (g_initialized && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
                return true;

            if (g_initialized && render::show_menu) {
                ImGuiIO& io = ImGui::GetIO();
                if (io.WantCaptureMouse) {
                    switch (msg) {
                        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
                        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
                        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
                        case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
                        case WM_MOUSEMOVE:
                            return true;
                    }
                }
                if (io.WantCaptureKeyboard) {
                    switch (msg) {
                        case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
                        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
                            return true;
                    }
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
        return CallWindowProcW(g_game_wndproc, hwnd, msg, wParam, lParam);
    }

    // ── Create overlay window + DX11 device ──
    inline bool init_overlay() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] init_overlay (game_hwnd=%p)...", g_game_hwnd);

        // Get game client area in screen coords
        RECT r;
        GetClientRect(g_game_hwnd, &r);
        POINT pt = {0, 0};
        ClientToScreen(g_game_hwnd, &pt);
        int w = r.right, h = r.bottom;

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Game client: %dx%d at (%d,%d)", w, h, pt.x, pt.y);

        // Register overlay window class
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc  = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"PrismeOvl";
        RegisterClassExW(&wc);

        // Create transparent, topmost, click-through overlay
        // WS_EX_TOOLWINDOW hides from taskbar/alt-tab
        g_overlay_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
            L"PrismeOvl", L"",
            WS_POPUP,
            pt.x, pt.y, w, h,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!g_overlay_hwnd) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] CreateWindowExW failed: %u", GetLastError());
            return false;
        }

        // Color key transparency: RGB(0,0,0) pixels become transparent
        SetLayeredWindowAttributes(g_overlay_hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(g_overlay_hwnd, SW_SHOWNOACTIVATE);

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Overlay window created: %p", g_overlay_hwnd);

        // Create our own DX11 device + swap chain (completely separate from game!)
        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount        = 2;
        sd.BufferDesc.Width   = w;
        sd.BufferDesc.Height  = h;
        sd.BufferDesc.Format  = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow       = g_overlay_hwnd;
        sd.SampleDesc.Count   = 1;
        sd.Windowed           = TRUE;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL fl  = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &fl, 1, D3D11_SDK_VERSION,
            &sd, &g_swapchain, &g_device, nullptr, &g_context);

        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] D3D11 create failed: 0x%08X", (unsigned)hr);
            DestroyWindow(g_overlay_hwnd); g_overlay_hwnd = nullptr;
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] DX11 device created for overlay");

        // Create render target view
        ID3D11Texture2D* bb = nullptr;
        g_swapchain->GetBuffer(0, IID_PPV_ARGS(&bb));
        g_device->CreateRenderTargetView(bb, nullptr, &g_rtv);
        bb->Release();

        // Init ImGui with GAME hwnd (for correct mouse coordinate mapping)
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename  = nullptr;
        io.MouseDrawCursor = true;

        prisme_theme::apply();
        ImGui_ImplWin32_Init(g_game_hwnd);              // game HWND for input
        ImGui_ImplDX11_Init(g_device, g_context);       // our DX11 for rendering

        // Hook game WndProc for ImGui input
        g_game_wndproc = (WNDPROC)SetWindowLongPtrW(g_game_hwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        g_initialized = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Overlay fully initialized!");
        return true;
    }

    // ── Render one frame ──
    inline void render_frame() {
        // Pump overlay window messages
        MSG msg;
        while (PeekMessageW(&msg, g_overlay_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Keep overlay on top, matching game position
        RECT r;
        GetClientRect(g_game_hwnd, &r);
        POINT pt = {0, 0};
        ClientToScreen(g_game_hwnd, &pt);
        SetWindowPos(g_overlay_hwnd, HWND_TOPMOST,
            pt.x, pt.y, r.right, r.bottom,
            SWP_NOACTIVATE | SWP_NOSENDCHANGING);

        // Clear with fully transparent
        float clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        g_context->ClearRenderTargetView(g_rtv, clear_color);
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);

        // Viewport
        D3D11_VIEWPORT vp = {};
        vp.Width  = (float)r.right;
        vp.Height = (float)r.bottom;
        g_context->RSSetViewports(1, &vp);

        // ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::GetIO().MouseDrawCursor = render::show_menu;
        if (render::show_menu) {
            imgui_menu::draw();
        }
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Present our overlay (NOT the game's swap chain)
        g_swapchain->Present(1, 0);
    }

    // ── Present hook: MINIMAL, only reads HWND + provides frame timing ──
    inline HRESULT WINAPI hkPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
        // Re-entrancy guard: our overlay's Present may trigger this hook
        if (g_rendering) return oPresent(swap, sync, flags);
        g_rendering = true;

        g_frame_count++;

        // Warmup: let engine fully init
        if (g_frame_count < 100) {
            if (g_frame_count == 1)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Present hook ALIVE, warming up...");
            if (g_frame_count == 50)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Warmup 50/100...");
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        if (g_init_failed) {
            g_rendering = false;
            return oPresent(swap, sync, flags);
        }

        __try {
            // Get game HWND from swap chain (once)
            if (!g_game_hwnd) {
                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED(swap->GetDesc(&desc))) {
                    g_game_hwnd = desc.OutputWindow;
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Game HWND: %p", g_game_hwnd);
                }
            }

            // Init overlay (once)
            if (!g_initialized && g_game_hwnd) {
                if (!init_overlay()) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] init_overlay FAILED");
                    g_init_failed = true;
                }
            }

            // Render each frame
            if (g_initialized) {
                render_frame();
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            static int ex_count = 0;
            if (++ex_count <= 3)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Exception #%d in Present", ex_count);
            if (ex_count >= 3) g_init_failed = true;
        }

        g_rendering = false;
        return oPresent(swap, sync, flags);
    }

    // ── Get DXGI Present address via dummy DX12 device ──
    inline bool get_present_addr(void** out_present) {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Creating dummy device for vtable...");

        WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0,
                          GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                          L"DXGIDummy", nullptr};
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                  0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        ID3D12Device* dev = nullptr;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] D3D12CreateDevice failed");
            DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        D3D12_COMMAND_QUEUE_DESC qd = {};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ID3D12CommandQueue* queue = nullptr;
        dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
        if (!queue) {
            dev->Release(); DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        IDXGIFactory4* factory = nullptr;
        CreateDXGIFactory1(IID_PPV_ARGS(&factory));
        if (!factory) {
            queue->Release(); dev->Release();
            DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.BufferCount    = 2;
        sd.Width          = 100;
        sd.Height         = 100;
        sd.Format         = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage    = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect     = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;

        IDXGISwapChain1* swap = nullptr;
        factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &swap);
        if (!swap) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Dummy swap chain failed");
            factory->Release(); queue->Release(); dev->Release();
            DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        void** vtable = *reinterpret_cast<void***>(swap);
        *out_present = vtable[8]; // IDXGISwapChain::Present

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Present addr=%p", *out_present);

        swap->Release();
        factory->Release();
        queue->Release();
        dev->Release();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return true;
    }

    // ── Main init ──
    inline bool initialize() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] === Initialize (overlay window mode) ===");

        if (MH_Initialize() != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] MH_Initialize failed");
            return false;
        }

        void* present_addr = nullptr;
        if (!get_present_addr(&present_addr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Failed to get Present address");
            return false;
        }

        // Only hook Present - nothing else!
        MH_STATUS s = MH_CreateHook(present_addr, &hkPresent, reinterpret_cast<void**>(&oPresent));
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Hook Present result=%d", s);
        if (s != MH_OK) return false;

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Hook enabled, waiting for Present calls...");
        return true;
    }

} // namespace dx_hook
