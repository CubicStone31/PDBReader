#include "PDBReader.h"
#include <filesystem>

PDBReader::PDBReader(std::wstring pdb_name)
{
    CComPtr<IDiaDataSource> pSource;
    HRESULT hr;
    hr = CreateDiaDataSourceWithoutComRegistration(&pSource);
    if (FAILED(hr))
    {
        throw std::exception("Could not CoCreate CLSID_DiaSource. You should place msdiaXX.dll in current folder.");
    }
    hr = pSource->loadDataFromPdb(pdb_name.c_str());
    if (FAILED(hr))
    {
        throw std::exception("Could not load pdb file.");
    }
    hr = pSource->openSession(&pSession);
    if (FAILED(hr))
    {
        throw std::exception("Could not open session.");
    }
    hr = pSession->get_globalScope(&pGlobal);
    if (FAILED(hr))
    {
        throw std::exception("Could not get global scope.");
    }
}

PDBReader::PDBReader(std::wstring executable_name, std::wstring search_path)
{
    CComPtr<IDiaDataSource> pSource;
    HRESULT hr = CreateDiaDataSourceWithoutComRegistration(&pSource);
    if (FAILED(hr))
    {
        throw std::exception("Could not CoCreate CLSID_DiaSource. You should place msdiaXX.dll in current folder.");
    }
    // using format: srv*search_path* to treat target folder as a symbol cache and search it recursively
    std::wstring search_str = std::filesystem::absolute(search_path);
    search_str = L"srv*" + search_str + L"*";
    hr = pSource->loadDataForExe(executable_name.c_str(), search_str.c_str(), 0);
    if (FAILED(hr))
    {
        throw std::exception("Could not load pdb file.");
    }
    hr = pSource->openSession(&pSession);
    if (FAILED(hr))
    {
        throw std::exception("Could not open session.");
    }
    hr = pSession->get_globalScope(&pGlobal);
    if (FAILED(hr))
    {
        throw std::exception("Could not get global scope.");
    }
}

std::optional<DWORD> PDBReader::FindSymbol(std::wstring sym, DWORD& type)
{
    // lookup in cache
    // it seems that com api such as findchildren() will also cache its result. 
    // however, as I don't find any document about this we keep our simple cache system here.
    if (symbolRVACache.find(sym) != symbolRVACache.end())
    {
        return symbolRVACache[sym];
    }

    CComPtr<IDiaEnumSymbols> pEnumSymbols;
    HRESULT hr = pGlobal->findChildren(SymTagEnum::SymTagNull, sym.c_str(), nsfCaseSensitive, &pEnumSymbols);
    if (FAILED(hr))
    {
        return {};
    }
    LONG count = 0;
    hr = pEnumSymbols->get_Count(&count);
    if (count != 1 || (FAILED(hr)))
    {
        return {};
    }
    CComPtr<IDiaSymbol> pSymbol;
    ULONG celt = 1;
    hr = pEnumSymbols->Next(1, &pSymbol, &celt);
    if ((FAILED(hr)) || (celt != 1))
    {
        return {};
    }
    // get symbols's type info
    hr = pSymbol->get_symTag(&type);
    if (FAILED(hr))
    {
        return {};
    }
    DWORD rva = 0;
    hr = pSymbol->get_relativeVirtualAddress(&rva);

    // add to cache to speed up next lookup
    if (symbolRVACache.find(sym) == symbolRVACache.end())
    {
        symbolRVACache[sym] = rva;
    }
    return rva;
}

std::optional<DWORD> PDBReader::FindConst(std::wstring const_name)
{
    DWORD type = SymTagEnum::SymTagData;
    return FindSymbol(const_name, type);
}

std::optional<DWORD> PDBReader::FindFunction(std::wstring func)
{
    DWORD type = SymTagEnum::SymTagFunction;
    return FindSymbol(func, type);
}

std::optional<DWORD> PDBReader::FindStructMemberOffset(std::wstring structName, std::wstring memberName)
{
    CComPtr<IDiaEnumSymbols> pEnumSymbols;
    HRESULT hr = pGlobal->findChildren(SymTagEnum::SymTagUDT, structName.c_str(), nsfCaseSensitive, &pEnumSymbols);
    if (FAILED(hr))
    {
        return {};
    }
    LONG count = 0;
    hr = pEnumSymbols->get_Count(&count);
    if (count != 1 || (FAILED(hr)))
    {
        return {};
    }
    CComPtr<IDiaSymbol> pSymbol;
    ULONG celt = 1;
    hr = pEnumSymbols->Next(1, &pSymbol, &celt);
    if ((FAILED(hr)) || (celt != 1))
    {
        return {};
    }
    CComPtr<IDiaEnumSymbols> structEnumSymbols;
    hr = pSymbol->findChildren(SymTagEnum::SymTagNull, memberName.c_str(), nsfCaseSensitive, &structEnumSymbols);
    if (FAILED(hr))
    {
        return {};
    }
    count = 0;
    hr = structEnumSymbols->get_Count(&count);
    if (count != 1 || (FAILED(hr)))
    {
        return {};
    }
    CComPtr<IDiaSymbol> memberSymbol;
    celt = 1;
    hr = structEnumSymbols->Next(1, &memberSymbol, &celt);
    if ((FAILED(hr)) || (celt != 1))
    {
        return {};
    }
    LONG offset = 0;
    hr = memberSymbol->get_offset(&offset);
    if (FAILED(hr))
    {
        return {};
    }
    return offset;
}

std::optional<UINT64> PDBReader::FindStructSize(std::wstring structName)
{
    CComPtr<IDiaEnumSymbols> pEnumSymbols;
    HRESULT hr = pGlobal->findChildren(SymTagEnum::SymTagUDT, structName.c_str(), nsfCaseSensitive, &pEnumSymbols);
    if (FAILED(hr))
    {
        return {};
    }
    LONG count = 0;
    hr = pEnumSymbols->get_Count(&count);
    if (count != 1 || (FAILED(hr)))
    {
        return {};
    }
    CComPtr<IDiaSymbol> pSymbol;
    ULONG celt = 1;
    hr = pEnumSymbols->Next(1, &pSymbol, &celt);
    if ((FAILED(hr)) || (celt != 1))
    {
        return {};
    }
    UINT64 size;
    hr = pSymbol->get_length(&size);
    if (FAILED(hr))
    {
        return {};
    }
    return size;
}

void PDBReader::FindNearestSymbolFromRVA(DWORD rva, std::wstring& symbolName, DWORD& symbolType)
{
    CComPtr<IDiaSymbol> target;
    HRESULT hr = pSession->findSymbolByRVA(rva, SymTagEnum::SymTagNull, &target);
    if (FAILED(hr))
    {
        throw std::exception("findSymbolByRVA() failed.");
    }
    hr = target->get_symTag(&symbolType);
    if (FAILED(hr))
    {
        throw std::exception("get_symTag() failed.");
    }
    hr = target->get_relativeVirtualAddress(&rva);
    if (FAILED(hr))
    {
        throw std::exception("get_relativeVirtualAddress() failed.");
    }
    BSTR bstrName;
    hr = target->get_name(&bstrName);
    if (FAILED(hr))
    {
        throw std::exception("get_name() failed.");
    }
    symbolName = bstrName;
    SysFreeString(bstrName);
    return ;
}

HRESULT PDBReader::CoInit(DWORD init_flag)
{
    return CoInitializeEx(0, init_flag);
}

void PDBReader::DownloadPDBForFile(std::wstring executable_name, std::wstring symbol_folder, std::wstring SYMBOL_SERVER_URL)
{
    CComPtr<IDiaDataSource> pSource;
    HRESULT hr;
    hr = CreateDiaDataSourceWithoutComRegistration(&pSource);
    if (FAILED(hr))
    {
        throw std::exception("Could not CoCreate CLSID_DiaSource. You should place msdiaXXX.dll in current folder.");
    }
    std::filesystem::path sym_cache_folder = std::filesystem::absolute(symbol_folder);
    std::wstring seach_path = std::wstring(L"srv*") + sym_cache_folder.c_str() + L"*" + SYMBOL_SERVER_URL;
    hr = pSource->loadDataForExe(executable_name.c_str(), seach_path.c_str(), 0);
    if (FAILED(hr))
    {
        throw std::exception("Failed to download symbols. Maybe you should place symsrv.dll in current folder or check your internet connection.");
    }
    return;
}

// We dont want to regsvr32 msdia140.dll on client's machine...
HRESULT PDBReader::CreateDiaDataSourceWithoutComRegistration(IDiaDataSource** data_source)
{
    HMODULE hmodule = LoadLibraryW(PDBReader::dia_dll_name.c_str());
    // try to load dia dll in the same folder as main executable's
    if (!hmodule)
    {
        wchar_t temp[MAX_PATH] = { 0 };
        GetModuleFileNameW(0, temp, MAX_PATH);
        std::filesystem::path dllPath(temp);
        dllPath = dllPath.parent_path() / L"msdia140.dll";
        hmodule = LoadLibraryW(dllPath.c_str());
        if (!hmodule)
        {
            return HRESULT_FROM_WIN32(GetLastError()); // library not found
        }
    }
    HRESULT(WINAPI * DllGetClassObject)(REFCLSID, REFIID, LPVOID*) = (HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*))GetProcAddress(hmodule, "DllGetClassObject");
    if (!DllGetClassObject)
    {
        return E_FAIL;
    }
    IClassFactory* pClassFactory = NULL;
    HRESULT hr = DllGetClassObject(CLSID_DiaSource, IID_IClassFactory, (LPVOID*)&pClassFactory);
    if (FAILED(hr))
    {
        return hr;
    }
    hr = pClassFactory->CreateInstance(NULL, IID_IDiaDataSource, (void**)data_source);
    if (FAILED(hr))
    {
        pClassFactory->Release();
        return hr;
    }
    pClassFactory->Release();
    return S_OK;
}