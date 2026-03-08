#include "Window.h"
#include "../Log.h"

LRESULT CALLBACK Window::wndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_USER)
    {
        if (window.m_menuVisible)
            SetFocus(hwnd);
        return 0;
    }

    if (msg == WM_TIMER)
    {
        const LONG_PTR overlayObj = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        if (overlayObj)
        {
            *reinterpret_cast<BYTE*>(overlayObj + State::kVisibleFlagOffset)    = 1;
            *reinterpret_cast<BYTE*>(overlayObj + State::kForegroundFlagOffset) = 1;
            *reinterpret_cast<BYTE*>(overlayObj + State::kDirtyFlagOffset)      = 0;
        }
        return 0;
    }

    if (msg == WM_APP)
    {
        SetWindowPos(hwnd, nullptr, 0, 0,
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
        return 0;
    }

    if (msg == WM_SHOWWINDOW && wParam == FALSE)
        return 0;

    if (msg == WM_WINDOWPOSCHANGING)
    {
        auto* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (wp)
        {
            wp->flags = (wp->flags & ~SWP_HIDEWINDOW) | SWP_SHOWWINDOW;
            if (!(wp->flags & SWP_NOSIZE))
            {
                wp->cx = GetSystemMetrics(SM_CXSCREEN);
                wp->cy = GetSystemMetrics(SM_CYSCREEN);
            }
            if (!(wp->flags & SWP_NOMOVE))
            {
                wp->x = 0;
                wp->y = 0;
            }
        }
    }

    if (window.m_menuVisible && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return TRUE;

    if (msg == WM_MOUSEACTIVATE && window.m_menuVisible)
        return MA_ACTIVATE;

    if (msg == WM_NCHITTEST)
        return window.m_menuVisible ? HTCLIENT : HTTRANSPARENT;

    return CallWindowProcA(window.m_originalWndProc, hwnd, msg, wParam, lParam);
}

void Window::subclass(HWND hwnd)
{
    if (!hwnd || m_originalWndProc)
        return;

    m_hwnd            = hwnd;
    m_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProcHook)));

    PostMessage(hwnd, WM_APP, 0, 0);

    setInteractive(m_menuVisible);
}

void Window::restore()
{
    if (m_hwnd && IsWindow(m_hwnd))
    {
        if (m_originalWndProc)
            SetWindowLongPtrA(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalWndProc));
    }

    m_originalWndProc = nullptr;
    m_hwnd            = nullptr;
}

void Window::setInteractive(bool interactive)
{
    if (!m_hwnd || !IsWindow(m_hwnd))
        return;

    const LONG_PTR ex = GetWindowLongPtrA(m_hwnd, GWL_EXSTYLE);
    if (interactive)
    {
        SetWindowLongPtrA(m_hwnd, GWL_EXSTYLE,
            ex & ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE));
        BringWindowToTop(m_hwnd);
        SetForegroundWindow(m_hwnd);
        SendMessage(m_hwnd, WM_USER, 0, 0);
    }
    else
    {
        SetWindowLongPtrA(m_hwnd, GWL_EXSTYLE,
            ex | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }
}



