#include "disk.h"

class SingleIndirect
{
public:
    int dataSectors[SectorSize / sizeof(int)];
};