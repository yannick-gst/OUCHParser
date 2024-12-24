#include "ouchparser.h"
#include <fstream>

int main ()
{
    std::ifstream ifs("OUCHLMM2.incoming.packets", std::ios::binary);
    while (!ifs.eof() && !ifs.fail()) {
        readPackets(ifs);
    }

    printOutput();

    return 0;
}
