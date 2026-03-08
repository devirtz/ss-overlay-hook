#include "Shellcode.h"

#pragma code_seg(push, "r", ".scode")
#pragma runtime_checks("", off)
#pragma optimize("g", off)

void __stdcall Shellcode(MappingData* pData)
{
    if (!pData) return;

    BYTE* pBase          = pData->pBase;
    auto& LoadLibraryA_   = pData->pLoadLibraryA;
    auto& GetProcAddress_ = pData->pGetProcAddress;

    auto* pDosHdr = reinterpret_cast<IMAGE_DOS_HEADER*>(pBase);
    auto* pNtHdr  = reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + pDosHdr->e_lfanew);
    auto& optHdr  = pNtHdr->OptionalHeader;

    // Relocations
    BYTE* delta = pBase - optHdr.ImageBase;
    if (delta && optHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size)
    {
        auto* pReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
            pBase + optHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

        while (pReloc->VirtualAddress)
        {
            UINT  count   = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* entries = reinterpret_cast<WORD*>(pReloc + 1);

            for (UINT i = 0; i < count; ++i)
            {
                WORD type   = entries[i] >> 12;
                WORD offset = entries[i] & 0x0FFF;

                if (type == IMAGE_REL_BASED_DIR64)
                    *reinterpret_cast<UINT_PTR*>(pBase + pReloc->VirtualAddress + offset)
                        += reinterpret_cast<UINT_PTR>(delta);
                else if (type == IMAGE_REL_BASED_HIGHLOW)
                    *reinterpret_cast<DWORD*>(pBase + pReloc->VirtualAddress + offset)
                        += static_cast<DWORD>(reinterpret_cast<UINT_PTR>(delta));
            }

            pReloc = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                reinterpret_cast<BYTE*>(pReloc) + pReloc->SizeOfBlock);
        }
    }

    // Imports
    if (optHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size)
    {
        auto* pImportDesc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            pBase + optHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

        while (pImportDesc->Name)
        {
            HINSTANCE hMod = LoadLibraryA_(reinterpret_cast<char*>(pBase + pImportDesc->Name));

            auto* pThunk    = reinterpret_cast<IMAGE_THUNK_DATA*>(pBase + pImportDesc->OriginalFirstThunk);
            auto* pFuncAddr = reinterpret_cast<IMAGE_THUNK_DATA*>(pBase + pImportDesc->FirstThunk);

            if (!pImportDesc->OriginalFirstThunk)
                pThunk = pFuncAddr;

            while (pThunk->u1.AddressOfData)
            {
                if (IMAGE_SNAP_BY_ORDINAL(pThunk->u1.Ordinal))
                    pFuncAddr->u1.Function = reinterpret_cast<UINT_PTR>(
                        GetProcAddress_(hMod, reinterpret_cast<char*>(pThunk->u1.Ordinal & 0xFFFF)));
                else
                    pFuncAddr->u1.Function = reinterpret_cast<UINT_PTR>(
                        GetProcAddress_(hMod,
                            reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pBase + pThunk->u1.AddressOfData)->Name));
                ++pThunk;
                ++pFuncAddr;
            }
            ++pImportDesc;
        }
    }

    // TLS callbacks
    if (optHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size)
    {
        auto* pTls = reinterpret_cast<IMAGE_TLS_DIRECTORY*>(
            pBase + optHdr.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
        auto** ppCb = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTls->AddressOfCallBacks);
        if (ppCb)
            while (*ppCb) { (*ppCb)(pBase, DLL_PROCESS_ATTACH, nullptr); ++ppCb; }
    }

    // DllMain
    if (optHdr.AddressOfEntryPoint)
        reinterpret_cast<f_DllMain>(pBase + optHdr.AddressOfEntryPoint)(
            reinterpret_cast<HINSTANCE>(pBase), DLL_PROCESS_ATTACH, nullptr);

    pData->initialized = TRUE;
}

void __stdcall ShellcodeEnd() {}

#pragma optimize("g", on)
#pragma runtime_checks("", restore)
#pragma code_seg(pop)
