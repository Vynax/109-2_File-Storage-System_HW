#include "tripleindirect.h"
#include <cstring>
#include "pbitmap.h"
#include "debug.h"
#include "synchdisk.h"
#include "main.h"
#include <cmath>

TripleIndirect::TripleIndirect()
{
    memset(DISectors, -1, sizeof(DISectors));
}

TripleIndirect::~TripleIndirect() {}

bool TripleIndirect::Allocate(PersistentBitmap *freeMap, int sectorAmount)
{
    int numSectors = sectorAmount;

    DISize = pow(SectorSize / sizeof(int), 2); // sectors per DoubleIndirect
    numDoubleIndirect = sectorAmount / DISize + !!(sectorAmount % DISize);
    if (freeMap->NumClear() < numDoubleIndirect + sectorAmount)
        return FALSE; // not enough space

    // for original inode
    for (int i = 0; i < numDoubleIndirect; i++)
    {
        DISectors[i] = freeMap->FindAndSet();
        // since we checked that there was enough free space,
        // we expect this to succeed
        ASSERT(DISectors[i] >= 0);
    }

    // for DoubleIndirect
    if (numDoubleIndirect)
        table = new DoubleIndirect[numDoubleIndirect];
    for (int i = 0; i < numDoubleIndirect; i++)
    {
        if (numSectors > DISize)
            table[i].Allocate(freeMap, DISize);
        else
            table[i].Allocate(freeMap, numSectors);

        numSectors = numSectors - DISize;
    }
    return TRUE;
}

void TripleIndirect::Deallocate(PersistentBitmap *freeMap)
{
    for (int i = 0; i < numDoubleIndirect; i++)
    {
        table[i].Deallocate(freeMap);
        ASSERT(freeMap->Test((int)DISectors[i])); // ought to be marked!
        freeMap->Clear((int)DISectors[i]);
    }
}

void TripleIndirect::FetchFrom(int sector)
{
    kernel->synchDisk->ReadSector(sector, (char *)this);

    DISize = pow(SectorSize / sizeof(int), 2); // sectors per SingleIndirect

    numDoubleIndirect = 0;
    while ((DISectors[numDoubleIndirect] > -1) && DISectors[numDoubleIndirect] < NumSectors && numDoubleIndirect < 32)
    {
        numDoubleIndirect++;
    }

    table = new DoubleIndirect[numDoubleIndirect];

    for (int i = 0; i < numDoubleIndirect; i++)
    {
        table[i].FetchFrom(DISectors[i]);
    }
}

void TripleIndirect::WriteBack(int sector)
{
    kernel->synchDisk->WriteSector(sector, (char *)this);

    for (int i = 0; i < numDoubleIndirect; i++)
    {
        table[i].WriteBack(DISectors[i]);
    }
}

int TripleIndirect::ByteToSector(int offset)
{
    // targetDoubleIndirectSector
    int target_Sector = offset / SectorSize;
    int Max_Pointer_Of_Sector = pow(SectorSize / sizeof(int), 2);
    int target_sector_divid = target_Sector / Max_Pointer_Of_Sector;
    int target_sector_remain = target_Sector % Max_Pointer_Of_Sector;

    return table[target_sector_divid].ByteToSector(target_sector_remain * SectorSize);
}