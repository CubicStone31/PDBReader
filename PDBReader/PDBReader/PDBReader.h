#pragma once
#include <string>
#include <atlbase.h>
#include <dia2.h>
#include <optional>

class PDBReader 
{
public:
    PDBReader(std::wstring pdb_name);

    PDBReader(std::wstring executable_name, std::wstring search_path);

    std::optional<DWORD> FindSymbol(std::wstring sym, DWORD& type);

    std::optional<DWORD> FindConst(std::wstring const_name);

    std::optional<DWORD> FindFunction(std::wstring func);

    std::optional<DWORD> FindStructMemberOffset(std::wstring structName, std::wstring memberName);

    std::optional<UINT64> FindStructSize(std::wstring structName);

    void FindNearestSymbolFromRVA(DWORD rva, std::wstring& symbolName, DWORD& symbolType);

    // Helper function
    static void DownloadPDBForFile(std::wstring executable_name, std::wstring symbol_folder, std::wstring SYMBOL_SERVER_URL = L"https://msdl.microsoft.com/download/symbols");

    static HRESULT CoInit(DWORD init_flag = COINIT_MULTITHREADED);

private:
    static HRESULT CreateDiaDataSourceWithoutComRegistration(IDiaDataSource** data_source);

    static inline std::wstring dia_dll_name = L"msdia140.dll";
    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaSymbol> pGlobal;
};
