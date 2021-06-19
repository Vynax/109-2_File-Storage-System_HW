#pragma once

#include "disk.h"
#include "pbitmap.h"
#include "singleindirect.h"

class DoubleIndirect
{
public:
    DoubleIndirect();
    ~DoubleIndirect();

    bool Allocate(PersistentBitmap *bitMap, int sectorAmount); // Initialize a file header,
                                                               //  including allocating space
                                                               //  on disk for the file data
    void Deallocate(PersistentBitmap *bitMap);                 // De-allocate this file's
                                                               //  data blocks

    void FetchFrom(int sectorNumber); // Initialize file header from disk
    void WriteBack(int sectorNumber); // Write modifications to file header
                                      //  back to disk

    int ByteToSector(int offset); // Convert a byte offset into the file
                                  // to the disk sector containing
                                  // the byte
private:
    int SISectors[SectorSize / sizeof(int)]; // SingleIndirectSectors
    SingleIndirect *table;
    int SISize; // sectors per singleIndirect
    int numSingleIndirect;
};