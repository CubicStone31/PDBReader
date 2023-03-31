#pragma once
#include <string>
#include <atlbase.h>
#include <dia2.h>
#include <optional>
#include <map>
#include <list>
#include <cstdint>
#include <vector>

class PDBReader
{
public:
    enum class Types
    {
        Unknown,
        Integer,
        UInteger,
        Float,
        Double,
        Char,
        Widechar,
        Array,
        Enum,
        Class,
    };

    class TypeInfo
    {
    public:
        PDBReader* attached_pdb;
        Types type;
        std::wstring freindly_name;
        DWORD associated_type_obj_id;
        uint32_t size;
        // if type is an array
        uint32_t arr_dim;
        DWORD arr_element_type_id;
    };

    class FieldInfo
    {
    public:
        TypeInfo type;
        std::wstring name;
        uint32_t offset;
    };

    PDBReader(std::wstring pdb_name);

    PDBReader(std::wstring executable_name, std::wstring search_path);

    std::optional<DWORD> FindSymbol(std::wstring sym, DWORD& type);

    std::optional<DWORD> FindConst(std::wstring const_name);

    std::optional<DWORD> FindFunction(std::wstring func);

    std::optional<DWORD> FindStructMemberOffset(std::wstring structName, std::wstring memberName);

    std::optional<UINT64> FindStructSize(std::wstring structName);

    bool FindMostRelatedFunctionName(DWORD rva, std::wstring& funcname);

    void FindNearestSymbolFromRVA(DWORD rva, std::wstring& symbolName, DWORD& symbolType);

    void DumpTypes(enum SymTagEnum type, const std::wstring out_file);

    const std::vector<FieldInfo> GetStructureFields(const std::wstring& structName);

    const std::vector<FieldInfo> GetStructureFields(DWORD symbolId);

    const TypeInfo GetTypeInfo(DWORD symbolId);

    // Helper function
    static void DownloadPDBForFile(std::wstring executable_name, std::wstring symbol_folder, std::wstring SYMBOL_SERVER_URL = L"https://msdl.microsoft.com/download/symbols");

    static HRESULT CoInit(DWORD init_flag = COINIT_MULTITHREADED);

    static void SetMsdiaDllPath(std::wstring p);

private:
    static HRESULT CreateDiaDataSourceWithoutComRegistration(IDiaDataSource** data_source);

    static inline std::wstring dia_dll_name = L"msdia140.dll";
    static inline std::wstring dia_dll_full_path = L"";

    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaSymbol> pGlobal;

    std::map<std::wstring, DWORD> symbolRVACache;
    std::map<DWORD, TypeInfo> symbolTypeInfoCache;
    std::map<DWORD, std::vector<FieldInfo>> structureFieldInfoCache;

    const std::vector<FieldInfo> GetStructureFields(IDiaSymbol* sym);

    void BuildSortedFunctionRVANameList();

    std::list<std::tuple<uint32_t, std::wstring>> SortedFunctionRVANameList;

};
