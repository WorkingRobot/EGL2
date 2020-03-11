/**
 * @file memfs.h
 *
 * @copyright 2015-2020 Bill Zissimopoulos
 */
 /*
  * This file is part of WinFsp.
  *
  * You can redistribute it and/or modify it under the terms of the GNU
  * General Public License version 3 as published by the Free Software
  * Foundation.
  *
  * Licensees holding a valid commercial license may use this software
  * in accordance with the commercial license agreement provided in
  * conjunction with the software.  The terms and conditions of any such
  * commercial license agreement shall govern, supersede, and render
  * ineffective any application of the GPLv3 license to this software,
  * notwithstanding of any reference thereto in the software or
  * associated repository.
  */

#ifndef MEMFS_H_INCLUDED
#define MEMFS_H_INCLUDED

#include <winfsp/winfsp.h>
#include <functional>

#define MEMFS_MAX_PATH                  512
FSP_FSCTL_STATIC_ASSERT(MEMFS_MAX_PATH > MAX_PATH,
    "MEMFS_MAX_PATH must be greater than MAX_PATH.");

#define MEMFS_SECTOR_SIZE               512
#define MEMFS_SECTORS_PER_ALLOCATION_UNIT 1

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct _MEMFS MEMFS;

    typedef struct _MEMFS_FILE_PROVIDER MEMFS_FILE_PROVIDER;

    enum
    {
        MemfsDisk = 0x00000000,
        MemfsNet = 0x00000001,
        MemfsDeviceMask = 0x0000000f,
        MemfsCaseInsensitive = 0x80000000,
        MemfsFlushAndPurgeOnCleanup = 0x40000000,
    };

    NTSTATUS MemfsCreateFunnel(
        ULONG Flags,
        ULONG FileInfoTimeout,
        ULONG MaxFileNodes,
        ULONG MaxFileSize,
        ULONG SlowioMaxDelay,
        ULONG SlowioPercentDelay,
        ULONG SlowioRarefyDelay,
        PWSTR FileSystemName,
        PWSTR VolumePrefix,
        PWSTR RootSddl,
        MEMFS_FILE_PROVIDER* FileProvider,
        MEMFS** PMemfs);
    VOID MemfsDelete(MEMFS* Memfs);
    NTSTATUS MemfsStart(MEMFS* Memfs);
    VOID MemfsStop(MEMFS* Memfs);
    FSP_FILE_SYSTEM* MemfsFileSystem(MEMFS* Memfs);

    MEMFS_FILE_PROVIDER* CreateProvider(
        std::function<PVOID(PCWSTR fileName)> Open,
        std::function<void(PVOID Handle)> Close,
        std::function<UINT64(PCWSTR fileName)> GetSize,
        std::function<void(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead)> Read);
    void CloseProvider(MEMFS_FILE_PROVIDER* Provider);

    NTSTATUS CreateFsFile(MEMFS* Memfs,
        PWSTR FileName, BOOLEAN Directory);

#ifdef __cplusplus
}
#endif

#endif

