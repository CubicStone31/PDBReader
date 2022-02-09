#include "PDBReader/pdbreader.h"
#include <iostream>

int main() 
{
    try
    {
        PDBReader::CoInit();

        PDBReader reader2(L"F:\\test.pdb");  
        for (int i = 0; i < 100; i++)
        {
            auto t1 = GetTickCount();
            reader2.FindFunction(L"_oi_symmetry_encrypt2");
            auto t2 = GetTickCount();
            std::cout << "time " << t2 - t1 << " for " << i << " loop\n";
        }



        system("pause");
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}