#include "PDBReader.h"
#include <filesystem>
#include <fstream>

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
        throw std::exception("Could not CoCreate CLSID_DiaSource. Maybe msdiaxxx.dll cannot be found.");
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

void PDBReader::DumpFunctions(const std::wstring out_file)
{
    std::ofstream out;
    out.open(out_file, std::ofstream::binary);
    if (!out.is_open())
    {
        throw std::exception("cannot create file for output");
    }

    CComPtr<IDiaEnumSymbols> pEnumSymbols;
    HRESULT hr = pGlobal->findChildren(SymTagEnum::SymTagFunction, 0, nsfCaseSensitive, &pEnumSymbols);
    if (FAILED(hr))
    {
        throw std::exception("findChildren() with null name failed.");
    }
    LONG count = 0;
    hr = pEnumSymbols->get_Count(&count);
    if (count == 0 || (FAILED(hr)))
    {
        throw std::exception("get_Count() failed or returned zero");
    }
    while (true)
    {
        CComPtr<IDiaSymbol> pSymbol;
        ULONG celt = 1;
        hr = pEnumSymbols->Next(1, &pSymbol, &celt);
        if ((FAILED(hr)) || (celt != 1))
        {
            break;
        }
        DWORD rva = 0;
        hr = pSymbol->get_relativeVirtualAddress(&rva);
        if (FAILED(hr))
        {
            continue;
        }
        CComBSTR name;
        hr = pSymbol->get_name(&name);
        if (FAILED(hr))
        {
            continue;
        }       
        out.write((char*)name.m_str, 2 * (wcslen(name.m_str) + 1));
        out.write((char*)L"\t", 2);
        auto addr = std::to_wstring(rva);
        out.write((char*)addr.c_str(), 2 * (1 + addr.size()));
        out.write((char*)L"\n", 2);
    }
}

HRESULT PDBReader::CoInit(DWORD init_flag)
{
    return CoInitializeEx(0, init_flag);
}

void PDBReader::SetMsdiaDllPath(std::wstring p)
{
    dia_dll_full_path = p;
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

HRESULT PDBReader::CreateDiaDataSourceWithoutComRegistration(IDiaDataSource** data_source)
{
    HMODULE hmodule = LoadLibraryW(PDBReader::dia_dll_name.c_str());
    // try to load dia dll using the full name
    if (!hmodule && dia_dll_full_path != L"")
    {
        hmodule = LoadLibraryW(dia_dll_full_path.c_str());
    }
    if (!hmodule)
    {
        return HRESULT_FROM_WIN32(GetLastError());
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