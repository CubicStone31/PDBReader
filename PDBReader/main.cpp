#include "PDBReader/pdbreader.h"
#include <iostream>

int main() 
{
    try
    {
        PDBReader::CoInit();
        //std::cout << "Start downloading symbol files\n";
        //PDBReader::DownloadPDBForFile(L"C:\\windows\\system32\\ntoskrnl.exe", L"Symbols");
        //std::cout << "Download pdb succeed." << std::endl;
        //PDBReader reader(L"C:\\windows\\system32\\ntoskrnl.exe", L"Symbols");
        //auto offset = reader.FindStructMemberOffset(L"_EPROCESS", L"Protection");
        //if (!offset)
        //{
        //    std::cout << "Failed to find target symbol.\n";
        //    return 0;
        //}
        //std::cout << "Offset of Protection field in EPROCES: " << offset.value() << std::endl;

        PDBReader reader(L"test.pdb");
        reader.DumpTypes(SymTagEnum::SymTagPublicSymbol, L"D:\\info.txt");
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}