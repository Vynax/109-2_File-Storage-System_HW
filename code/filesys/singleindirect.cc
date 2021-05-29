#include "singleindirect.h"
#include <cstring>
#include "pbitmap.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

SingleIndirect::SingleIndirect()
{
    memset(dataSectors, -1, sizeof(dataSectors));
}

SingleIndirect::~SingleIndirect() {}

bool SingleIndirect::Allocate(PersistentBitmap *freeMap, int sectorAmount)
{
    numSectors = sectorAmount;
    for (int i = 0; i < sectorAmount; i++)
    {
        dataSectors[i] = freeMap->FindAndSet();
        // since we checked that there was enough free space,
        // we expect this to succeed
        ASSERT(dataSectors[i] >= 0);
    }
    return true;
}

void SingleIndirect::Deallocate(PersistentBitmap *freeMap)
{
    for (int i = 0; i < numSectors; i++)
    {
        ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
        freeMap->Clear((int)dataSectors[i]);
    }
}

void SingleIndirect::FetchFrom(int sector)
{
    kernel->synchDisk->ReadSector(sector, (char *)this);

    // int i = 0;
    int size = SectorSize / sizeof(int); // sectors per singleIndirect
    numSectors = 0;
    while ((dataSectors[numSectors] != -1) && numSectors < size)
        numSectors++;
}

void SingleIndirect::WriteBack(int sector)
{
    kernel->synchDisk->WriteSector(sector, (char *)this);
}

int SingleIndirect::ByteToSector(int offset)
{
    return (dataSectors[offset / SectorSize]);
}