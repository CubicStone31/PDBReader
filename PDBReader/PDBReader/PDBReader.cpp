#include "PDBReader.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <codecvt>

std::string wstring2stringbytruncation(const std::wstring& in)
{
    std::string str(in.begin(), in.end());
    return str;
}

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

bool PDBReader::FindMostRelatedFunctionName(DWORD rva, std::wstring& funcname)
{
    if (!SortedFunctionRVANameList.size())
    {
        BuildSortedFunctionRVANameList();
    }
    if (std::get<0>(*SortedFunctionRVANameList.begin()) > rva)
    {
        return false;
    }
    for (auto itr = SortedFunctionRVANameList.begin(); itr != SortedFunctionRVANameList.end(); itr++)
    {
        auto next = std::next(itr, 1);
        if (next == SortedFunctionRVANameList.end())
        {
            funcname = std::get<1>(*itr);
            return true;
        }
        else if (std::get<0>(*next) > rva)
        {
            funcname = std::get<1>(*itr);
            return true;
        }
    }
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
    return;
}

void PDBReader::DumpTypes(enum SymTagEnum type, const std::wstring out_file)
{
    std::ofstream out;
    out.open(out_file, std::ofstream::binary);
    if (!out.is_open())
    {
        throw std::exception("cannot create file for output");
    }
    CComPtr<IDiaEnumSymbols> pEnumSymbols;
    HRESULT hr = pGlobal->findChildren(type, 0, nsfCaseSensitive, &pEnumSymbols);
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

    printf("total %d types to dump\n", count);

    for (int i = 0; ; i += 1)
    {
        if ((i % 100) == 0)
        {
            printf("progress: %d / %d\n", i, count);
        }
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
        CComBSTR tmp_name;
        hr = pSymbol->get_name(&tmp_name);
        if (FAILED(hr))
        {
            continue;
        }
        auto name = wstring2stringbytruncation(tmp_name.m_str);
        out.write(name.c_str(), name.size());
        out.write("\t", 1);
        auto addr = std::to_string(rva);
        out.write(addr.c_str(), addr.size());
        out.write("\n", 1);
    }
}

const std::vector<PDBReader::FieldInfo> PDBReader::GetStructureFields(const std::wstring& structName)
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
    return GetStructureFields(pSymbol);
}

const std::vector<PDBReader::FieldInfo> PDBReader::GetStructureFields(DWORD symbolId)
{
    CComPtr<IDiaSymbol> pSymbol;
    if (FAILED(pSession->symbolById(symbolId, &pSymbol)))
    {
        return {};
    }
    return GetStructureFields(pSymbol);
}

HRESULT PDBReader::CoInit(DWORD init_flag)
{
    return CoInitializeEx(0, init_flag);
}

void PDBReader::SetMsdiaDllPath(std::wstring p)
{
    dia_dll_full_path = p;
}

const PDBReader::TypeInfo PDBReader::GetTypeInfo(DWORD symbolId)
{
    if (symbolTypeInfoCache.find(symbolId) != symbolTypeInfoCache.end())
    {
        return symbolTypeInfoCache[symbolId];
    }
    CComPtr<IDiaSymbol> sym;
    if (FAILED(pSession->symbolById(symbolId, &sym)))
    {
        return {};
    }
    DWORD tag;
    if (FAILED(sym->get_symTag(&tag)))
    {
        return {};
    }
    ULONGLONG size;
    if (FAILED(sym->get_length(&size)) || !size)
    {
        return {};
    }
    DWORD id;
    if (FAILED(sym->get_symIndexId(&id)))
    {
        return {};
    }
    TypeInfo ret = {};
    ret.attached_pdb = this;
    ret.size = size;
    ret.type = Types::Unknown;
    ret.freindly_name = L"unknown";
    ret.associated_type_obj_id = id;
    switch (tag)
    {
    case SymTagUDT:
    {
        ret.type = Types::Class;
        BSTR struct_name;
        if (FAILED(sym->get_name(&struct_name)))
        {
            return {};
        }
        ret.freindly_name = struct_name;
        SysFreeString(struct_name);
        break;
    }
    case SymTagArrayType:
    {
        ret.type = Types::Array;
        DWORD array_element_id;
        if (FAILED(sym->get_typeId(&array_element_id)))
        {
            return {};
        }
        ret.arr_element_type_id = array_element_id;
        auto element_type = GetTypeInfo(array_element_id);
        if (!element_type.size)
        {
            return {};
        }
        if ((ret.size % element_type.size) != 0)
        {
            return {};
        }
        ret.arr_dim = ret.size / element_type.size;
        ret.freindly_name = std::wstring(element_type.freindly_name) + L"[" + std::to_wstring(ret.arr_dim) + L"]";
        break;
    }
    case SymTagBaseType:
    {
        DWORD base_type;
        if (FAILED(sym->get_baseType(&base_type)))
        {
            break;
        }
        if (base_type == btChar || base_type == btChar8)
        {
            ret.type = Types::Char;
            ret.freindly_name = L"char";
            break;
        }
        else if (base_type == btWChar || base_type == btChar16)
        {
            ret.type = Types::Widechar;
            ret.freindly_name = L"wchar";
            break;
        }
        else if (base_type == btInt || base_type == btLong)
        {
            ret.type = Types::Integer;
            ret.freindly_name = L"int" + std::to_wstring(ret.size * 8);
            break;
        }
        else if (base_type == btUInt || base_type == btULong)
        {
            ret.type = Types::UInteger;
            ret.freindly_name = L"uint" + std::to_wstring(ret.size * 8);
            break;
        }
        else if (base_type == btFloat && ret.size == 4)
        {
            ret.type = Types::Float;
            ret.freindly_name = L"float";
            break;
        }
        else if (base_type == btFloat && ret.size == 8)
        {
            ret.type = Types::Double;
            ret.freindly_name = L"double";
            break;
        }
        break;
    }
    default:
    {
        break;
    }
    }
    symbolTypeInfoCache[symbolId] = ret;
    return ret;
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

const std::vector<PDBReader::FieldInfo> PDBReader::GetStructureFields(IDiaSymbol* sym)
{
    DWORD id;
    if (FAILED(sym->get_symIndexId(&id)))
    {
        return {};
    }
    if (structureFieldInfoCache.find(id) != structureFieldInfoCache.end())
    {
        return structureFieldInfoCache[id];
    }
    CComPtr<IDiaEnumSymbols> pEnumSymbols;
    if (FAILED(sym->findChildrenEx(SymTagEnum::SymTagNull, 0, nsfCaseSensitive, &pEnumSymbols)))
    {
        return {};
    }
    LONG count = 0;
    if (FAILED(pEnumSymbols->get_Count(&count)))
    {
        return {};
    }

    auto ret = std::vector<FieldInfo>();
    for (int i = 0; i < count; i++)
    {
        CComPtr<IDiaSymbol> field;
        ULONG celt = 1;
        if (FAILED(pEnumSymbols->Next(1, &field, &celt)))
        {
            return {};
        }
        DWORD tag;
        if (FAILED(field->get_symTag(&tag)))
        {
            return {};
        }
        if (tag != SymTagData)
        {
            continue;
        }
        FieldInfo info = {};
        BSTR field_name;
        if (FAILED(field->get_name(&field_name)))
        {
            return {};
        }
        info.name = field_name;
        SysFreeString(field_name);
        LONG offset;
        if (FAILED(field->get_offset(&offset)))
        {
            return {};
        }
        info.offset = offset;
        DWORD type_obj_id;
        if (FAILED(field->get_typeId(&type_obj_id)))
        {
            return {};
        }
        auto type = GetTypeInfo(type_obj_id);
        if (!type.size)
        {
            return {};
        }
        info.type = type;
        ret.push_back(info);
    }
    structureFieldInfoCache[id] = ret;
    return ret;
}

void PDBReader::BuildSortedFunctionRVANameList()
{
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
    uint32_t current_distance = 0xffffffff;
    std::wstring current_name;
    for (int i = 0; ; i += 1)
    {
        CComPtr<IDiaSymbol> pSymbol;
        ULONG celt = 1;
        hr = pEnumSymbols->Next(1, &pSymbol, &celt);
        if ((FAILED(hr)) || (celt != 1))
        {
            break;
        }
        DWORD function_rva = 0;
        hr = pSymbol->get_relativeVirtualAddress(&function_rva);
        if (FAILED(hr))
        {
            continue;
        }
        CComBSTR tmp_name;
        hr = pSymbol->get_name(&tmp_name);
        if (FAILED(hr))
        {
            continue;
        }
        std::wstring func_name(tmp_name);
        SysFreeString(tmp_name);
        SortedFunctionRVANameList.push_back(std::make_tuple((uint32_t)function_rva, func_name));
    }

    SortedFunctionRVANameList.sort([](const std::tuple<uint32_t, std::wstring>& p1, const std::tuple<uint32_t, std::wstring>& p2) -> bool {
        return std::get<0>(p1) < std::get<0>(p2);
        });;
}
