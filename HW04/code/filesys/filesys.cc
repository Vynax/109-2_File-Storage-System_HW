// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#ifndef FILESYS_STUB

#include "copyright.h"
#include "debug.h"
#include "disk.h"
#include "pbitmap.h"
#include "directory.h"
#include "filehdr.h"
#include "filesys.h"
#include <vector>

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define DirectorySector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format)
{
    DEBUG(dbgFile, "Initializing the file system.");
    if (format)
    {
        cout << "FreeMapFileSize : " << 65536 << endl;
        // cout << "FreeMapFileSize : " << FreeMapFileSize << endl;
        cout << "DirectoryFileSize : " << DirectoryFileSize + 4 << endl;
        PersistentBitmap *freeMap = new PersistentBitmap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG(dbgFile, "Formatting the file system.");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(DirectorySector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG(dbgFile, "Writing headers back to disk.");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(DirectorySector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG(dbgFile, "Writing bitmap and directory back to disk.");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);

        if (debug->IsEnabled('f'))
        {
            freeMap->Print();
            directory->Print();
        }
        delete freeMap;
        delete directory;
        delete mapHdr;
        delete dirHdr;
    }
    else
    {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(DirectorySector);
    }

    currentDirectoryFile = directoryFile;
    currentDirectory = new Directory(NumDirEntries);
    currentDirectory->FetchFrom(currentDirectoryFile);
}

//----------------------------------------------------------------------
// MP4 mod tag
// FileSystem::~FileSystem
//----------------------------------------------------------------------
FileSystem::~FileSystem()
{
    delete freeMapFile;
    if (directoryFile == currentDirectoryFile)
        delete directoryFile;
    else
    {
        delete directoryFile;
        delete currentDirectoryFile;
    }
    delete currentDirectory;
}

vector<string> FileSystem::path_Parser(char *name)
{
    int i = 0;
    int slash_count = 0;
    string temp = "";
    vector<string> path_name;
    while (name[i] != '\0')
    {
        if (name[i] == '/')
        {
            if (slash_count != 0) // if slash_count = 0 , that means it's first '/'
            {
                path_name.push_back(temp);
                temp = "";
            }
            slash_count++;
        }
        else
            temp += name[i];
        i++;
    }

    if (temp != "")
        path_name.push_back(temp);
    return path_name;
}

bool FileSystem::changeCurrenDir(char *name, bool lastname)
{
    vector<string> path_name = path_Parser(name);
    int sector;
    bool success = true;
    int limit;
    if (lastname) // need the last name of path
        limit = path_name.size();
    else // doesn't need the last name of path
        limit = path_name.size() - 1;
    DEBUG(dbgFile, "Changing Current Directory " << name);

    if (name[0] == '/')
    {
        //OpenFile &tempDirectoryFile = *currentDirectoryFile; // save pointer
        currentDirectoryFile = directoryFile;
        currentDirectory->FetchFrom(currentDirectoryFile);
        for (int i = 0; i < limit && success == true; i++)
        {
            char *temp_c_str = new char[path_name[i].length() + 1];
            strcpy(temp_c_str, path_name[i].c_str());
            sector = currentDirectory->Find(temp_c_str);
            if (sector == -1) // can't find the path
            {
                std::cout << "No such Directory :";
                for (int j = 0; j <= i; j++)
                    std::cout << "/" << path_name[j];

                std::cout << std::endl;
                success = false;
            }
            else
            {
                if (currentDirectoryFile != directoryFile)
                {
                    delete currentDirectoryFile;
                    delete currentDirectory;
                }
                currentDirectoryFile = new OpenFile(sector);
                currentDirectory = new Directory(NumDirEntries);
                currentDirectory->FetchFrom(currentDirectoryFile);
            }
        }
    }

    return success;
}

void FileSystem::closeCurrentDir()
{
    if (currentDirectoryFile != directoryFile)
    {
        delete currentDirectoryFile;
        delete currentDirectory;

        currentDirectoryFile = directoryFile;
        currentDirectory = new Directory(NumDirEntries);
        currentDirectory->FetchFrom(currentDirectoryFile);
    }
}

bool FileSystem::Mkdir(char *name)
{
    vector<string> path_name = path_Parser(name);
    Directory *newDirectory;
    OpenFile *newDirectoryFile;
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success = true;

    DEBUG(dbgFile, "Creating Directory " << name);

    if (path_name.size() <= 0)
    {
        std::cout << "root donen't need to be made" << std::endl;
        return false;
    }
    else if (!changeCurrenDir(name, false))
    {
        std::cout << "Failed on changing current directory" << std::endl;
        closeCurrentDir();
        return false;
    }

    char *temp_c_str = new char[path_name[path_name.size() - 1].length() + 1];
    strcpy(temp_c_str, path_name[path_name.size() - 1].c_str());
    if (currentDirectory->Find(temp_c_str) != -1) //folder already exist
    {
        std::cout << "Directory already existed :" << name << std::endl;
        success = false;
    }
    else
    {
        freeMap = new PersistentBitmap(freeMapFile, NumSectors);
        sector = freeMap->FindAndSet(); // find a sector to hold the file header
        if (sector == -1)
        {
            success = FALSE; // no free block for file header
            std::cout << "no free block for file header" << std::endl;
        }
        else if (!currentDirectory->Add(temp_c_str, sector, DIRECTORY_TYPE))
        {
            success = FALSE; // no space in directory
            std::cout << "no space in directory" << std::endl;
        }
        else
        {
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, DirectoryFileSize))
            {
                success = FALSE; // no space on disk for data
                std::cout << "no space on disk for data" << std::endl;
            }
            else
            {
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector);
                currentDirectory->WriteBack(currentDirectoryFile);
                freeMap->WriteBack(freeMapFile);

                newDirectoryFile = new OpenFile(sector);
                newDirectory = new Directory(NumDirEntries);
                newDirectory->WriteBack(newDirectoryFile);

                delete newDirectory;
                delete newDirectoryFile;
            }
            delete hdr;
        }
        delete freeMap;
    }

    delete[] temp_c_str;

    closeCurrentDir();
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file doesn't already exist
//        Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
//   		file is already in directory
//	 	no free space for file header
//	 	no free entry for file in directory
//	 	no free space for data blocks for the file
//
// 	Note that this implementation assumes there is no concurrent access
//	to the file system!
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(char *name, int initialSize)
{
    vector<string> path_name = path_Parser(name);
    PersistentBitmap *freeMap;
    FileHeader *hdr;
    int sector;
    bool success;

    DEBUG(dbgFile, "Creating file " << name << " size " << initialSize);

    if (path_name.size() <= 0)
    {
        std::cout << "root donen't need to be made" << std::endl;
        return false;
    }
    else if (!changeCurrenDir(name, false))
    {
        std::cout << "Failed on changing current directory" << std::endl;
        closeCurrentDir();
        return false;
    }

    char *temp_c_str = new char[path_name[path_name.size() - 1].length() + 1];
    strcpy(temp_c_str, path_name[path_name.size() - 1].c_str());

    if (currentDirectory->Find(temp_c_str) != -1) // file is already in directory
    {
        std::cout << "File is already in directory :" << name << std::endl;
        success = FALSE;
    }
    else
    {
        freeMap = new PersistentBitmap(freeMapFile, NumSectors);
        sector = freeMap->FindAndSet(); // find a sector to hold the file header
        if (sector == -1)
            success = FALSE; // no free block for file header
        else if (!currentDirectory->Add(temp_c_str, sector, FILE_TYPE))
            success = FALSE; // no space in directory
        else
        {
            hdr = new FileHeader;
            if (!hdr->Allocate(freeMap, initialSize))
                success = FALSE; // no space on disk for data
            else
            {
                success = TRUE;
                // everthing worked, flush all changes back to disk
                hdr->WriteBack(sector);
                currentDirectory->WriteBack(currentDirectoryFile);
                freeMap->WriteBack(freeMapFile);
            }
            delete hdr;
        }
        delete freeMap;
    }
    return success;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//
//	"name" -- the text name of the file to be opened
//----------------------------------------------------------------------

OpenFile *FileSystem::Open(char *name)
{
    vector<string> path_name = path_Parser(name);
    OpenFile *openFile = NULL;
    int sector;

    changeCurrenDir(name, false);

    char *temp_c_str = new char[path_name[path_name.size() - 1].length() + 1];
    strcpy(temp_c_str, path_name[path_name.size() - 1].c_str());

    DEBUG(dbgFile, "Opening file" << name);
    sector = currentDirectory->Find(temp_c_str);
    if (sector >= 0)
        openFile = new OpenFile(sector); // name was found in directory

    delete temp_c_str;
    return openFile; // return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(char *name)
{
    vector<string> path_name = path_Parser(name);
    PersistentBitmap *freeMap;
    FileHeader *fileHdr;
    int sector;
    int index_dir;

    DEBUG(dbgFile, "Removing File : " << name);

    if (path_name.size() <= 0)
    {
        std::cout << "root donen't need to be removed" << std::endl;
        return false;
    }
    else if (!changeCurrenDir(name, false))
    {
        std::cout << "Failed on changing current directory" << std::endl;
        closeCurrentDir();
        return false;
    }

    char *temp_c_str = new char[path_name[path_name.size() - 1].length() + 1];
    strcpy(temp_c_str, path_name[path_name.size() - 1].c_str());

    sector = currentDirectory->Find(temp_c_str);
    if (sector == -1)
    {
        closeCurrentDir();
        return FALSE; // file not found
    }
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    freeMap = new PersistentBitmap(freeMapFile, NumSectors);

    fileHdr->Deallocate(freeMap); // remove data blocks
    freeMap->Clear(sector);       // remove header block
    currentDirectory->Remove(temp_c_str);

    freeMap->WriteBack(freeMapFile);                   // flush to disk
    currentDirectory->WriteBack(currentDirectoryFile); // flush to disk
    delete fileHdr;
    delete freeMap;
    closeCurrentDir();
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List(char *path)
{
    changeCurrenDir(path, true);
    currentDirectory->List(false);

    closeCurrentDir();
}

void FileSystem::ListRecur(char *path)
{
    Directory *directory = new Directory(NumDirEntries);

    directory->FetchFrom(directoryFile);
    directory->List(true);
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print()
{
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    PersistentBitmap *freeMap = new PersistentBitmap(freeMapFile, NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMap->Print();

    directory->FetchFrom(directoryFile);
    directory->Print();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}

#endif // FILESYS_STUB
