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
        auto ret = reader.GetStructureFields(L"xxx");
        for (auto& f : ret)
        {
            printf("%ws, offset %d, size %d, %ws\n", f.type.freindly_name.c_str(), f.offset, f.type.size, f.name.c_str());
            if (f.type.type == PDBReader::Types::Class)
            {
                printf("=============\n");
                auto classinfo = reader.GetStructureFields(f.type.associated_type_obj_id);
                for (auto& f2 : classinfo)
                {
                    printf("%ws, offset %d, size %d, %ws\n", f2.type.freindly_name.c_str(), f2.offset, f2.type.size, f2.name.c_str());
                }
                printf("=============\n");
            }
        }
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
    }
    return 0;
}