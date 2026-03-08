#include "Injector.h"

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2 || argc > 3)
    {
        std::wcerr << L"Usage:\n"
                   << L"  Injector <dll_path>\n"
                   << L"  Injector <process_name|pid> <dll_path>\n";
        return 1;
    }

    DWORD        pid = 0;
    std::wstring dllPath;

    if (argc == 2)
    {
        dllPath = argv[1];
        pid = Injector::findSteelSeriesOverlayPid();
        if (!pid)
        {
            std::cerr << "[-] SteelSeries GameOverlay window not found.\n";
            return 1;
        }
        std::cout << "[+] Found GameOverlay PID: " << pid << '\n';
    }
    else
    {
        std::wstring target = argv[1];
        dllPath             = argv[2];

        wchar_t* end = nullptr;
        pid = static_cast<DWORD>(wcstoul(target.c_str(), &end, 10));
        if (*end != L'\0')
            pid = Injector::findPidByName(target);

        if (!pid)
        {
            std::wcerr << L"[-] Process not found: " << target << L'\n';
            return 1;
        }
        std::cout << "[+] Target PID: " << pid << '\n';
    }

    Injector inj(pid);
    return inj.inject(dllPath) ? 0 : 1;
}
