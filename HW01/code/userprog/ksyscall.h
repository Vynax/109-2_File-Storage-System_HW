/**************************************************************
 *
 * userprog/ksyscall.h
 *
 * Kernel interface for systemcalls 
 *
 * by Marcus Voelp  (c) Universitaet Karlsruhe
 *
 **************************************************************/

#ifndef __USERPROG_KSYSCALL_H__
#define __USERPROG_KSYSCALL_H__

#include "kernel.h"

#include "synchconsole.h"

void SysHalt()
{
    kernel->interrupt->Halt();
}

int SysAdd(int op1, int op2)
{
    return op1 + op2;
}

int SysCreate(char *filename)
{
    // return value
    // 1: success
    // 0: failed
    return kernel->interrupt->CreateFile(filename);
}

// =================================below is my code=================================================

int SysRead(char *buffer, int size, OpenFileId id)
{
    // std::cout << "Read????" << std::endl;
    OpenFile *file = (OpenFile *)id;
    return file->Read(buffer, size);
}

int SysClose(OpenFileId id)
{
    OpenFile *file = (OpenFile *)id;
    try
    {
        file->~OpenFile();
    }
    catch (std::string e)
    {
        throw e;
        return 0;
    }
    return 1;
}

int SysWrite(char *buffer, int size, OpenFileId id)
{
    OpenFile *file = (OpenFile *)id;
    return file->Write(buffer, size);
}

OpenFileId SysOpen(char *name)
{
    return (int)(kernel->fileSystem->Open(name));
}

void SysPrintInt(int n)
{
    kernel->synchConsoleOut->PrintInt(n);
}

// =================================above is my code=================================================

#endif /* ! __USERPROG_KSYSCALL_H__ */
