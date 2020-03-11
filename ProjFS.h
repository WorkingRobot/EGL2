#pragma once

#define ALLOCATION_UNIT                 4096
#define PROGNAME						"EGL2"

#include <winfsp/winfsp.h>
#include <Shlwapi.h>
#include <string>

#include "filetree.h"

class ProjFS {
public:
	ProjFS();

	void Initialize(std::wstring mountPoint, uint64_t creationTimestamp, uint64_t volumeSize);

	~ProjFS();

private:
    static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM* FileSystem,
        FSP_FSCTL_VOLUME_INFO* VolumeInfo);

    static NTSTATUS SetVolumeLabel(FSP_FILE_SYSTEM* FileSystem,
        PWSTR VolumeLabel,
        FSP_FSCTL_VOLUME_INFO* VolumeInfo);

    static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM* FileSystem,
        PWSTR FileName, PUINT32 PFileAttributes,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize);

    static NTSTATUS Create(FSP_FILE_SYSTEM* FileSystem,
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
        PVOID* PFileContext, FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS Open(FSP_FILE_SYSTEM* FileSystem,
        PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
        PVOID* PFileContext, FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS Overwrite(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
        FSP_FSCTL_FILE_INFO* FileInfo);

    static VOID Cleanup(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, PWSTR FileName, ULONG Flags);

    static VOID Close(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext0);

    static NTSTATUS Read(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        PULONG PBytesTransferred);

    static NTSTATUS Write(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
        BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
        PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS Flush(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext,
        FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext,
        FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, UINT32 FileAttributes,
        UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
        FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS SetFileSize(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
        FSP_FSCTL_FILE_INFO* FileInfo);

    static NTSTATUS Rename(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext,
        PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);

    static NTSTATUS GetSecurity(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext,
        PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize);

    static NTSTATUS SetSecurity(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext,
        SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor);

    static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext0, PWSTR Pattern, PWSTR Marker,
        PVOID Buffer, ULONG BufferLength, PULONG PBytesTransferred);

    static NTSTATUS SetDelete(FSP_FILE_SYSTEM* FileSystem,
        PVOID FileContext, PWSTR FileName, BOOLEAN DeleteFile);

    FileEntry* GetFile(PWSTR fileName);

    uint64_t volumeSize;
    uint64_t fileCreationTime;

    const FSP_FILE_SYSTEM_INTERFACE callInterface =
    {
    .GetVolumeInfo = GetVolumeInfo,
    .SetVolumeLabel = SetVolumeLabel,
    .GetSecurityByName = GetSecurityByName,
    .Create = Create,
    .Open = Open,
    .Overwrite = Overwrite,
    .Cleanup = Cleanup,
    .Close = Close,
    .Read = Read,
    .Write = Write,
    .Flush = Flush,
    .GetFileInfo = GetFileInfo,
    .SetBasicInfo = SetBasicInfo,
    .SetFileSize = SetFileSize,
    .Rename = Rename,
    .GetSecurity = GetSecurity,
    .SetSecurity = SetSecurity,
    .ReadDirectory = ReadDirectory,
    .SetDelete = SetDelete,
    };
    
	FSP_FILE_SYSTEM* fileSystem;
	FileTree fileTree;
};



struct FileContextData {
    FileProvider* Provider;
    void* Context;
    PVOID DirBuffer;
    wchar_t FullName[MAX_PATH];
    FileEntry* Entry; // remove this or something soon kthx
    ProjFS* Filesystem;
};