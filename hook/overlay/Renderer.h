#pragma once

#include "State.h"
#include "Window.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <thread>
#include <atomic>
#include <chrono>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"

class Renderer
{
public:
    bool install(HWND hwnd);
    void uninstall();
    IDXGISwapChain* swapchain() const { return m_swapChain; }

private:
    static constexpr int kSlotPresent       = 8;
    static constexpr int kSlotResizeBuffers = 13;

    IDXGISwapChain*         m_swapChain             = nullptr;
    ID3D11Device*           m_device                = nullptr;
    ID3D11DeviceContext*    m_context               = nullptr;
    ID3D11RenderTargetView* m_rtv                   = nullptr;
    PresentFn               m_originalPresent       = nullptr;
    ResizeBuffersFn         m_originalResizeBuffers = nullptr;
    uintptr_t*              m_vtable                = nullptr;
    bool                    m_imguiReady            = false;

    std::thread             m_renderThread;
    std::atomic<bool>       m_renderRunning{ false };

    uintptr_t patchVtable(uintptr_t* vtable, int slot, uintptr_t fn);
    void      syncSwapChain(IDXGISwapChain* sc);
    void      ensureBackbuffer(IDXGISwapChain* sc);
    void      createRTV();
    void      initImGui();
    void      shutdownImGui();
    void      releaseD3D();
    void      renderLoop();
    void      renderScene();

    static HRESULT __stdcall hookedPresent(IDXGISwapChain*, UINT, UINT);
    static HRESULT __stdcall hookResizeBuffers(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
};

inline Renderer renderer;
