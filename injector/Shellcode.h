#pragma once

#include <Windows.h>

using f_LoadLibraryA   = HINSTANCE(WINAPI*)(const char*);
using f_GetProcAddress = FARPROC  (WINAPI*)(HMODULE, const char*);
using f_DllMain        = BOOL     (WINAPI*)(HINSTANCE, DWORD, LPVOID);

struct MappingData
{
    f_LoadLibraryA   pLoadLibraryA;
    f_GetProcAddress pGetProcAddress;
    BYTE*            pBase;
    DWORD            lastError;
    BOOL             initialized;
};

void __stdcall Shellcode(MappingData* pData);
void __stdcall ShellcodeEnd();
