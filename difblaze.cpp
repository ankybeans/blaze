#include <windows.h>
#include <gdiplus.h>
#include <string>

using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winmm.lib") // For timeBeginPeriod

// --- Global Variables (Guessed from Ghidra DAT_ addresses) ---
extern ULONG_PTR g_gdiplusToken;
extern BOOL      g_IsMacroEnabled;   // DAT_140011248
extern BOOL      g_IsMacroPaused;    // DAT_1400111ec
extern DWORD     g_InputType;        // DAT_140011078 (1 = Left Click, 2 = Right Click, otherwise Virtual Key)
extern double    g_TargetCPS;        // DAT_140011080 (Clicks Per Second)
extern INT64     g_PerfFrequency;    // DAT_1400111c8
extern volatile LONG g_ClickCounter; // DAT_140011814
extern DWORD     g_CpsDisplayValue;  // DAT_1400113e8

// --- External Helper Functions ---
extern GraphicsPath* CreateRoundedRectPath(float x, float y, float width, float height, float radius); // FUN_140001b60
extern FontFamily* LoadCustomFont(LPCWSTR fontName);                                                 // FUN_140001d10
extern void          ResetCpsCounter();                                                                // FUN_1400011d0
extern void          FormatCpsString(char* buffer, const char* format, DWORD cpsValue, int unknown);   // FUN_140001110


// ============================================================================
// FUN_140001e30 -> UI Overlay Rendering Logic
// ============================================================================
void DrawOverlay(HWND hWnd)
{
    if (hWnd == NULL) return;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    int width = clientRect.right;
    int height = clientRect.bottom;

    // Set up a standard 32-bit ARGB Bitmap Info header for layered window drawing
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down bitmap
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HDC hdcScreen = CreateCompatibleDC(NULL);
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HGDIOBJ hOldBmp = SelectObject(hdcScreen, hBitmap);

    // Initialize GDI+ Graphics object from our memory DC
    Graphics* graphics = new Graphics(hdcScreen);
    graphics->SetSmoothingMode(SmoothingModeAntiAlias);
    graphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
    graphics->Clear(Color(0, 0, 0, 0)); // Transparent clear

    // 1. Draw Window Background (Rounded rectangle with a linear gradient)
    GraphicsPath* bgPath = CreateRoundedRectPath(0.0f, 0.0f, (float)width, (float)height, 12.0f);
    LinearGradientBrush bgBrush(
        Point(0, 0), 
        Point(width, height), 
        Color(255, 13, 17, 23),  // #0d1117 (GitHub dark theme style)
        Color(255, 22, 27, 34)   // #161b22
    );
    graphics->FillPath(&bgBrush, bgPath);

    // Draw background subtle border outline
    Pen borderPen(Color(0x30, 0x22, 0xFF, 0x55), 1.0f); // Semi-transparent lime green
    graphics->DrawPath(&borderPen, bgPath);
    
    if (bgPath != nullptr) {
        delete bgPath;
    }

    // 2. Draw Separator Accent Line
    Pen accentPen(Color(0xFF, 0x22, 0xFF, 0x55), 2.0f); // Solid lime green
    graphics->DrawLine(&accentPen, 12, 1, width - 12, 1);

    // 3. Draw Application Icon
    HMODULE hInst = GetModuleHandleW(NULL);
    HICON hIcon = (HICON)LoadImageW(hInst, L"IDI_ICON1", IMAGE_ICON, 14, 14, 0);
    if (hIcon != NULL) {
        DrawIconEx(hdcScreen, 14, 10, hIcon, 14, 14, 0, NULL, DI_NORMAL);
        DestroyIcon(hIcon);
    }

    // 4. Render Title: "BLAZE MACRO" (Character by character with custom tracking)
    FontFamily* titleFontFamily = LoadCustomFont(L"Orbitron");
    Font titleFont(titleFontFamily, 16.0f, FontStyleBold, UnitPixel);
    SolidBrush textBrush(Color(0x80, 0x22, 0xFF, 0x55)); // Transparent-ish green

    std::wstring titleText = L"BLAZE MACRO";
    float currentX = 34.0f;
    for (size_t i = 0; i < titleText.length(); ++i) {
        wchar_t ch[2] = { titleText[i], L'\0' };
        PointF textPt(currentX, 14.0f);
        graphics->DrawString(ch, 1, &titleFont, textPt, &textBrush);

        // Measure char width dynamically to step X position forward
        RectF layoutRect, boundingBox;
        graphics->MeasureString(ch, 1, &titleFont, textPt, &boundingBox);
        currentX += boundingBox.Width; // Equivalent to Ghidra's loop width stepping logic
    }

    if (titleFontFamily != nullptr) {
        delete titleFontFamily;
    }

    // 5. Draw status tag: "ON" or "OFF"
    FontFamily* statusFontFamily = LoadCustomFont(L"Orbitron");
    Font statusFont(statusFontFamily, 22.0f, FontStyleBold, UnitPixel);
    
    Color statusColor = g_IsMacroEnabled ? Color(0xFF, 0x22, 0xFF, 0x55) : Color(0xFF, 0xFF, 0x44, 0x44);
    SolidBrush statusBrush(statusColor);

    WCHAR statusBuffer[8];
    MultiByteToWideChar(CP_ACP, 0, g_IsMacroEnabled ? "ON" : "OFF", -1, statusBuffer, 8);
    
    // Position text on the upper right side
    PointF statusPt((float)(width - 60), 14.0f); 
    graphics->DrawString(statusBuffer, -1, &statusFont, statusPt, &statusBrush);

    if (statusFontFamily != nullptr) {
        delete statusFontFamily;
    }

    // 6. Draw CPS Counter Display Text
    FontFamily* monoFontFamily = LoadCustomFont(L"Consolas");
    Font statsFont(monoFontFamily ? monoFontFamily : FontFamily::GenericMonospace(), 14.0f, FontStyleRegular, UnitPixel);
    
    char asciiCps[32];
    WCHAR wideCps[16];
    // Formats dynamic CPS variable from data sections
    FormatCpsString(asciiCps, "%d", g_CpsDisplayValue, 2); 
    MultiByteToWideChar(CP_ACP, 0, asciiCps, -1, wideCps, 16);

    Color cpsTextColor = g_IsMacroEnabled ? Color(0xFF, 0x22, 0xFF, 0x55) : Color(0xFF, 0x8B, 0x94, 0x9E);
    SolidBrush cpsBrush(cpsTextColor);
    
    PointF cpsPt(14.0f, 40.0f);
    graphics->DrawString(wideCps, -1, &statsFont, cpsPt, &cpsBrush);

    // Measure string layout size to app-end " CPS" label accurately next to the value
    RectF cpsBoundingBox;
    graphics->MeasureString(wideCps, -1, &statsFont, cpsPt, &cpsBoundingBox);

    SolidBrush labelBrush(Color(255, 0x8B, 0x94, 0x9E)); // Ash-gray label color
    PointF cpsLabelPt(cpsBoundingBox.Width + 14.0f, 40.0f);
    graphics->DrawString(L" CPS", 4, &statsFont, cpsLabelPt, &labelBrush);

    if (monoFontFamily != nullptr) {
        delete monoFontFamily;
    }

    // 7. Draw State Indicators: "READY" or "PAUSED"
    Pen indicatorLinePen(Color(0x15, 0x22, 0xFF, 0x55), 1.0f);
    graphics->DrawLine(&indicatorLinePen, 14, 80, width - 14, 80);

    FontFamily* stateFontFamily = LoadCustomFont(L"Rajdhani");
    Font stateFont(stateFontFamily, 12.0f, FontStyleBold, UnitPixel);
    
    Color stateColor = g_IsMacroPaused ? Color(0xFF, 0xFF, 0x44, 0x44) : Color(0xFF, 0x22, 0xFF, 0x55);
    SolidBrush stateBrush(stateColor);

    WCHAR stateBuffer[16];
    MultiByteToWideChar(CP_ACP, 0, g_IsMacroPaused ? "PAUSED" : "READY", -1, stateBuffer, 16);
    graphics->DrawString(stateBuffer, -1, &stateFont, PointF(14.0f, 85.0f), &stateBrush);

    if (stateFontFamily != nullptr) {
        delete stateFontFamily;
    }

    // 8. Cleanup and Update Layered Window Graphics
    delete graphics;

    HDC hdcDst = GetDC(NULL);
    POINT ptSrc = { 0, 0 };
    POINT ptDst = { 0, 0 };
    SIZE sizeWnd = { width, height };

    RECT rectWnd;
    GetWindowRect(hWnd, &rectWnd);
    ptDst.x = rectWnd.left;
    ptDst.y = rectWnd.top;

    BLENDFUNCTION blend = { 0 };
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = 220; // 0xdc -> Opacity level
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hWnd, hdcDst, &ptDst, &sizeWnd, hdcScreen, &ptSrc, 0, &blend, ULW_ALPHA);

    ReleaseDC(NULL, hdcDst);
    SelectObject(hdcScreen, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcScreen);
}


// ============================================================================
// FUN_1400017f0 -> Engine / High Performance Clicker Thread Loop
// ============================================================================
DWORD WINAPI MacroLoopThread(LPVOID lpParam)
{
    // Optimize affinity mask to dedicated CPU cores for microsecond accuracy stability
    SetThreadAffinityMask(GetCurrentThread(), 0x30);
    timeBeginPeriod(1);

    if (g_PerfFrequency == 0) {
        QueryPerformanceFrequency((LARGE_INTEGER*)&g_PerfFrequency);
    }

    LARGE_INTEGER perfCounter;
    QueryPerformanceCounter(&perfCounter);
    double lastResetTime = ((double)perfCounter.QuadPart / (double)g_PerfFrequency) * 1000.0;

    while (true)
    {
        // Thread block loop waiting until Macro is toggled ON
        while (!g_IsMacroEnabled) {
            Sleep(5);
        }

        do {
            // Send configured Input Event
            if (g_InputType == 1) {
                // Left click mouse down + up
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
            else if (g_InputType == 2) {
                // Right click mouse down + up
                mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
            }
            else {
                // Map out virtual key layouts to keyboard events
                UINT vKey = MapVirtualKeyW(g_InputType, MAPVK_VK_TO_VSC);
                keybd_event((BYTE)g_InputType, (BYTE)vKey, 0, 0);
                keybd_event((BYTE)g_InputType, (BYTE)vKey, KEYEVENTF_KEYUP, 0);
            }

            // Thread-safe update of current macro calculations
            InterlockedIncrement(&g_ClickCounter);

            QueryPerformanceCounter(&perfCounter);
            double currentTime = ((double)perfCounter.QuadPart / (double)g_PerfFrequency) * 1000.0;

            // Every 5000 milliseconds (5 seconds), calculate and reset CPS window
            if ((currentTime - lastResetTime) > 5000.0) {
                ResetCpsCounter();
                lastResetTime = currentTime;
            }

            // Calculate precise micro-delay based on targeted CPS
            double targetIntervalMs = 1000.0 / g_TargetCPS;

            LARGE_INTEGER loopStartCounter;
            QueryPerformanceCounter(&loopStartCounter);
            double loopStartTimeFrame = (double)loopStartCounter.QuadPart;
            double frequencyDouble = (double)g_PerfFrequency;

            // Hybrid high precision sleep layout: OS Sleep for bulk time, busy-wait for sub-millisecond precision
            if (targetIntervalMs > 2.0) {
                Sleep((DWORD)(targetIntervalMs - 2.0));
            }

            LARGE_INTEGER currentWaitCounter;
            do {
                QueryPerformanceCounter(&currentWaitCounter);
                double deltaMs = ((double)currentWaitCounter.QuadPart / frequencyDouble) * 1000.0 - 
                                 (loopStartTimeFrame / frequencyDouble) * 1000.0;
                
                if (deltaMs >= targetIntervalMs) {
                    break;
                }
            } while (g_IsMacroEnabled);

        } while (g_IsMacroEnabled);
    }

    timeEndPeriod(1);
    return 0;
}