// Main.cpp - ARK: Survival Ascended Cheat with Modern GUI
#include "ArkCheat.h"

// External from hooks
extern "C" {
    extern volatile uintptr_t g_CapturedRAX;
    extern volatile float g_CapturedHP;
}

// ============== GUI GLOBALS ==============
int g_CurrentTab = 0;
bool g_Dragging = false;
int g_DragX = 0, g_DragY = 0;

// Colors - Dark purple/blue theme
#define COL_BG          RGB(18, 18, 28)
#define COL_HEADER      RGB(28, 28, 42)
#define COL_TAB         RGB(38, 38, 55)
#define COL_TAB_ACTIVE  RGB(88, 101, 242)
#define COL_PANEL       RGB(32, 32, 48)
#define COL_TOGGLE_ON   RGB(67, 181, 129)
#define COL_TOGGLE_OFF  RGB(55, 55, 75)
#define COL_TEXT        RGB(220, 220, 235)
#define COL_TEXT_DIM    RGB(130, 130, 160)
#define COL_ACCENT      RGB(88, 101, 242)
#define COL_RED         RGB(240, 71, 71)
#define COL_GREEN       RGB(67, 181, 129)

// Tab indices
#define TAB_PLAYER    0
#define TAB_COMBAT    1
#define TAB_MISC      2
#define TAB_SETTINGS  3

// ============== DRAWING HELPERS ==============
void FillRoundRect(HDC hdc, int x, int y, int w, int h, int r, COLORREF col) {
    HBRUSH brush = CreateSolidBrush(col);
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    SelectObject(hdc, brush);
    SelectObject(hdc, pen);
    RoundRect(hdc, x, y, x + w, y + h, r, r);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawText2(HDC hdc, const char* text, int x, int y, COLORREF col, int size, bool bold = false) {
    HFONT font = CreateFontA(size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x, y, text, (int)strlen(text));
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

void DrawToggle(HDC hdc, int x, int y, bool on, const char* label) {
    // Toggle background
    FillRoundRect(hdc, x, y, 44, 22, 11, on ? COL_TOGGLE_ON : COL_TOGGLE_OFF);

    // Toggle circle
    int cx = on ? (x + 26) : (x + 4);
    HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255));
    HPEN wp = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    SelectObject(hdc, wb);
    SelectObject(hdc, wp);
    Ellipse(hdc, cx, y + 3, cx + 16, y + 19);
    DeleteObject(wb);
    DeleteObject(wp);

    // Label
    DrawText2(hdc, label, x + 55, y + 2, COL_TEXT, 14);
}

void DrawButton(HDC hdc, int x, int y, int w, int h, const char* text, COLORREF col) {
    FillRoundRect(hdc, x, y, w, h, 6, col);

    HFONT font = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
    SelectObject(hdc, font);

    SIZE sz;
    GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);
    TextOutA(hdc, x + (w - sz.cx) / 2, y + (h - sz.cy) / 2, text, (int)strlen(text));
    DeleteObject(font);
}

void DrawProgressBar(HDC hdc, int x, int y, int w, int h, float value, float maxVal, COLORREF col) {
    // Background
    FillRoundRect(hdc, x, y, w, h, 4, COL_TOGGLE_OFF);

    // Fill
    float pct = (maxVal > 0) ? (value / maxVal) : 0;
    if (pct > 1.0f) pct = 1.0f;
    int fillW = (int)(w * pct);
    if (fillW > 4) {
        FillRoundRect(hdc, x, y, fillW, h, 4, col);
    }
}

// ============== GUI DRAW ==============
void DrawGUI(HDC hdc, int width, int height) {
    // Background
    HBRUSH bg = CreateSolidBrush(COL_BG);
    RECT r = {0, 0, width, height};
    FillRect(hdc, &r, bg);
    DeleteObject(bg);

    // Header
    FillRoundRect(hdc, 0, 0, width, 55, 0, COL_HEADER);
    DrawText2(hdc, "ARK ASCENDED", 20, 10, COL_ACCENT, 24, true);
    DrawText2(hdc, "Internal Cheat v1.0", 20, 35, COL_TEXT_DIM, 12);

    // Status indicator
    uintptr_t player = g_PlayerBase.load();
    const char* status = player ? "ATTACHED" : "WAITING...";
    COLORREF statusCol = player ? COL_GREEN : COL_RED;
    DrawText2(hdc, status, width - 90, 20, statusCol, 12, true);

    // Tabs
    const char* tabs[] = {"Player", "Combat", "Misc", "Settings"};
    int tabW = (width - 20) / 4;
    for (int i = 0; i < 4; i++) {
        int tx = 10 + i * tabW;
        FillRoundRect(hdc, tx, 65, tabW - 5, 30, 6, i == g_CurrentTab ? COL_TAB_ACTIVE : COL_TAB);

        HFONT f = CreateFontA(13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, "Segoe UI");
        SelectObject(hdc, f);
        SIZE sz;
        GetTextExtentPoint32A(hdc, tabs[i], (int)strlen(tabs[i]), &sz);
        SetTextColor(hdc, i == g_CurrentTab ? RGB(255,255,255) : COL_TEXT_DIM);
        SetBkMode(hdc, TRANSPARENT);
        TextOutA(hdc, tx + (tabW - 5 - sz.cx) / 2, 72, tabs[i], (int)strlen(tabs[i]));
        DeleteObject(f);
    }

    // Content
    int cy = 110;
    int cx = 15;
    int cw = width - 30;

    switch (g_CurrentTab) {
        case TAB_PLAYER: {
            // HP Display
            FillRoundRect(hdc, cx, cy, cw, 80, 8, COL_PANEL);
            DrawText2(hdc, "HEALTH", cx + 15, cy + 10, COL_TEXT_DIM, 11, true);

            float hp = GetHP();
            float maxHP = g_MaxHP.load();
            if (maxHP < 1.0f) maxHP = 100.0f;

            char hpText[64];
            sprintf(hpText, "%.0f / %.0f", hp, maxHP);
            DrawText2(hdc, hpText, cx + 15, cy + 30, COL_TEXT, 18, true);
            DrawProgressBar(hdc, cx + 15, cy + 55, cw - 30, 12, hp, maxHP, COL_GREEN);

            cy += 95;

            // God Mode
            FillRoundRect(hdc, cx, cy, cw, 55, 8, COL_PANEL);
            DrawToggle(hdc, cx + 15, cy + 16, g_GodMode, "God Mode (Infinite HP)");

            cy += 65;

            // Heal button
            DrawButton(hdc, cx, cy, cw, 40, "FULL HEAL", COL_ACCENT);
            break;
        }

        case TAB_COMBAT: {
            FillRoundRect(hdc, cx, cy, cw, 60, 8, COL_PANEL);
            DrawText2(hdc, "Combat features coming soon...", cx + 15, cy + 20, COL_TEXT_DIM, 14);
            DrawText2(hdc, "Find more addresses in Cheat Engine", cx + 15, cy + 38, COL_TEXT_DIM, 12);
            break;
        }

        case TAB_MISC: {
            FillRoundRect(hdc, cx, cy, cw, 55, 8, COL_PANEL);
            DrawToggle(hdc, cx + 15, cy + 16, g_InfiniteStamina, "Infinite Stamina");

            cy += 65;

            FillRoundRect(hdc, cx, cy, cw, 55, 8, COL_PANEL);
            DrawToggle(hdc, cx + 15, cy + 16, g_InfiniteWeight, "Infinite Weight");
            break;
        }

        case TAB_SETTINGS: {
            FillRoundRect(hdc, cx, cy, cw, 90, 8, COL_PANEL);
            DrawText2(hdc, "HOTKEYS", cx + 15, cy + 10, COL_TEXT_DIM, 11, true);
            DrawText2(hdc, "F1 - Toggle Menu", cx + 15, cy + 35, COL_TEXT, 13);
            DrawText2(hdc, "END - Exit Cheat", cx + 15, cy + 55, COL_TEXT, 13);

            cy += 105;

            DrawButton(hdc, cx, cy, cw, 40, "EXIT CHEAT", COL_RED);
            break;
        }
    }

    // Footer
    DrawText2(hdc, "Press F1 to toggle menu", 15, height - 25, COL_TEXT_DIM, 11);
}

// ============== CLICK HANDLING ==============
void HandleClick(HWND hwnd, int x, int y) {
    int width = 360;
    int tabW = (width - 20) / 4;

    // Tab clicks
    if (y >= 65 && y <= 95) {
        for (int i = 0; i < 4; i++) {
            int tx = 10 + i * tabW;
            if (x >= tx && x <= tx + tabW - 5) {
                g_CurrentTab = i;
                InvalidateRect(hwnd, NULL, FALSE);
                return;
            }
        }
    }

    int cx = 15;
    int cw = width - 30;

    switch (g_CurrentTab) {
        case TAB_PLAYER: {
            // God Mode toggle (y ~= 221)
            if (y >= 205 && y <= 250 && x >= cx + 15 && x <= cx + 60) {
                g_GodMode = !g_GodMode;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            // Heal button (y ~= 270)
            if (y >= 270 && y <= 310 && x >= cx && x <= cx + cw) {
                float maxHP = g_MaxHP.load();
                if (maxHP < 1.0f) maxHP = 100.0f;
                SetHP(maxHP);
            }
            break;
        }

        case TAB_MISC: {
            // Infinite Stamina toggle
            if (y >= 110 && y <= 165 && x >= cx + 15 && x <= cx + 60) {
                g_InfiniteStamina = !g_InfiniteStamina;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            // Infinite Weight toggle
            if (y >= 175 && y <= 230 && x >= cx + 15 && x <= cx + 60) {
                g_InfiniteWeight = !g_InfiniteWeight;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }

        case TAB_SETTINGS: {
            // Exit button
            if (y >= 215 && y <= 255) {
                g_Running = false;
            }
            break;
        }
    }
}

// ============== WINDOW PROC ==============
LRESULT CALLBACK GUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
            SelectObject(memDC, memBmp);

            DrawGUI(memDC, rect.right, rect.bottom);

            BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            if (y < 55) {
                g_Dragging = true;
                g_DragX = x;
                g_DragY = y;
                SetCapture(hwnd);
            } else {
                HandleClick(hwnd, x, y);
            }
            return 0;
        }

        case WM_LBUTTONUP:
            g_Dragging = false;
            ReleaseCapture();
            return 0;

        case WM_MOUSEMOVE:
            if (g_Dragging) {
                POINT pt;
                GetCursorPos(&pt);
                SetWindowPos(hwnd, NULL, pt.x - g_DragX, pt.y - g_DragY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return 0;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            g_GUIVisible = false;
            return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void CreateGUI() {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = GUIProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "ArkCheatGUI";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);

    g_GUIWnd = CreateWindowExA(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "ArkCheatGUI", NULL,
        WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - 360) / 2,
        (GetSystemMetrics(SM_CYSCREEN) - 380) / 2,
        360, 380,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    HRGN rgn = CreateRoundRectRgn(0, 0, 361, 381, 15, 15);
    SetWindowRgn(g_GUIWnd, rgn, TRUE);
}

// ============== MAIN THREAD ==============
DWORD WINAPI MainThread(LPVOID hModule) {
    // Wait for game to load
    Sleep(5000);

    // Console for debug
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);

    printf("=== ARK ASCENDED CHEAT ===\n\n");
    printf("Scanning for signatures...\n");

    // Find HP signature
    g_HPCodeAddr = FindPattern("ArkAscended.exe", Signatures::HP_Write, Signatures::HP_Write_Mask);
    printf("HP Write: %llX %s\n", (unsigned long long)g_HPCodeAddr, g_HPCodeAddr ? "[OK]" : "[FAIL]");

    if (!g_HPCodeAddr) {
        printf("\nERROR: Signature not found!\n");
        printf("Press END to exit...\n");
        while (!(GetAsyncKeyState(VK_END) & 1)) Sleep(100);
        FreeConsole();
        FreeLibraryAndExitThread((HMODULE)hModule, 0);
        return 0;
    }

    // Install HP hook
    printf("\nInstalling hooks...\n");
    g_HPTrampoline = CreateHPHookStub(g_HPCodeAddr + Signatures::HP_InstrLen);
    if (g_HPTrampoline && InstallHook(g_HPCodeAddr, g_HPTrampoline, g_HPOriginalBytes, Signatures::HP_InstrLen)) {
        printf("  HP hook -> OK\n");
    } else {
        printf("  HP hook -> FAILED\n");
    }

    printf("\nControls: F1 = Menu, END = Exit\n");
    printf("Take damage or heal to activate hook!\n\n");

    // Create GUI
    CreateGUI();

    // Main loop
    while (g_Running) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // F1 = Toggle menu
        if (GetAsyncKeyState(VK_F1) & 1) {
            g_GUIVisible = !g_GUIVisible;
            if (g_GUIWnd) {
                ShowWindow(g_GUIWnd, g_GUIVisible ? SW_SHOW : SW_HIDE);
                if (g_GUIVisible) SetForegroundWindow(g_GUIWnd);
            }
        }

        // END = Exit
        if (GetAsyncKeyState(VK_END) & 1) {
            g_Running = false;
        }

        // Update from hook captures
        uintptr_t capturedPlayer = g_CapturedRAX;
        if (capturedPlayer && capturedPlayer != g_PlayerBase.load()) {
            g_PlayerBase.store(capturedPlayer);
            printf("Player found: %llX\n", (unsigned long long)capturedPlayer);
        }

        float capturedHP = g_CapturedHP;
        if (capturedHP > 0) {
            g_CurrentHP.store(capturedHP);
            // Update max HP if current is higher
            if (capturedHP > g_MaxHP.load()) {
                g_MaxHP.store(capturedHP);
            }
        }

        // God mode - keep HP at max
        if (g_GodMode && g_PlayerBase.load()) {
            float maxHP = g_MaxHP.load();
            if (maxHP > 0) {
                SetHP(maxHP);
            }
        }

        // Refresh GUI
        if (g_GUIVisible && g_GUIWnd) {
            InvalidateRect(g_GUIWnd, NULL, FALSE);
        }

        Sleep(16);
    }

    // Cleanup
    printf("Cleaning up...\n");
    if (g_HPCodeAddr) {
        RemoveHook(g_HPCodeAddr, g_HPOriginalBytes, Signatures::HP_InstrLen);
    }
    if (g_HPTrampoline) {
        VirtualFree(g_HPTrampoline, 0, MEM_RELEASE);
    }
    if (g_GUIWnd) DestroyWindow(g_GUIWnd);

    printf("Done!\n");
    FreeConsole();
    FreeLibraryAndExitThread((HMODULE)hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, hModule, 0, NULL);
    }
    return TRUE;
}
