#include <windows.h>
#include <gdiplus.h>
#include <string>

using namespace Gdiplus;

// Global/External states from the original binary
extern ULONG_ONLY DAT_140011000;
extern BOOL  g_MacroEnabled;       // DAT_140011248: ON/OFF State
extern BOOL  g_MacroPaused;        // DAT_1400111ec: READY/PAUSED State
extern DWORD g_InputType;          // DAT_140011078: 1=Left Click, 2=Right Click, Else=Virtual Key
extern double g_TargetCPS;         // DAT_140011080: Target Clicks Per Second
extern int   g_CurrentCPS;         // DAT_1400113e8: Current measured CPS value
extern __int64 DAT_1400111c8;      // QueryPerformanceFrequency cache
extern LONG  g_TotalClicks;        // DAT_140011814: Safe click counter

// Forward declarations of missing helper functions
GraphicsPath* CreateRoundedRectPath(float x, float y, float width, float height, float radius); 
void FormatCpsString(char* buffer, const char* format, int cpsValue, int unknownParam); 
void UpdateCpsStats(); 

// --- OVERLAY RENDERING FUNCTION ---
void DrawOverlay(HWND hWnd)
{
    if (hWnd == NULL) return;

    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int width = rcClient.right;
    int height = rcClient.bottom;

    // Set up 32-bit ARGB Bitmap Info for Layered Window
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hdcScreen = CreateCompatibleDC(NULL);
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HGDIOBJ hOldBmp = SelectObject(hdcScreen, hBitmap);

    // Initialize GDI+ Graphics
    Graphics* graphics = new Graphics(hdcScreen);
    graphics->SetSmoothingMode(SmoothingModeAntiAlias);
    graphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics->Clear(Color(0, 0, 0, 0)); // Transparent canvas

    // Draw UI Container Background (Dark grey theme)
    GraphicsPath* panelPath = CreateRoundedRectPath(0.0f, 0.0f, (float)width, (float)height, 12.0f);
    LinearGradientBrush bgBrush(Point(0, 0), Point(width, height), Color(0xFF, 0x0D, 0x11, 0x17), Color(0xFF, 0x16, 0x1B, 0x22));
    graphics->FillPath(&bgBrush, panelPath);

    // Draw Semi-transparent outer neon border
    Pen outerBorderPen(Color(0x30, 0x22, 0xFF, 0x55), 1.0f);
    graphics->DrawPath(&outerBorderPen, panelPath);
    
    if (panelPath != nullptr) {
        delete panelPath;
    }

    // Draw solid inner neon line
    Pen innerBorderPen(Color(0xFF, 0x22, 0xFF, 0x55), 2.0f);
    graphics->DrawLine(&innerBorderPen, 12, 1, width - 12, 1);

    // Load and Render window icon
    HMODULE hInst = GetModuleHandleW(NULL);
    HICON hIcon = (HICON)LoadImageW(hInst, L"IDI_ICON1", IMAGE_ICON, 14, 14, 0);
    if (hIcon != NULL) {
        DrawIconEx(hdcScreen, 14, 10, hIcon, 14, 14, 0, NULL, DI_NORMAL);
        DestroyIcon(hIcon);
    }

    // Draw Title Text "BLAZE MACRO" letter-by-letter for custom spacing
    FontFamily orbitronFont(L"Orbitron");
    Font titleFont(&orbitronFont, 16.0f, FontStyleBold, UnitPixel);
    SolidBrush textShadowBrush(Color(0x80, 0x22, 0xFF, 0x55));
    
    float currentX = 34.0f;
    const wchar_t* titleStr = L"BLAZE MACRO";
    for (int i = 0; titleStr[i] != L'\0'; i++) {
        wchar_t letter[2] = { titleStr[i], L'\0' };
        PointF origin(currentX, 10.0f);
        graphics->DrawString(letter, 1, &titleFont, origin, &textShadowBrush);
        
        RectF layoutRect;
        graphics->MeasureString(letter, 1, &titleFont, origin, &layoutRect);
        currentX += layoutRect.Width; // Advance cursor position
    }

    // Draw Status State (ON/OFF)
    Color statusColor = g_MacroEnabled ? Color(0xFF, 0x22, 0xFF, 0x55) : Color(0xFF, 0xFF, 0x44, 0x44);
    SolidBrush statusBrush(statusColor);
    
    const char* statusTextA = g_MacroEnabled ? "ON" : "OFF";
    wchar_t statusTextW[8];
    MultiByteToWideChar(CP_ACP, 0, statusTextA, -1, statusTextW, 8);
    
    Font statusFont(&orbitronFont, 27.0f, FontStyleBold, UnitPixel);
    graphics->DrawString(statusTextW, -1, &statusFont, PointF(22.0f, 32.0f), &statusBrush);

    // Format & Draw CPS Text
    FontFamily consolasFont(L"Consolas");
    Font cpsNumFont(&consolasFont, 12.0f, FontStyleBold, UnitPixel);
    
    char cpsBufferA[32];
    FormatCpsString(cpsBufferA, "%d", g_CurrentCPS, 2); // Internal string formatter lookup
    wchar_t cpsBufferW[16];
    MultiByteToWideChar(CP_ACP, 0, cpsBufferA, -1, cpsBufferW, 16);

    Color cpsNumColor = g_MacroEnabled ? Color(0xFF, 0x22, 0xFF, 0x55) : Color(0xFF, 0x8B, 0x94, 0x9E);
    SolidBrush cpsNumBrush(cpsNumColor);
    graphics->DrawString(cpsBufferW, -1, &cpsNumFont, PointF(14.0f, 58.0f), &cpsNumBrush);

    // Measure Dynamic size to append " CPS" label perfectly next to the number
    RectF cpsNumSize;
    graphics->MeasureString(cpsBufferW, -1, &cpsNumFont, PointF(0, 0), &cpsNumSize);
    
    SolidBrush cpsLabelBrush(Color(0xFF, 0x8B, 0x94, 0x9E));
    graphics->DrawString(L" CPS", 4, &cpsNumFont, PointF(cpsNumSize.Width + 14.0f - 2.0f, 58.0f), &cpsLabelBrush);

    // Draw dividing structural vertical line
    Pen dividerPen(Color(0x15, 0x22, 0xFF, 0x55), 1.0f);
    graphics->DrawLine(&dividerPen, 14, 80, width - 14, 80);

    // Draw Activity State Indicator (READY/PAUSED)
    FontFamily rajdhaniFont(L"Rajdhani");
    Font stateFont(&rajdhaniFont, 11.0f, FontStyleBold, UnitPixel);
    
    Color stateColor = g_MacroPaused ? Color(0xFF, 0xFF, 0x44, 0x44) : Color(0xFF, 0x22, 0xFF, 0x55);
    SolidBrush stateBrush(stateColor);

    const char* stateTextA = g_MacroPaused ? "PAUSED" : "READY";
    wchar_t stateTextW[16];
    MultiByteToWideChar(CP_ACP, 0, stateTextA, -1, stateTextW, 16);
    graphics->DrawString(stateTextW, -1, &stateFont, PointF(14.0f, 88.0f), &stateBrush);

    // Clean up graphics objects
    delete graphics;

    // Update Windows Layered Window Composition
    HDC hdcDst = GetDC(NULL);
    POINT ptSrc = { 0, 0 };
    SIZE sizeWindow = { width, height };
    
    RECT rcWindow;
    GetWindowRect(hWnd, &rcWindow);
    POINT ptDst = { rcWindow.left, rcWindow.top };

    BLENDFUNCTION blend;
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 220; // 0xdc: Subtle window transparency 
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hWnd, hdcDst, &ptDst, &sizeWindow, hdcScreen, &ptSrc, 0, &blend, ULW_ALPHA);
    
    // Release GDI context handles
    ReleaseDC(NULL, hdcDst);
    SelectObject(hdcScreen, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcScreen);
}

// --- CORE INPUT LOOP THREAD ---
void MacroThreadProc(void)
{
    // Bind execution to CPU Cores 4 & 5 for timing determinism
    SetThreadAffinityMask(GetCurrentThread(), 0x30);
    timeBeginPeriod(1);

    if (DAT_1400111c8 == 0) {
        QueryPerformanceFrequency((LARGE_INTEGER*)&DAT_1400111c8);
    }

    LARGE_INTEGER lintCounter;
    QueryPerformanceCounter(&lintCounter);
    double lastCpsCheckTime = ((double)lintCounter.QuadPart / (double)DAT_1400111c8) * 1000.0;

    while (true) 
    {
        // Thread Block: Idle spin wait while macro toggle is disabled
        while (!g_MacroEnabled) {
            Sleep(5);
        }

        // Action Loop execution phase
        do {
            // Trigger configured input
            if (g_InputType == 1) { // Left Click
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
            else if (g_InputType == 2) { // Right Click
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            }
            else { // Emulate Hardware Keyboard Stroke
                UINT vKey = g_InputType;
                UINT scanCode = MapVirtualKeyW(vKey, MAPVK_VK_TO_VSC);
                keybd_event((BYTE)vKey, (BYTE)scanCode, 0, 0);
                keybd_event((BYTE)vKey, (BYTE)scanCode, KEYEVENTF_KEYUP, 0);
            }

            // Thread-safe update to global stats counters
            InterlockedIncrement(&g_TotalClicks);

            // Periodically refresh/calculate CPS every 5000 milliseconds
            QueryPerformanceCounter(&lintCounter);
            double currentTime = ((double)lintCounter.QuadPart / (double)DAT_1400111c8) * 1000.0;
            if ((currentTime - lastCpsCheckTime) > 5000.0) {
                UpdateCpsStats();
                lastCpsCheckTime = currentTime;
            }

            // Accurate High-Precision Micro-Sleep Handling to align with TargetCPS
            double targetDelayMs = 1000.0 / g_TargetCPS;
            
            LARGE_INTEGER lintLoopStart;
            QueryPerformanceCounter(&lintLoopStart);
            double loopStartTimeFrame = (double)lintLoopStart.QuadPart;
            double perfFrequency = (double)DAT_1400111c8;

            // Coarse sleep fallback if target interval allows significant headroom
            if (targetDelayMs > 2.0) {
                Sleep((DWORD)(targetDelayMs - 2.0));
            }

            // High precision query spinning for remaining fractional delay windows
            LARGE_INTEGER lintLoopCurrent;
            do {
                QueryPerformanceCounter(&lintLoopCurrent);
                double elapsedMs = ((double)lintLoopCurrent.QuadPart / perfFrequency) * 1000.0 - 
                                   (loopStartTimeFrame / perfFrequency) * 1000.0;
                
                if (elapsedMs >= targetDelayMs) {
                    break;
                }
            } while (g_MacroEnabled); // Immediately drop out if user toggles off macro mid-delay

        } while (g_MacroEnabled);
    }
}