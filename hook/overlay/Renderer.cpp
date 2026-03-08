#include "Renderer.h"

uintptr_t Renderer::patchVtable(uintptr_t* vtable, int slot, uintptr_t fn)
{
    const uintptr_t old = vtable[slot];
    DWORD protect = 0;
    VirtualProtect(&vtable[slot], sizeof(uintptr_t), PAGE_EXECUTE_READWRITE, &protect);
    vtable[slot] = fn;
    VirtualProtect(&vtable[slot], sizeof(uintptr_t), protect, &protect);
    return old;
}

void Renderer::syncSwapChain(IDXGISwapChain* sc)
{
    ID3D11Device* newDevice = nullptr;
    sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&newDevice));

    if (newDevice != m_device)
    {
        shutdownImGui();
        if (m_rtv)     { m_rtv->Release();     m_rtv     = nullptr; }
        if (m_context) { m_context->Release();  m_context = nullptr; }
        if (m_device)  { m_device->Release();   m_device  = nullptr; }
        m_device = newDevice;
        m_device->GetImmediateContext(&m_context);
    }
    else
    {
        if (newDevice) newDevice->Release();
        if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    }

    if (m_swapChain) m_swapChain->Release();
    sc->AddRef();
    m_swapChain = sc;
}

void Renderer::ensureBackbuffer(IDXGISwapChain* sc)
{
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    DXGI_SWAP_CHAIN_DESC desc{};
    sc->GetDesc(&desc);
    if ((int)desc.BufferDesc.Width >= sw && (int)desc.BufferDesc.Height >= sh)
        return;

    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    if (m_imguiReady) ImGui_ImplDX11_InvalidateDeviceObjects();
    sc->ResizeBuffers(0, sw, sh, DXGI_FORMAT_UNKNOWN, 0);
    createRTV();
    if (m_imguiReady) ImGui_ImplDX11_CreateDeviceObjects();
}

HRESULT __stdcall Renderer::hookedPresent(IDXGISwapChain* sc, UINT syncInterval, UINT flags)
{
    if (sc != renderer.m_swapChain)
        renderer.syncSwapChain(sc);

    if (!renderer.m_imguiReady)
    {
        if (!renderer.m_device)
        {
            if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&renderer.m_device))))
                return renderer.m_originalPresent(sc, syncInterval, flags);
            renderer.m_device->GetImmediateContext(&renderer.m_context);
        }
        renderer.createRTV();
        renderer.initImGui();
        if (!renderer.m_imguiReady)
            return renderer.m_originalPresent(sc, syncInterval, flags);
    }

    if (!renderer.m_rtv)
        renderer.createRTV();

    renderer.ensureBackbuffer(sc);

    if (!IsWindowVisible(window.hwnd()))
        ShowWindow(window.hwnd(), SW_SHOWNOACTIVATE);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    POINT pt{};
    GetCursorPos(&pt);
    ScreenToClient(window.hwnd(), &pt);
    ImGui::GetIO().MousePos    = ImVec2(static_cast<float>(pt.x), static_cast<float>(pt.y));
    ImGui::GetIO().DisplaySize = ImVec2(
        static_cast<float>(GetSystemMetrics(SM_CXSCREEN)),
        static_cast<float>(GetSystemMetrics(SM_CYSCREEN)));

    ImGui::NewFrame();
    renderer.renderScene();
    ImGui::Render();

    renderer.m_context->OMSetRenderTargets(1, &renderer.m_rtv, nullptr);

    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    const D3D11_VIEWPORT vp{ 0.0f, 0.0f, ds.x, ds.y, 0.0f, 1.0f };
    renderer.m_context->RSSetViewports(1, &vp);

    constexpr float kClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    renderer.m_context->ClearRenderTargetView(renderer.m_rtv, kClear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return renderer.m_originalPresent(sc, 0, 0);
}

void Renderer::renderLoop()
{
    using namespace std::chrono;
    constexpr int kTargetFPS = 165;
    constexpr auto kFrameTime = duration_cast<nanoseconds>(duration<double>(1.0 / kTargetFPS));

    while (m_renderRunning)
    {
        const auto frameStart = high_resolution_clock::now();

        const LONG_PTR overlayObj = GetWindowLongPtrW(window.hwnd(), GWLP_USERDATA);
        IDXGISwapChain* liveSc = overlayObj
            ? *reinterpret_cast<IDXGISwapChain**>(overlayObj + State::kSwapChainOffset)
            : nullptr;

        if (!liveSc) { Sleep(1); continue; }
        liveSc->Present(0, 0);

        const auto elapsed = high_resolution_clock::now() - frameStart;
        const auto remaining = kFrameTime - elapsed;
        if (remaining > nanoseconds(0))
            std::this_thread::sleep_for(remaining);
    }
}

HRESULT __stdcall Renderer::hookResizeBuffers(
    IDXGISwapChain* sc, UINT count, UINT w, UINT h, DXGI_FORMAT fmt, UINT flags)
{
    if (renderer.m_rtv) { renderer.m_rtv->Release(); renderer.m_rtv = nullptr; }
    if (renderer.m_imguiReady) ImGui_ImplDX11_InvalidateDeviceObjects();

    const HRESULT hr = renderer.m_originalResizeBuffers(sc, count, w, h, fmt, flags);

    renderer.createRTV();
    if (renderer.m_imguiReady) ImGui_ImplDX11_CreateDeviceObjects();
    return hr;
}

void Renderer::createRTV()
{
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    ID3D11Texture2D* buf = nullptr;
    m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&buf));
    if (buf) { m_device->CreateRenderTargetView(buf, nullptr, &m_rtv); buf->Release(); }
}

void Renderer::initImGui()
{
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    ImGui_ImplWin32_Init(window.hwnd());
    ImGui_ImplDX11_Init(m_device, m_context);
    m_imguiReady = true;
}

void Renderer::shutdownImGui()
{
    if (!m_imguiReady) return;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    m_imguiReady = false;
}

void Renderer::releaseD3D()
{
    if (m_rtv)       { m_rtv->Release();      m_rtv       = nullptr; }
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_context)   { m_context->Release();   m_context   = nullptr; }
    if (m_device)    { m_device->Release();    m_device    = nullptr; }
}

void Renderer::renderScene()
{
    ImGui::GetBackgroundDrawList()->AddCircleFilled({ 960.0f, 540.0f }, 30.0f, IM_COL32(255, 50, 50, 200));

    if (!window.menuVisible())
        return;

    static bool  showDemo = true;
    static float value    = 0.0f;
    static int   clicks   = 0;

    ImGui::Begin("Overlay");
    ImGui::Text("Insert: toggle   F3: eject");
    ImGui::Separator();
    ImGui::Checkbox("ImGui Demo", &showDemo);
    ImGui::SliderFloat("Value", &value, 0.0f, 1.0f);
    if (ImGui::Button("Click")) ++clicks;
    ImGui::SameLine();
    ImGui::Text("count: %d   %.0f fps", clicks, ImGui::GetIO().Framerate);
    ImGui::End();

    if (showDemo)
        ImGui::ShowDemoWindow(&showDemo);
}

bool Renderer::install(HWND hwnd)
{
    const LONG_PTR overlayObj = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!overlayObj) return false;

    auto* sc = *reinterpret_cast<IDXGISwapChain**>(overlayObj + State::kSwapChainOffset);
    if (!sc) return false;

    m_swapChain = sc;
    m_swapChain->AddRef();
    m_vtable = *reinterpret_cast<uintptr_t**>(sc);

    m_originalPresent       = reinterpret_cast<PresentFn>(
        patchVtable(m_vtable, kSlotPresent, reinterpret_cast<uintptr_t>(&hookedPresent)));
    m_originalResizeBuffers = reinterpret_cast<ResizeBuffersFn>(
        patchVtable(m_vtable, kSlotResizeBuffers, reinterpret_cast<uintptr_t>(&hookResizeBuffers)));

    window.subclass(hwnd);

    m_renderRunning = true;
    m_renderThread  = std::thread([]{ renderer.renderLoop(); });

    return true;
}

void Renderer::uninstall()
{
    m_renderRunning = false;
    if (m_renderThread.joinable())
        m_renderThread.join();

    if (m_vtable)
    {
        if (m_originalPresent)
            patchVtable(m_vtable, kSlotPresent, reinterpret_cast<uintptr_t>(m_originalPresent));
        if (m_originalResizeBuffers)
            patchVtable(m_vtable, kSlotResizeBuffers, reinterpret_cast<uintptr_t>(m_originalResizeBuffers));
        m_vtable = nullptr;
    }

    shutdownImGui();
    releaseD3D();

    m_originalPresent       = nullptr;
    m_originalResizeBuffers = nullptr;

    window.restore();
}


