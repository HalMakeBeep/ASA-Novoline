#pragma once

// =============================================================
// DX12 Hook with D3D11On12 ImGui rendering.
// Hooks Present + ExecuteCommandLists on the game's DX12 swapchain.
// Uses D3D11On12 wrapper for ImGui rendering — this handles all
// resource state transitions internally, avoiding UE5.5 barrier conflicts.
// =============================================================

#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <MinHook.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"

#include "render.h"
#include "prisme_theme.h"
#include "imgui_menu.h"
#include "../core/console.h"
#include "../features/chams.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

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
    inline UINT g_buffer_count   = 0;

    // Game DX12 objects (captured)
    inline ID3D12Device*       g_d3d12_device  = nullptr;
    inline ID3D12CommandQueue* g_command_queue  = nullptr;
    inline ID3D12CommandQueue* g_render_queue   = nullptr;
    inline ID3D12CommandQueue* g_last_queue     = nullptr;

    // D3D11On12 objects
    inline ID3D11Device*        g_d3d11_device    = nullptr;
    inline ID3D11DeviceContext*  g_d3d11_context   = nullptr;
    inline ID3D11On12Device*     g_d3d11on12       = nullptr;
    inline ID3D11RenderTargetView** g_d3d11_rtvs   = nullptr;
    inline ID3D11Resource**         g_d3d11_buffers = nullptr;

    // ── ExecuteCommandLists hook — tracks the most recent DIRECT queue ──
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

            float sens = 1.0f;
            g_vmouse_x += dx * sens;
            g_vmouse_y += dy * sens;

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

    // ── Initialize D3D11On12 rendering ──
    inline bool init_dx12(IDXGISwapChain* swap) {
        DXGI_SWAP_CHAIN_DESC sc_desc;
        if (FAILED(swap->GetDesc(&sc_desc))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetDesc failed");
            return false;
        }
        g_game_hwnd    = sc_desc.OutputWindow;
        g_buffer_count = sc_desc.BufferCount;

        if (FAILED(swap->GetDevice(IID_PPV_ARGS(&g_d3d12_device)))) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] GetDevice failed");
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Device=%p HWND=%p Buffers=%u Format=%u",
            g_d3d12_device, g_game_hwnd, g_buffer_count, sc_desc.BufferDesc.Format);

        g_initialized = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] DX12 device captured, ImGui deferred to first F1");
        return true;
    }

    // ── Deferred ImGui init — called on first F1 press ──
    inline bool init_imgui(IDXGISwapChain* swap) {
        if (g_imgui_ready) return true;

        DXGI_SWAP_CHAIN_DESC sc_desc;
        if (FAILED(swap->GetDesc(&sc_desc))) return false;

        // Lock the render queue at the moment the user presses F1
        g_render_queue = g_last_queue ? g_last_queue : g_command_queue;
        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] Locked render queue: %p (at first F1)", g_render_queue);
        if (!g_render_queue) return false;

        // Create D3D11On12 device wrapping the game's DX12 device
        IUnknown* queues[] = { g_render_queue };
        D3D_FEATURE_LEVEL feature_level;
        HRESULT hr = D3D11On12CreateDevice(
            g_d3d12_device,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0,
            queues, 1,
            0,
            &g_d3d11_device, &g_d3d11_context, &feature_level
        );

        if (FAILED(hr) || !g_d3d11_device) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] D3D11On12CreateDevice FAILED: 0x%08X", (unsigned)hr);
            return false;
        }

        hr = g_d3d11_device->QueryInterface(IID_PPV_ARGS(&g_d3d11on12));
        if (FAILED(hr) || !g_d3d11on12) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] QueryInterface ID3D11On12Device FAILED: 0x%08X", (unsigned)hr);
            return false;
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] D3D11On12 device created (feature level 0x%X)", (unsigned)feature_level);

        // Create wrapped resources + RTVs for each backbuffer
        g_d3d11_buffers = new ID3D11Resource*[g_buffer_count]();
        g_d3d11_rtvs    = new ID3D11RenderTargetView*[g_buffer_count]();

        for (UINT i = 0; i < g_buffer_count; i++) {
            ID3D12Resource* backbuffer = nullptr;
            hr = swap->GetBuffer(i, IID_PPV_ARGS(&backbuffer));
            if (FAILED(hr) || !backbuffer) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] GetBuffer(%u) failed: 0x%08X", i, (unsigned)hr);
                return false;
            }

            D3D11_RESOURCE_FLAGS d3d11_flags = { D3D11_BIND_RENDER_TARGET };
            hr = g_d3d11on12->CreateWrappedResource(
                backbuffer, &d3d11_flags,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT,
                IID_PPV_ARGS(&g_d3d11_buffers[i])
            );
            backbuffer->Release();

            if (FAILED(hr) || !g_d3d11_buffers[i]) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] CreateWrappedResource(%u) failed: 0x%08X", i, (unsigned)hr);
                return false;
            }

            hr = g_d3d11_device->CreateRenderTargetView(g_d3d11_buffers[i], nullptr, &g_d3d11_rtvs[i]);
            if (FAILED(hr) || !g_d3d11_rtvs[i]) {
                dbg::log_ex(dbg::Level::Warn, dbg::Init,
                    "[DX12] CreateRenderTargetView(%u) failed: 0x%08X", i, (unsigned)hr);
                return false;
            }
        }

        dbg::log_ex(dbg::Level::Warn, dbg::Init,
            "[DX12] %u wrapped backbuffers + RTVs created", g_buffer_count);

        // Init ImGui
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

        // Init ImGui DX11 backend
        ImGui_ImplDX11_Init(g_d3d11_device, g_d3d11_context);

        g_imgui_ready = true;
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] ImGui initialized with D3D11On12");
        return true;
    }

    // ── Render one frame ──
    inline int g_render_count = 0;

    inline void render_frame(IDXGISwapChain* swap) {
        if (!render::show_menu) {
            g_vmouse_active = false;
            return;
        }

        // Deferred ImGui init — first time menu is shown
        if (!g_imgui_ready) {
            if (!init_imgui(swap)) {
                g_init_failed = true;
                dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] init_imgui FAILED on first F1");
                return;
            }
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

        if (g_render_count <= 3) {
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] render_frame #%d, idx=%u", g_render_count, idx);
        }

        // Acquire the wrapped backbuffer — D3D11On12 handles the state transition
        g_d3d11on12->AcquireWrappedResources(&g_d3d11_buffers[idx], 1);

        // Set render target
        g_d3d11_context->OMSetRenderTargets(1, &g_d3d11_rtvs[idx], nullptr);

        // ImGui rendering
        feed_imgui_input();
        ImGui::GetIO().MouseDrawCursor = true;

        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        imgui_menu::draw();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        // Release the wrapped backbuffer — transitions back to PRESENT state
        g_d3d11on12->ReleaseWrappedResources(&g_d3d11_buffers[idx], 1);

        // Flush D3D11 commands to the GPU
        g_d3d11_context->Flush();
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

        // Store chams vtable addresses for deferred hook creation
        if (draw_indexed_addr && ia_set_vb_addr && set_pso_addr && create_pso_addr) {
            features::chams::g_addr_draw_indexed = draw_indexed_addr;
            features::chams::g_addr_ia_set_vb    = ia_set_vb_addr;
            features::chams::g_addr_set_pso      = set_pso_addr;
            features::chams::g_addr_create_pso   = create_pso_addr;
            dbg::log_ex(dbg::Level::Warn, dbg::Init,
                "[DX12] Chams vtable addresses saved (activate from Visuals > Chams)");
        }

        MH_EnableHook(MH_ALL_HOOKS);
        dbg::log_ex(dbg::Level::Warn, dbg::Init, "[DX12] Hooks enabled");
        return true;
    }

} // namespace dx_hook
