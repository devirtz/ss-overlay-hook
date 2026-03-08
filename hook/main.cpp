#include <Windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#include "overlay/Window.h"
#include "Log.h"

HWND findOverlayWindow()
{
	if (HWND h = FindWindowA("GameOverlay", "GameOverlay"))
		return h;
	return FindWindowA("GameOverlay", nullptr);
}

bool waitForSwapChain(HWND hwnd)
{
	for (int i = 0; i < 200; ++i)
	{
		const LONG_PTR obj = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
		if (obj && *reinterpret_cast<void**>(obj + State::kSwapChainOffset))
			return true;
		Sleep(50);
	}
	return false;
}

DWORD WINAPI run(HMODULE module)
{
	// Truncate log file for this inject session.
	{
		HANDLE hf = CreateFileA(logPath(), GENERIC_WRITE, FILE_SHARE_READ,
			nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hf != INVALID_HANDLE_VALUE) CloseHandle(hf);
	}
	Log() << "[+] Hook loaded\n";

	HWND hwnd = nullptr;
	for (int i = 0; i < 100 && !hwnd; ++i)
	{
		hwnd = findOverlayWindow();
		Sleep(50);
	}

	if (!hwnd || !waitForSwapChain(hwnd))
	{
		Log() << "[-] GameOverlay not ready\n";
		FreeLibraryAndExitThread(module, 0);
	}

	if (!renderer.install(hwnd))
	{
		Log() << "[-] Install failed\n";
		FreeLibraryAndExitThread(module, 0);
	}

	Log() << "[+] Running  Insert: toggle   F3: eject\n";

	timeBeginPeriod(1);
	bool insertDown = false;
	while (!(GetAsyncKeyState(VK_F3) & 0x8000))
	{
		Sleep(10);

		const bool insert = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
		if (insert && !insertDown)
		{
			window.setMenuVisible(!window.menuVisible());
			window.setInteractive(window.menuVisible());
			Log() << "[Insert] " << (window.menuVisible() ? "ON" : "OFF") << "\n";
		}
		insertDown = insert;
	}
	timeEndPeriod(1);
	renderer.uninstall();
	FreeLibraryAndExitThread(module, 0);
	return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);
		CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(run), module, 0, nullptr);
	}
	return TRUE;
}
