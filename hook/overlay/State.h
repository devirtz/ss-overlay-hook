#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

using PresentFn       = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

struct State
{
    static constexpr size_t    kSwapChainOffset      = 0x28; // IDXGISwapChain* inside overlay object
    // Overlay_RenderFrame / Overlay_UpdateVisibility internals (overlayObject-relative):
    static constexpr ptrdiff_t kTrackedHwndOffset    = 8;   // HWND   — if non-null, triggers foreground check → early return when not foreground
    static constexpr ptrdiff_t kVisibleFlagOffset    = 120; // BYTE   — 1 = timer id=1 is armed + window visible; 0 = hidden (KillTimer was called)
    static constexpr ptrdiff_t kForegroundFlagOffset = 122; // BYTE   — 1 = overlay is foreground; 0 = hidden/early-exit
    static constexpr ptrdiff_t kDirtyFlagOffset      = 123; // BYTE   — 1 = force full render+Present this tick
};
