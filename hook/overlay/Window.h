#pragma once
#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#include <atomic>

#include "Renderer.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_win32.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

class Window
{
public:
    void subclass(HWND hwnd);
    void restore();
    void setInteractive(bool interactive);

    HWND hwnd()         const { return m_hwnd; }
    bool menuVisible()  const { return m_menuVisible; }
    void setMenuVisible(bool v) { m_menuVisible = v; }

private:
    HWND    m_hwnd            = nullptr;
    WNDPROC m_originalWndProc = nullptr;
    bool    m_menuVisible     = true;

    static LRESULT CALLBACK wndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

inline Window window;
