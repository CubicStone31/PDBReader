# PDBReader

Automatically download pdb file from windows symbol server and read symbol data from it.

## How to use

add pdbreader.cpp and pdbreader.h to your project, and include pdbreader.h

Add dia2.h to your include directory, and link to diaguids.lib. These files should be found at dia sdk, which is normally at "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\DIA SDK\".

When your application runs, make sure `msdia140.dll` and `symsrv.dll` are placed in your application's working directory. They should be found at dia sdk and windbg sdk.

You can also found these files at "3rdparty" folder of this project.

## Interfaces

```c
std::optional<DWORD> FindSymbol(std::wstring sym, DWORD& type);

std::optional<DWORD> FindConst(std::wstring const_name);

std::optional<DWORD> FindFunction(std::wstring func);

std::optional<DWORD> FindStructMemberOffset(std::wstring structName, std::wstring memberName);

std::optional<UINT64> FindStructSize(std::wstring structName);

void FindNearestSymbolFromRVA(DWORD rva, std::wstring& symbolName, DWORD& symbolType);

static void DownloadPDBForFile(std::wstring executable_name, std::wstring symbol_folder, std::wstring SYMBOL_SERVER_URL = L"https://msdl.microsoft.com/download/symbols");
```

## Example for usage

```c
#include "PDBReader/pdbreader.h"
#include <iostream>

int main() 
{
    try
    {
        PDBReader::CoInit();
        std::cout << "Start downloading symbol files\n";
        PDBReader::DownloadPDBForFile(L"C:\\windows\\system32\\ntoskrnl.exe", L"Symbols");
        std::cout << "Download pdb succeed." << std::endl;
        PDBReader reader(L"C:\\windows\\system32\\ntoskrnl.exe", L"Symbols");
        auto offset = reader.FindStructMemberOffset(L"_EPROCESS", L"Protection");
        if (!offset)
        {
            std::cout << "Failed to find target symbol.\n";
            return 0;
        }
        std::cout << "Offset of Protection field in EPROCES: " << offset.value() << std::endl;
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}
```

## Supported platform

The program is tested on Windows 10 x64 and x86, but should be woking on any version of windows.
