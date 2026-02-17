#pragma once

// =============================================================
// Transparent overlay using DirectComposition for per-pixel alpha.
// Own DX11 device, ZERO interaction with game's DX12 pipeline.
// =============================================================

#include <d3d11.h>
#include <d3d12.h>      // only for dummy device vtable
#include <dxgi1_2.h>
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
    inline bool      g_rendering        = false;

    // ── Our overlay (independent DX11) ──
    inline HWND                     g_overlay_hwnd  = nullptr;
    inline ID3D11Device*            g_device        = nullptr;
    inline ID3D11DeviceContext*     g_context       = nullptr;
    inline IDXGISwapChain1*         g_swapchain     = nullptr;
    inline ID3D11RenderTargetView*  g_rtv           = nullptr;
    inline IDCompositionDevice*     g_dcomp         = nullptr;
    inline IDCompositionTarget*     g_dcomp_target  = nullptr;
    inline IDCompositionVisual*     g_dcomp_visual  = nullptr;
    inline int                      g_width         = 0;
    inline int                      g_height        = 0;

    // ── Game WndProc hook ──
    inline LRESULT WINAPI hkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        __try {
            if (g_initialized) {
                if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
                    return true;

                if (render::show_menu) {
                    switch (msg) {
                        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
                        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
                        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
                        case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
                        case WM_MOUSEMOVE:
                        case WM_KEYDOWN: case WM_KEYUP: case WM_CHAR:
                        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
                            return true;
                    }
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
        return CallWindowProcW(g_game_wndproc, hwnd, msg, wParam, lParam);
    }

    // ── Create overlay ──
    inline bool init_overlay() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] init_overlay (game=%p)...", g_game_hwnd);

        RECT r;
        GetClientRect(g_game_hwnd, &r);
        POINT pt = {0, 0};
        ClientToScreen(g_game_hwnd, &pt);
        g_width = r.right;
        g_height = r.bottom;

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Game: %dx%d at (%d,%d)", g_width, g_height, pt.x, pt.y);

        // Register class
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc  = DefWindowProcW;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"PrismeOvl";
        RegisterClassExW(&wc);

        // WS_EX_NOREDIRECTIONBITMAP is REQUIRED for DirectComposition transparency
        g_overlay_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
            L"PrismeOvl", L"",
            WS_POPUP,
            pt.x, pt.y, g_width, g_height,
            nullptr, nullptr, wc.hInstance, nullptr);

        if (!g_overlay_hwnd) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] CreateWindow failed: %u", GetLastError());
            return false;
        }
        ShowWindow(g_overlay_hwnd, SW_SHOWNOACTIVATE);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Overlay window: %p", g_overlay_hwnd);

        // ── Create DX11 device ──
        D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            &fl, 1, D3D11_SDK_VERSION,
            &g_device, nullptr, &g_context);
        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] D3D11CreateDevice failed: 0x%08X", (unsigned)hr);
            return false;
        }
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] DX11 device OK");

        // ── Get DXGI factory from device ──
        IDXGIDevice* dxgi_dev = nullptr;
        g_device->QueryInterface(IID_PPV_ARGS(&dxgi_dev));
        IDXGIAdapter* adapter = nullptr;
        dxgi_dev->GetAdapter(&adapter);
        IDXGIFactory2* factory = nullptr;
        adapter->GetParent(IID_PPV_ARGS(&factory));
        adapter->Release();

        // ── Create swap chain for composition (premultiplied alpha!) ──
        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width              = g_width;
        sd.Height             = g_height;
        sd.Format             = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count   = 1;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount        = 2;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.AlphaMode          = DXGI_ALPHA_MODE_PREMULTIPLIED;

        hr = factory->CreateSwapChainForComposition(g_device, &sd, nullptr, &g_swapchain);
        factory->Release();
        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] CreateSwapChainForComposition failed: 0x%08X", (unsigned)hr);
            dxgi_dev->Release();
            return false;
        }
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Composition swap chain OK");

        // ── RTV ──
        ID3D11Texture2D* bb = nullptr;
        g_swapchain->GetBuffer(0, IID_PPV_ARGS(&bb));
        g_device->CreateRenderTargetView(bb, nullptr, &g_rtv);
        bb->Release();

        // ── DirectComposition: bind swap chain to overlay window ──
        hr = DCompositionCreateDevice(dxgi_dev, IID_PPV_ARGS(&g_dcomp));
        dxgi_dev->Release();
        if (FAILED(hr)) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] DCompositionCreateDevice failed: 0x%08X", (unsigned)hr);
            return false;
        }

        g_dcomp->CreateTargetForHwnd(g_overlay_hwnd, TRUE, &g_dcomp_target);
        g_dcomp->CreateVisual(&g_dcomp_visual);
        g_dcomp_visual->SetContent(g_swapchain);
        g_dcomp_target->SetRoot(g_dcomp_visual);
        g_dcomp->Commit();
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] DirectComposition bound");

        // ── ImGui ──
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename  = nullptr;
        io.MouseDrawCursor = true;

        prisme_theme::apply();
        ImGui_ImplWin32_Init(g_game_hwnd);
        ImGui_ImplDX11_Init(g_device, g_context);

        // Hook game WndProc
        g_game_wndproc = (WNDPROC)SetWindowLongPtrW(g_game_hwnd, GWLP_WNDPROC, (LONG_PTR)hkWndProc);

        g_initialized = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Overlay fully initialized!");
        return true;
    }

    // ── Render ──
    inline void render_frame() {
        // Pump overlay messages
        MSG msg;
        while (PeekMessageW(&msg, g_overlay_hwnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Only render when game window is valid
        if (IsWindow(g_game_hwnd) && !IsIconic(g_game_hwnd)) {
            RECT r;
            GetClientRect(g_game_hwnd, &r);
            POINT pt = {0, 0};
            ClientToScreen(g_game_hwnd, &pt);

            if (!IsWindowVisible(g_overlay_hwnd))
                ShowWindow(g_overlay_hwnd, SW_SHOWNOACTIVATE);

            SetWindowPos(g_overlay_hwnd, HWND_TOPMOST,
                pt.x, pt.y, r.right, r.bottom,
                SWP_NOACTIVATE | SWP_NOSENDCHANGING);

            // Clear transparent (alpha=0 = fully see-through)
            float clear[4] = {0.f, 0.f, 0.f, 0.f};
            g_context->ClearRenderTargetView(g_rtv, clear);
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);

            D3D11_VIEWPORT vp = {};
            vp.Width  = (float)r.right;
            vp.Height = (float)r.bottom;
            g_context->RSSetViewports(1, &vp);

            // ImGui
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGui::GetIO().MouseDrawCursor = render::show_menu;
            if (render::show_menu) {
                imgui_menu::draw();
            }
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            g_swapchain->Present(0, 0);
        }
    }

    // ── Present hook (MINIMAL) ──
    inline HRESULT WINAPI hkPresent(IDXGISwapChain* swap, UINT sync, UINT flags) {
        if (g_rendering) return oPresent(swap, sync, flags);
        g_rendering = true;

        g_frame_count++;

        if (g_frame_count < 100) {
            if (g_frame_count == 1)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Present hook ALIVE");
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
            if (!g_game_hwnd) {
                DXGI_SWAP_CHAIN_DESC desc;
                if (SUCCEEDED(swap->GetDesc(&desc))) {
                    g_game_hwnd = desc.OutputWindow;
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Game HWND: %p", g_game_hwnd);
                }
            }

            if (!g_initialized && g_game_hwnd) {
                if (!init_overlay()) {
                    dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] init_overlay FAILED");
                    g_init_failed = true;
                }
            }

            if (g_initialized) {
                render_frame();
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            static int ex = 0;
            if (++ex <= 3)
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Exception #%d", ex);
            if (ex >= 3) g_init_failed = true;
        }

        g_rendering = false;
        return oPresent(swap, sync, flags);
    }

    // ── Get Present vtable address ──
    inline bool get_present_addr(void** out) {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Creating dummy device...");

        WNDCLASSEXW wc = {sizeof(wc), CS_CLASSDC, DefWindowProcW, 0, 0,
                          GetModuleHandleW(nullptr), nullptr, nullptr, nullptr, nullptr,
                          L"DXGIDummy", nullptr};
        RegisterClassExW(&wc);
        HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                                  0, 0, 100, 100, nullptr, nullptr, wc.hInstance, nullptr);

        ID3D12Device* dev = nullptr;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
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
            factory->Release(); queue->Release(); dev->Release();
            DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
            return false;
        }

        void** vtable = *reinterpret_cast<void***>(swap);
        *out = vtable[8];

        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Present=%p", *out);

        swap->Release(); factory->Release(); queue->Release(); dev->Release();
        DestroyWindow(hwnd); UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return true;
    }

    // ── Init ──
    inline bool initialize() {
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] === Initialize (DirectComposition overlay) ===");

        if (MH_Initialize() != MH_OK) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] MH_Initialize failed");
            return false;
        }

        void* addr = nullptr;
        if (!get_present_addr(&addr)) return false;

        MH_STATUS s = MH_CreateHook(addr, &hkPresent, reinterpret_cast<void**>(&oPresent));
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Hook result=%d", s);
        if (s != MH_OK) return false;

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX_HOOK] Hook enabled");
        return true;
    }

} // namespace dx_hook
