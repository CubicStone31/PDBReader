# PDBReader

Automatically download pdb file from windows symbol server and read symbol data from it.

## Interfaces

```c
static void DownloadPDBForFile(std::wstring executable_name, std::wstring symbol_folder);

std::optional<size_t> FindSymbol(std::wstring sym, DWORD& type); // Find a global symbol by name and return its address and type

std::optional<size_t> FindConst(std::wstring const_name);

std::optional<size_t> FindFunction(std::wstring func);

std::optional<LONG> FindStructMemberOffset(std::wstring structName, std::wstring memberName);
```

## Example for usage

```c
#include "PDBReader/pdbreader.h"
#include <iostream>

int main() 
{
    try
    {
        PDBReader::COINIT(COINIT_APARTMENTTHREADED);

        std::cout << "Start downloading symbol files\n";
        PDBReader::DownloadPDBForFile(L"C:\\windows\\system32\\ntoskrnl.exe", L"Symbols");
        std::cout << "Download pdb succeed." << std::endl;

        PDBReader reader2(L"C:\\windows\\system32\\ntoskrnl.exe", L"Symbols");
        auto offset = reader2.FindStructMemberOffset(L"_EPROCESS", L"Protection");
        if (!offset)
        {
            std::cout << "Failed to find target symbol.\n";
            return;
        }
        std::cout << "Offset of Protection field in EPROCES: " << offset.value() << std::endl;

        system("pause");
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}
```

You should also place `msdia140.dll` and `symsrv.dll` in the same folder as your application's working directory

## About

Only x64 binary are supported.
