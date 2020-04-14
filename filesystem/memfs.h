#pragma once

#define MEMFS_MAX_PATH                  512
#define MEMFS_SECTOR_SIZE               512
#define MEMFS_SECTORS_PER_ALLOCATION_UNIT 1

#include <winfsp/winfsp.h>

#include <functional>

FSP_FSCTL_STATIC_ASSERT(MEMFS_MAX_PATH > MAX_PATH,
    "MEMFS_MAX_PATH must be greater than MAX_PATH.");

typedef struct _MEMFS MEMFS;
typedef struct _MEMFS_FILE_PROVIDER MEMFS_FILE_PROVIDER;

enum
{
    MemfsDisk = 0x00000000,
    MemfsNet = 0x00000001,
    MemfsDeviceMask = 0x0000000f,
    MemfsCaseInsensitive = 0x80000000,
};

NTSTATUS MemfsCreateFunnel(
    ULONG Flags,
    ULONG FileInfoTimeout,
    ULONG MaxFileNodes,
    PWSTR FileSystemName,
    PWSTR VolumePrefix,
    PWSTR VolumeLabel,
    UINT64 VolumeTotal,
    UINT64 VolumeFree,
    PVOID Security,
    SIZE_T SecuritySize,
    MEMFS_FILE_PROVIDER* FileProvider,
    MEMFS** PMemfs);
VOID MemfsDelete(MEMFS* Memfs);
NTSTATUS MemfsStart(MEMFS* Memfs);
VOID MemfsStop(MEMFS* Memfs);
FSP_FILE_SYSTEM* MemfsFileSystem(MEMFS* Memfs);

MEMFS_FILE_PROVIDER* CreateProvider(
    std::function<PVOID(PCWSTR fileName, UINT64* fileSize)> Open,
    std::function<void(PVOID Handle)> Close,
    std::function<void(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead)> Read);
void CloseProvider(MEMFS_FILE_PROVIDER* Provider);

NTSTATUS CreateFsFile(MEMFS* Memfs,
    PWSTR FileName, BOOLEAN Directory);


