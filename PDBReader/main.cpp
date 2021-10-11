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
            return 0;
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