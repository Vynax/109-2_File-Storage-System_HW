#include "doubleindirect.h"
#include <cstring>
#include "pbitmap.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"

DoubleIndirect::DoubleIndirect()
{
    memset(SISectors, -1, sizeof(SISectors));
}

DoubleIndirect::~DoubleIndirect() {}

bool DoubleIndirect::Allocate(PersistentBitmap *freeMap, int sectorAmount)
{
    int numSectors = sectorAmount;

    SISize = SectorSize / sizeof(int); // sectors per SingleIndirect
    numSingleIndirect = sectorAmount / SISize + !!(sectorAmount % SISize);
    if (freeMap->NumClear() < numSingleIndirect + sectorAmount)
        return FALSE; // not enough space

    // for original inode
    for (int i = 0; i < numSingleIndirect; i++)
    {
        SISectors[i] = freeMap->FindAndSet();
        // since we checked that there was enough free space,
        // we expect this to succeed
        ASSERT(SISectors[i] >= 0);
    }

    // for singleIndirect
    if (numSingleIndirect)
        table = new SingleIndirect[numSingleIndirect];
    for (int i = 0; i < numSingleIndirect; i++)
    {
        //table[i] = SingleIndirect();
        if (numSectors > SISize)
            table[i].Allocate(freeMap, SISize);
        else
            table[i].Allocate(freeMap, numSectors);

        numSectors = numSectors - SISize;
    }
    return TRUE;
}

void DoubleIndirect::Deallocate(PersistentBitmap *freeMap)
{
    for (int i = 0; i < numSingleIndirect; i++)
    {
        ASSERT(freeMap->Test((int)SISectors[i])); // ought to be marked!
        freeMap->Clear((int)SISectors[i]);
        table[i].Deallocate(freeMap);
    }
}

void DoubleIndirect::FetchFrom(int sector)
{
    kernel->synchDisk->ReadSector(sector, (char *)this);

    SISize = SectorSize / sizeof(int); // sectors per SingleIndirect

    numSingleIndirect = 0;
    while ((SISectors[numSingleIndirect] > -1) && SISectors[numSingleIndirect] < NumSectors && numSingleIndirect < SISize)
    {
        numSingleIndirect++;
    }

    table = new SingleIndirect[numSingleIndirect];

    for (int i = 0; i < numSingleIndirect; i++)
    {
        table[i].FetchFrom(SISectors[i]);
    }
}

void DoubleIndirect::WriteBack(int sector)
{
    kernel->synchDisk->WriteSector(sector, (char *)this);

    for (int i = 0; i < numSingleIndirect; i++)
    {
        table[i].WriteBack(SISectors[i]);
    }
}

int DoubleIndirect::ByteToSector(int offset)
{
    // targetSingleIndirectSector
    int targetSIS = offset / SectorSize;
    int Max_Pointer_Of_Sector = SectorSize / sizeof(int);
    int target_sector_divid = targetSIS / Max_Pointer_Of_Sector;
    int target_sector_remain = targetSIS % Max_Pointer_Of_Sector;

    return table[target_sector_divid].ByteToSector(target_sector_remain * SectorSize);
}