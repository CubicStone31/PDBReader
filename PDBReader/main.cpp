#include "PDBReader/pdbreader.h"
#include <iostream>

int main() 
{
    try
    {
        PDBReader::CoInit();

        PDBReader reader2(L"F:\\test.pdb");  
        std::wstring symbol;
        DWORD type = 0;
        reader2.FindNearestSymbolFromRVA(0x810970, symbol, type);
        std::wcout << L"Name: " << symbol.c_str() << std::endl;
        std::cout << "Type: " << type << std::endl;
     

        auto value = reader2.FindFunction(L"name");
        if (!value)
        {
            std::cout << "Not Found.\n";
        }
        else
        {
            std::cout << value.value() << std::endl;
        }

        system("pause");
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}