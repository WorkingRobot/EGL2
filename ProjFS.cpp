#include "ProjFS.h"

ProjFS::ProjFS()
{
    FileTree_InitializeFolder(&fileTree);
    auto folder = FileTree_AddFolder(&fileTree, L"whomegalul2");
    FileTree_AddFolder(folder, L"interesting");

    folder = FileTree_AddFolder(&fileTree, L"whomegalul");
    folder = FileTree_AddFolder(folder, L"meanie");
    FileTree_AddFolder(folder, L"meanie2");
}

void ProjFS::Initialize(std::wstring mountPoint, uint64_t creationTimestamp, uint64_t volumeSize) {
    FSP_FSCTL_VOLUME_PARAMS volumeParams = { 0 };
	volumeParams.SectorSize = ALLOCATION_UNIT;
	volumeParams.SectorsPerAllocationUnit = 1;
	{
		FILETIME ft;
		LONGLONG ll = Int32x32To64(creationTimestamp, 10000000) + 116444736000000000;
		ft.dwLowDateTime = (DWORD)ll;
		ft.dwHighDateTime = ll >> 32;
        this->fileCreationTime = ((PLARGE_INTEGER)&ft)->QuadPart;
        this->volumeSize = volumeSize;
		volumeParams.VolumeCreationTime = this->fileCreationTime;
		volumeParams.VolumeSerialNumber = 0;
		volumeParams.FileInfoTimeout = 1000;
		volumeParams.CaseSensitiveSearch = 0;
		volumeParams.CasePreservedNames = 1;
		volumeParams.UnicodeOnDisk = 1;
		volumeParams.PersistentAcls = 1;
		volumeParams.PostCleanupWhenModifiedOnly = 1;
		volumeParams.PassQueryDirectoryPattern = 1;
		volumeParams.FlushAndPurgeOnCleanup = 1;
		volumeParams.UmFileContextIsUserContext2 = 1;

        wcscpy_s(volumeParams.FileSystemName, sizeof volumeParams.FileSystemName / sizeof(WCHAR),
            L"" PROGNAME);
	}
	FspFileSystemCreate(L"" FSP_FSCTL_DISK_DEVICE_NAME, &volumeParams, &this->callInterface, &this->fileSystem);
	this->fileSystem->UserContext = this;

	FspFileSystemSetMountPoint(this->fileSystem, const_cast<PWSTR>(mountPoint.c_str()));

    FspFileSystemSetDebugLog(this->fileSystem, -1);

	FspFileSystemStartDispatcher(this->fileSystem, 0);
}

ProjFS::~ProjFS() {
	FspFileSystemStopDispatcher(this->fileSystem);
	FspFileSystemDelete(this->fileSystem);
    printf("closed up!\n");
}

FileEntry* ProjFS::GetFile(PWSTR fileName)
{
    wprintf(L"GetFile %s\n", fileName);
    wchar_t* context = NULL;
    auto filepart = wcstok_s(fileName, L"\\", &context);
    auto tree = &this->fileTree;
    bool findSuccess;
    while (filepart != NULL)
    {
        findSuccess = false;
        for (auto& folder : tree->folders)
        {
            if (!wcscmp(filepart, folder->value->name.c_str())) {
                findSuccess = true;
                tree = folder.get();
                break;
            }
        }
        if (!findSuccess) {
            for (auto& file : tree->files)
            {
                if (!wcscmp(filepart, file->name.c_str())) {
                    return file.get();
                }
            }
            return nullptr;
        }
        //if (wcschr(filepart, L'\\')) { // more directory paths
        //    
        //}
        filepart = wcstok_s(NULL, L"\\", &context);
    }
    if (!wcscmp(tree->value->name.c_str(), L"meanie")) {
        wprintf(L"found the thing!\n");
    }
    wprintf(L"GetFile returning %s\n", tree->value->name.c_str());
    return tree->value.get();
}

NTSTATUS ProjFS::GetVolumeInfo(FSP_FILE_SYSTEM* FileSystem,
	FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
	ProjFS* Context = (ProjFS*)FileSystem->UserContext;

	VolumeInfo->TotalSize = Context->volumeSize;
	VolumeInfo->FreeSize = 0;

	return STATUS_SUCCESS;
}

NTSTATUS ProjFS::SetVolumeLabel(FSP_FILE_SYSTEM* FileSystem,
	PWSTR VolumeLabel,
	FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
	// unsupported
	return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::GetSecurityByName(FSP_FILE_SYSTEM* FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
    ProjFS* Context = (ProjFS*)FileSystem->UserContext;

    auto Entry = Context->GetFile(FileName);
    if (!Entry)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (0 != PFileAttributes)
    {
        *PFileAttributes = Entry->fileAttributes;
    }

    if (0 != PSecurityDescriptorSize)
    {
        SecurityDescriptor = NULL; // hope this is valid?
        *PSecurityDescriptorSize = 0;
    }

    return STATUS_SUCCESS;
}

NTSTATUS ProjFS::Create(FSP_FILE_SYSTEM* FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID* PFileContext, FSP_FSCTL_FILE_INFO* FileInfo)
{
    wprintf(L"creating %s\n", FileName);
    ProjFS* Context = (ProjFS*)FileSystem->UserContext;

    wprintf(L"finding %s\n", FileName);
    auto Entry = Context->GetFile(FileName);
    if (!Entry)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (!Entry->fileProvider) {
        return STATUS_OBJECT_NAME_INVALID;
    }
    FileContextData* fileContext = new FileContextData;
    fileContext->Provider = Entry->fileProvider;
    fileContext->Context = fileContext->Provider->OpenFile(Entry);
    fileContext->Entry = Entry;
    fileContext->Filesystem = Context;
    fileContext->DirBuffer = NULL;
    FileTree_GetFullName(Entry, fileContext->FullName);
    *PFileContext = fileContext;

    FileInfo->FileAttributes = Entry->fileAttributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize = Entry->fileProvider->fileSize;
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = Context->fileCreationTime;
    FileInfo->LastAccessTime = Context->fileCreationTime;
    FileInfo->LastWriteTime = Context->fileCreationTime;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;
}

NTSTATUS ProjFS::Open(FSP_FILE_SYSTEM* FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    PVOID* PFileContext, FSP_FSCTL_FILE_INFO* FileInfo)
{
    wprintf(L"opening %s\n", FileName);
    ProjFS* Context = (ProjFS*)FileSystem->UserContext;

    wprintf(L"finding %s\n", FileName);
    auto Entry = Context->GetFile(FileName);
    if (!Entry)
        return STATUS_OBJECT_NAME_NOT_FOUND;

    if (!Entry->fileProvider) {
        return STATUS_OBJECT_NAME_INVALID;
    }
    FileContextData* fileContext = new FileContextData;
    fileContext->Provider = Entry->fileProvider;
    fileContext->Context = fileContext->Provider->OpenFile(Entry);
    fileContext->Entry = Entry;
    fileContext->Filesystem = Context;
    fileContext->DirBuffer = NULL;
    FileTree_GetFullName(Entry, fileContext->FullName);
    *PFileContext = fileContext;

    FileInfo->FileAttributes = Entry->fileAttributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize = Entry->fileProvider->fileSize;
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = Context->fileCreationTime;
    FileInfo->LastAccessTime = Context->fileCreationTime;
    FileInfo->LastWriteTime = Context->fileCreationTime;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    return STATUS_SUCCESS;
}

NTSTATUS ProjFS::Overwrite(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

VOID ProjFS::Cleanup(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, PWSTR FileName, ULONG Flags)
{
    if (Flags & FspCleanupDelete)
    {
        FileContextData* fileContext = (FileContextData*)FileContext;

        fileContext->Provider->CloseFile(fileContext->Context);
        fileContext->Context = (void*)43;
    }
}

VOID ProjFS::Close(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext0)
{
    FileContextData* fileContext = (FileContextData*)FileContext0;

    fileContext->Provider->CloseFile(fileContext->Context);
    fileContext->Context = (void*)42;
    FspFileSystemDeleteDirectoryBuffer(&fileContext->DirBuffer);

    delete fileContext;
}

NTSTATUS ProjFS::Read(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    FileContextData* fileContext = (FileContextData*)FileContext;
    fileContext->Provider->ReadFile(fileContext->Context, (void*)Buffer, (uint64_t)Offset, (uint32_t)Length, (uint64_t*)PBytesTransferred);

    return STATUS_SUCCESS;
}

NTSTATUS ProjFS::Write(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::Flush(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::GetFileInfo(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    FileContextData* fileContext = (FileContextData*)FileContext;

    FileInfo->FileAttributes = fileContext->Entry->fileAttributes;
    FileInfo->ReparseTag = 0;
    FileInfo->FileSize = fileContext->Provider->fileSize;
    FileInfo->AllocationSize = (FileInfo->FileSize + ALLOCATION_UNIT - 1)
        / ALLOCATION_UNIT * ALLOCATION_UNIT;
    FileInfo->CreationTime = fileContext->Filesystem->fileCreationTime;
    FileInfo->LastAccessTime = fileContext->Filesystem->fileCreationTime;
    FileInfo->LastWriteTime = fileContext->Filesystem->fileCreationTime;
    FileInfo->ChangeTime = FileInfo->LastWriteTime;
    FileInfo->IndexNumber = 0;
    FileInfo->HardLinks = 0;

    return STATUS_SUCCESS;
}

NTSTATUS ProjFS::SetBasicInfo(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::SetFileSize(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::Rename(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::GetSecurity(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
    SecurityDescriptor = NULL; // hope this is valid?
    *PSecurityDescriptorSize = 0;

    return STATUS_SUCCESS;
}

NTSTATUS ProjFS::SetSecurity(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS ProjFS::ReadDirectory(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext0, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG BufferLength, PULONG PBytesTransferred)
{
    ProjFS* Context = (ProjFS*)FileSystem->UserContext;
    FileContextData* fileContext = (FileContextData*)FileContext0;
    ULONG PatternLength;
    HANDLE FindHandle;
    union
    {
        UINT8 B[FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + MAX_PATH * sizeof(WCHAR)];
        FSP_FSCTL_DIR_INFO D;
    } DirInfoBuf;
    FSP_FSCTL_DIR_INFO* DirInfo = &DirInfoBuf.D;
    NTSTATUS DirBufferResult;

    DirBufferResult = STATUS_SUCCESS;
    if (FspFileSystemAcquireDirectoryBuffer(&fileContext->DirBuffer, 0 == Marker, &DirBufferResult))
    {
        if (0 == Pattern)
            Pattern = L"*";
        PatternLength = (ULONG)wcslen(Pattern); // pattern is unused atm, idc how you sort it rn

        if (!NT_SUCCESS(DirBufferResult))
        {
            FspFileSystemReleaseDirectoryBuffer(&fileContext->DirBuffer);
            return DirBufferResult;
        }

        FileTree* tree = (FileTree*)fileContext->Context;
        wprintf(L"Listing directory for %s\n", tree->value->name.c_str());

        wchar_t fileName[MAX_PATH];
        for (auto& folder : tree->folders) {
            wcscpy(fileName, const_cast<wchar_t*>(folder->value->name.c_str()));
            if (!PathMatchSpecW(fileName, Pattern)) {
                wprintf(L"doesn't match %s %s", fileName, Pattern);
                continue;
            }

            memset(DirInfo, 0, sizeof* DirInfo);
            auto Length = (ULONG)wcslen(fileName);
            DirInfo->Size = (UINT16)(FIELD_OFFSET(FSP_FSCTL_DIR_INFO, FileNameBuf) + Length * sizeof(WCHAR));
            DirInfo->FileInfo.FileAttributes = folder->value->fileAttributes;
            DirInfo->FileInfo.ReparseTag = 0;
            DirInfo->FileInfo.FileSize = folder->value->fileProvider->fileSize;
            DirInfo->FileInfo.AllocationSize = (DirInfo->FileInfo.FileSize + ALLOCATION_UNIT - 1)
                / ALLOCATION_UNIT * ALLOCATION_UNIT;
            DirInfo->FileInfo.CreationTime = Context->fileCreationTime;
            DirInfo->FileInfo.LastAccessTime = Context->fileCreationTime;
            DirInfo->FileInfo.LastWriteTime = Context->fileCreationTime;
            DirInfo->FileInfo.ChangeTime = DirInfo->FileInfo.LastWriteTime;
            DirInfo->FileInfo.IndexNumber = 0;
            DirInfo->FileInfo.HardLinks = 0;
            memcpy(DirInfo->FileNameBuf, fileName, Length * sizeof(WCHAR));
            wprintf(L"query filename: %s\nattr: %d\n", fileName, folder->value->fileAttributes);

            if (!FspFileSystemFillDirectoryBuffer(&fileContext->DirBuffer, DirInfo, &DirBufferResult))
                break;
        }

        FspFileSystemReleaseDirectoryBuffer(&fileContext->DirBuffer);
    }

    if (!NT_SUCCESS(DirBufferResult))
        return DirBufferResult;

    FspFileSystemReadDirectoryBuffer(&fileContext->DirBuffer,
        Marker, Buffer, BufferLength, PBytesTransferred);

    return STATUS_SUCCESS;
}

NTSTATUS ProjFS::SetDelete(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileContext, PWSTR FileName, BOOLEAN DeleteFile)
{
    // unsupported
    return STATUS_INVALID_DEVICE_REQUEST;
}