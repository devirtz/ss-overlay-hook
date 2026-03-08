#pragma once

#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

class Injector
{
public:
    explicit Injector(DWORD pid);

    bool inject(const std::wstring& dllPath);

    static DWORD findPidByName(const std::wstring& processName);
    static DWORD findSteelSeriesOverlayPid();

private:
    DWORD m_pid;

    static std::vector<BYTE> readFile(const std::wstring& path);
};
