// main.cpp
#include <windows.h>
#include <string>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

// ====================== STATE ======================
struct UIState
{
    bool enabled = false;
    bool paused = false;

    int cps = 250;

    int mode = 0; // 0 = Toggle, 1 = Hold

    std::string toggleKey = "E";
    std::string spamKey = "F";
};

UIState g_State;

// ====================== UI RENDER ======================
void RenderBlazeUI()
{
    ImGui::SetNextWindowSize(ImVec2(520, 320), ImGuiCond_Always);

    ImGui::Begin("BLAZE MACRO",
        nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse);

    // ================= OV (preview area) =================
    ImGui::Text("BLAZE MACRO");

    ImGui::Spacing();

    ImGui::Text("State: %s", g_State.enabled ? "ON" : "OFF");
    ImGui::Text("CPS: %d", g_State.cps);
    ImGui::Text("Mode: %s", g_State.mode == 0 ? "Toggle" : "Hold");

    ImGui::Separator();

    // ================= SETTINGS PANEL =================
    ImGui::Text("SETTINGS");

    ImGui::InputInt("CPS", &g_State.cps);

    ImGui::Text("SPAM KEY: %s", g_State.spamKey.c_str());
    ImGui::Text("TOGGLE KEY: %s", g_State.toggleKey.c_str());

    if (ImGui::RadioButton("Toggle", g_State.mode == 0))
        g_State.mode = 0;

    ImGui::SameLine();

    if (ImGui::RadioButton("Hold", g_State.mode == 1))
        g_State.mode = 1;

    ImGui::Separator();

    // ================= ACTIONS =================
    if (ImGui::Button(g_State.enabled ? "TURN OFF" : "TURN ON"))
        g_State.enabled = !g_State.enabled;

    ImGui::SameLine();

    if (ImGui::Button("SHUT DOWN"))
        PostQuitMessage(0);

    ImGui::Spacing();

    ImGui::TextDisabled("Made by slipail");

    ImGui::End();
}

// ====================== WIN32 SETUP ======================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            // resize handled by ImGui backend
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ====================== MAIN ======================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0, 0,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        L"BlazeMacro", NULL };

    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        L"BLAZE MACRO",
        WS_OVERLAPPEDWINDOW,
        100, 100, 600, 400,
        NULL, NULL,
        wc.hInstance,
        NULL
    );

    // ================= D3D DEVICE =================
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGISwapChain* swapChain = nullptr;

    // (Swapchain setup omitted for brevity if you already use ImGui DX11 backend)

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // ================= IMGUI INIT =================
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    // ImGui_ImplDX11_Init(device, context);

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // ImGui frame start
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderBlazeUI();

        ImGui::Render();

        // render backend would go here
    }

    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}