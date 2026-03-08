#include "Injector.h"
#include "Shellcode.h"

Injector::Injector(DWORD pid)
    : m_pid(pid)
{
}

std::vector<BYTE> Injector::readFile(const std::wstring& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    std::vector<BYTE> buf(size);
    f.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

bool Injector::inject(const std::wstring& dllPath)
{
    auto fileData = readFile(dllPath);
    if (fileData.empty()) { std::cerr << "[-] Cannot read DLL.\n"; return false; }

    BYTE* pSrc    = fileData.data();
    auto* pDosHdr = reinterpret_cast<IMAGE_DOS_HEADER*>(pSrc);
    auto* pNtHdr  = reinterpret_cast<IMAGE_NT_HEADERS*>(pSrc + pDosHdr->e_lfanew);

    if (pDosHdr->e_magic != IMAGE_DOS_SIGNATURE || pNtHdr->Signature != IMAGE_NT_SIGNATURE)
    { std::cerr << "[-] Invalid PE file.\n"; return false; }

    HANDLE hProc = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION  | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, m_pid);
    if (!hProc) { std::cerr << "[-] OpenProcess failed: " << GetLastError() << '\n'; return false; }

    const DWORD imageSize = pNtHdr->OptionalHeader.SizeOfImage;
    BYTE* pBase = reinterpret_cast<BYTE*>(
        VirtualAllocEx(hProc, nullptr, imageSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!pBase)
    { std::cerr << "[-] VirtualAllocEx (image) failed: " << GetLastError() << '\n'; CloseHandle(hProc); return false; }

    std::cout << "[+] Image allocated at " << static_cast<void*>(pBase) << " (" << imageSize << " bytes)\n";

    WriteProcessMemory(hProc, pBase, pSrc, pNtHdr->OptionalHeader.SizeOfHeaders, nullptr);

    auto* pSection = IMAGE_FIRST_SECTION(pNtHdr);
    for (WORD i = 0; i < pNtHdr->FileHeader.NumberOfSections; ++i)
    {
        if (!pSection[i].SizeOfRawData) continue;
        WriteProcessMemory(hProc,
            pBase + pSection[i].VirtualAddress,
            pSrc  + pSection[i].PointerToRawData,
            pSection[i].SizeOfRawData, nullptr);
        std::cout << "[+] Section [" << reinterpret_cast<char*>(pSection[i].Name)
                  << "] -> " << static_cast<void*>(pBase + pSection[i].VirtualAddress) << '\n';
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    MappingData data{};
    data.pLoadLibraryA   = reinterpret_cast<f_LoadLibraryA>  (GetProcAddress(hKernel32, "LoadLibraryA"));
    data.pGetProcAddress = reinterpret_cast<f_GetProcAddress>(GetProcAddress(hKernel32, "GetProcAddress"));
    data.pBase           = pBase;

    SIZE_T scSize = reinterpret_cast<BYTE*>(ShellcodeEnd) - reinterpret_cast<BYTE*>(Shellcode);
    if (!scSize || scSize > 0x10000)
    {
        std::cerr << "[-] Shellcode size invalid (" << scSize << "). Recompile without LTCG.\n";
        VirtualFreeEx(hProc, pBase, 0, MEM_RELEASE); CloseHandle(hProc); return false;
    }

    BYTE* pBlock = reinterpret_cast<BYTE*>(VirtualAllocEx(hProc, nullptr,
        sizeof(MappingData) + scSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!pBlock)
    { std::cerr << "[-] VirtualAllocEx (shellcode) failed.\n"; VirtualFreeEx(hProc, pBase, 0, MEM_RELEASE); CloseHandle(hProc); return false; }

    WriteProcessMemory(hProc, pBlock,                       &data,                               sizeof(data),  nullptr);
    WriteProcessMemory(hProc, pBlock + sizeof(MappingData),  reinterpret_cast<BYTE*>(Shellcode),  scSize,        nullptr);

    std::cout << "[+] Shellcode at " << static_cast<void*>(pBlock + sizeof(MappingData)) << " (" << scSize << " bytes)\n";

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(pBlock + sizeof(MappingData)),
        pBlock, 0, nullptr);
    if (!hThread)
    { std::cerr << "[-] CreateRemoteThread failed: " << GetLastError() << '\n'; VirtualFreeEx(hProc, pBlock, 0, MEM_RELEASE); VirtualFreeEx(hProc, pBase, 0, MEM_RELEASE); CloseHandle(hProc); return false; }

    std::cout << "[+] Remote thread started, waiting...\n";

    for (int i = 0; i < 500; ++i)
    {
        MappingData check{};
        ReadProcessMemory(hProc, pBlock, &check, sizeof(check), nullptr);
        if (check.initialized) { std::cout << "[+] Injected successfully.\n"; break; }
        Sleep(10);
        if (i == 499) std::cerr << "[-] Timed out.\n";
    }

    CloseHandle(hThread);
    VirtualFreeEx(hProc, pBlock, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return true;
}

DWORD Injector::findPidByName(const std::wstring& name)
{
    DWORD  pid  = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe))
        do { if (_wcsicmp(pe.szExeFile, name.c_str()) == 0) { pid = pe.th32ProcessID; break; } }
        while (Process32NextW(snap, &pe));

    CloseHandle(snap);
    return pid;
}

DWORD Injector::findSteelSeriesOverlayPid()
{
    HWND hw = FindWindowA("GameOverlay", "GameOverlay");
    if (!hw) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hw, &pid);
    return pid;
}
