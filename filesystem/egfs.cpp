#include "egfs.h"

EGFS::EGFS(EGFS_PARAMS* Params, NTSTATUS& ErrorCode) :
    FileSystem(nullptr),
    IsStarted(false)
{
	if (!Params) {
        ErrorCode = 420;
		return;
	}

    NTSTATUS Result;
    BOOLEAN Inserted;

    Callbacks = Params->Callbacks;
    Security = new char[Params->SecuritySize];
    SecuritySize = Params->SecuritySize;
    memcpy(Security, Params->Security, Params->SecuritySize);

    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.Version = sizeof FSP_FSCTL_VOLUME_PARAMS;
    VolumeParams.SectorSize = Params->SectorSize; // MEMFS_SECTOR_SIZE
    VolumeParams.SectorsPerAllocationUnit = Params->SectorsPerAllocationUnit; // MEMFS_SECTORS_PER_ALLOCATION_UNIT
    VolumeParams.VolumeCreationTime = Params->VolumeCreationTime; //MemfsGetSystemTime();
    VolumeParams.VolumeSerialNumber = Params->VolumeSerialNumber; //(UINT32)(MemfsGetSystemTime() / (10000 * 1000));
    VolumeParams.FileInfoTimeout = Params->FileInfoTimeout; // INFINITE;
    VolumeParams.CaseSensitiveSearch = Params->CaseSensitiveSearch; // true;
    VolumeParams.CasePreservedNames = Params->CasePreservedNames; // 1;
    VolumeParams.UnicodeOnDisk = Params->UnicodeOnDisk; // 1;
    VolumeParams.PersistentAcls = Params->PersistentAcls; // 1;
    VolumeParams.ReparsePoints = Params->ReparsePoints; // 1;
    VolumeParams.ReparsePointsAccessCheck = Params->ReparsePointsAccessCheck; // 0;
    VolumeParams.PostCleanupWhenModifiedOnly = Params->PostCleanupWhenModifiedOnly; // 1;
    VolumeParams.FlushAndPurgeOnCleanup = Params->FlushAndPurgeOnCleanup; // false;
    VolumeParams.AllowOpenInKernelMode = Params->AllowOpenInKernelMode; // 1;
    if (Params->VolumePrefix[0])
        wcscpy_s(VolumeParams.Prefix, Params->VolumePrefix);
    wcscpy_s(VolumeParams.FileSystemName, Params->FileSystemName);

    wcscpy_s(VolumeLabel, Params->VolumeLabel);
    VolumeTotal = Params->VolumeTotal;
    VolumeFree = Params->VolumeFree;

    Result = FspFileSystemCreate(L"" FSP_FSCTL_DISK_DEVICE_NAME, &VolumeParams, &FspInterface, &FileSystem);
    if (!NT_SUCCESS(Result))
    {
        ErrorCode = Result;
        return;
    }

    FspFileSystemSetDebugLog(FileSystem, Params->LogFlags);

    FileSystem->UserContext = this;

    ErrorCode = STATUS_SUCCESS;
}

EGFS::~EGFS() {
    if (FileSystem && Started()) {
        FspFileSystemDelete(FileSystem);
        delete[] Security;
    }
}

bool EGFS::SetMountPoint(PCWSTR MountPoint, PVOID Security) {
    if (Started()) {
        return false;
    }
    return NT_SUCCESS(FspFileSystemSetMountPointEx(FileSystem, (PWSTR)MountPoint, Security));
}

void EGFS::AddFile(fs::path& Path, PVOID Context, UINT64 FileSize)
{
    Files.AddFile(Path.generic_wstring().c_str(), EGFS_FILE(FileSize, Context));
}

bool EGFS::Start() {
    if (Started()) {
        return true;
    }
    if (NT_SUCCESS(FspFileSystemStartDispatcher(FileSystem, 0))) {
        IsStarted = true;
        return true;
    }
    return false;
}

bool EGFS::Stop() {
    if (!Started())
    {
        return true;
    }
    if (FileSystem) {
        FspFileSystemStopDispatcher(FileSystem);
    }
    IsStarted = false;
    return true;
}

bool EGFS::Started() {
    return IsStarted;
}

UINT32 EGFS::GetFileAttributes(node_type* file)
{
    return FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE |
        (std::holds_alternative<container_type>(*file) ? FILE_ATTRIBUTE_DIRECTORY : 0);
}

void EGFS::GetFileInfo(container_type* file, FSP_FSCTL_FILE_INFO* info)
{
    info->CreationTime = 0;
    info->LastAccessTime = 0;
    info->LastWriteTime = 0;
    info->ChangeTime = 0;
    info->IndexNumber = 0;

    info->FileSize = 0;
    info->AllocationSize = 0;
    info->FileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_DIRECTORY;
}

void EGFS::GetFileInfo(node_type* file, FSP_FSCTL_FILE_INFO* info)
{
    if (auto filePtr = std::get_if<EGFS_FILE>(file)) {
        // these can be simplified away (unimportant, just for looks)
        info->CreationTime = filePtr->CreationTime;
        info->LastAccessTime = filePtr->AccessTime;
        info->LastWriteTime = filePtr->WriteTime;
        info->ChangeTime = filePtr->ChangeTime;
        info->IndexNumber = 0;

        info->FileSize = filePtr->FileSize;
        info->AllocationSize = 0;
        info->FileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE;
    }
    else {
        GetFileInfo(std::get_if<container_type>(file), info);
    }
}

UINT64 EGFS::GetFileSize(node_type* file)
{
    if (auto filePtr = std::get_if<EGFS_FILE>(file)) {
        return filePtr->FileSize;
    }
    return 0; // folder/dirtree
}

FSP_FILE_SYSTEM_INTERFACE EGFS::FspInterface =
{
    GetVolumeInfo,
    SetVolumeLabel,
    GetSecurityByName,
    Create,
    Open,
    Overwrite,
    Cleanup,
    Close,
    Read,
    Write,
    Flush,
    GetFileInfo,
    SetBasicInfo,
    SetFileSize,
    CanDelete,
    Rename,
    GetSecurity,
    SetSecurity,
    ReadDirectory,
    0, // ResolveReparsePoints
    0, // GetReparsePoint
    0, // SetReparsePoint
    0, // DeleteReparsePoint
    0, // GetStreamInfo
    0, // GetDirInfoByName
    0, // Control
    0, // SetDelete
    0, // CreateEx
    0, // OverwriteEx
    0, // GetEa
    0, // SetEa
};

NTSTATUS EGFS::GetVolumeInfo(FSP_FILE_SYSTEM* FileSystem, FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
    auto Egfs = (EGFS*)FileSystem->UserContext;

    VolumeInfo->TotalSize = Egfs->VolumeTotal;
    VolumeInfo->FreeSize = Egfs->VolumeFree;
    VolumeInfo->VolumeLabelLength = wcslen(Egfs->VolumeLabel) * sizeof(WCHAR);
    wcscpy_s(VolumeInfo->VolumeLabel, Egfs->VolumeLabel);

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::SetVolumeLabel(FSP_FILE_SYSTEM* FileSystem, PWSTR VolumeLabel, FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
    return STATUS_ACCESS_DENIED;
}

NTSTATUS EGFS::GetSecurityByName(FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
    auto Egfs = (EGFS*)FileSystem->UserContext;

    auto file = Egfs->Files.GetFile(FileName);
    if (!file)
    {
        //Result = STATUS_OBJECT_NAME_NOT_FOUND;
        //MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (0 != PFileAttributes)
        *PFileAttributes = GetFileAttributes(file);

    if (0 != PSecurityDescriptorSize)
    {
        if (Egfs->SecuritySize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = Egfs->SecuritySize;
            return STATUS_BUFFER_OVERFLOW;
        }

        *PSecurityDescriptorSize = Egfs->SecuritySize;

        if (0 != SecurityDescriptor)
            memcpy(SecurityDescriptor, Egfs->Security, Egfs->SecuritySize);
    }

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::Create(FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize, PVOID* PFileNode, FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_ACCESS_DENIED;
}

NTSTATUS EGFS::Open(FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID* PFileNode, FSP_FSCTL_FILE_INFO* FileInfo)
{
    auto Egfs = (EGFS*)FileSystem->UserContext;

    if (EGFS_MAX_PATH <= wcslen(FileName))
        return STATUS_OBJECT_NAME_INVALID;

    auto file = Egfs->Files.GetFile(FileName);
    if (!file)
    {
        //Result = STATUS_OBJECT_NAME_NOT_FOUND;
        //MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    if (CreateOptions & FILE_DELETE_ON_CLOSE) // disable deletions
    {
        return STATUS_ACCESS_DENIED;
    }

    *PFileNode = file;
    GetFileInfo(file, FileInfo);

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::Overwrite(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_ACCESS_DENIED;
}

VOID EGFS::Cleanup(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR FileName, ULONG Flags)
{
    // I've never ever seen this called (when only reading)
    return; // never should be called if done correctly (PostCleanupWhenModifiedOnly I believe)
}

VOID EGFS::Close(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0)
{
    return;
}

NTSTATUS EGFS::Read(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred)
{
    auto Egfs = (EGFS*)FileSystem->UserContext;
    node_type* FileNode = (node_type*)FileNode0;
    UINT64 EndOffset;

    if (Offset >= GetFileSize(FileNode))
        return STATUS_END_OF_FILE;

    EGFS_FILE* FilePtr = std::get_if<EGFS_FILE>(FileNode);

    EndOffset = Offset + Length;
    if (EndOffset > GetFileSize(FileNode))
        EndOffset = GetFileSize(FileNode);

    Egfs->Callbacks.Read(FilePtr->Context, Buffer, Offset, (size_t)(EndOffset - Offset), PBytesTransferred);

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::Write(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length, BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo, PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo)
{
    *PBytesTransferred = 0;
    return STATUS_ACCESS_DENIED;
}

NTSTATUS EGFS::Flush(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, FSP_FSCTL_FILE_INFO* FileInfo)
{
    node_type* FileNode = (node_type*)FileNode0;

    if (0 != FileNode)
    {
        GetFileInfo(FileNode, FileInfo);
    }

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::GetFileInfo(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, FSP_FSCTL_FILE_INFO* FileInfo)
{
    node_type* FileNode = (node_type*)FileNode0;

    GetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::SetBasicInfo(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime, FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_ACCESS_DENIED; // maybe return success just in case?
}

NTSTATUS EGFS::SetFileSize(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize, FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_INSUFFICIENT_RESOURCES; // might set to access denied, i'm unsure
}

NTSTATUS EGFS::CanDelete(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR FileName)
{
    return STATUS_ACCESS_DENIED;
}

NTSTATUS EGFS::Rename(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    return STATUS_ACCESS_DENIED;
}

NTSTATUS EGFS::GetSecurity(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
    auto Egfs = (EGFS*)FileSystem->UserContext;
    node_type* FileNode = (node_type*)FileNode0;

    if (Egfs->SecuritySize > * PSecurityDescriptorSize)
    {
        *PSecurityDescriptorSize = Egfs->SecuritySize;
        return STATUS_BUFFER_OVERFLOW;
    }

    *PSecurityDescriptorSize = Egfs->SecuritySize;
    if (0 != SecurityDescriptor)
        memcpy(SecurityDescriptor, Egfs->Security, Egfs->SecuritySize);

    return STATUS_SUCCESS;
}

NTSTATUS EGFS::SetSecurity(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    return STATUS_ACCESS_DENIED;
}

// Marker is relative to FileNode0
NTSTATUS EGFS::ReadDirectory(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    //assert(0 == Pattern);

    auto Egfs = (EGFS*)FileSystem->UserContext;
    node_type* FileNode = (node_type*)FileNode0;
    container_type* FolderNode = std::get_if<container_type>(FileNode);
    if (!FolderNode) {
        return STATUS_NOT_A_DIRECTORY;
    }

    auto iter = FolderNode->begin();
    if (Marker) {
        auto marker = FolderNode->Children.find(Marker);
        if (marker != FolderNode->Children.end()) {
            iter = marker;
        }
    }

    /*
    Isn't used, can be utilized in future use, maybe
    std::wstring folderName;
    {
        folderName.reserve(EGFS_MAX_PATH);
        for (auto parent = FolderNodePtr; parent; parent = parent->Parent) {
            folderName.insert(0, 1, L'\\');
            folderName.insert(0, *parent->Name);
        }
    }
    */

    // reuse for recalculations, etc.
    UINT8 DirInfoBuf[sizeof(FSP_FSCTL_DIR_INFO) + EGFS_MAX_PATH * sizeof(WCHAR)];
    FSP_FSCTL_DIR_INFO* DirInfo = (FSP_FSCTL_DIR_INFO*)DirInfoBuf;
    memset(DirInfo->Padding, 0, sizeof(DirInfo->Padding));

#define ADD_DIR_INFO(Node, Name, NameLen)                                       \
{                                                                               \
    GetFileInfo(Node, &DirInfo->FileInfo);                                      \
    memcpy(DirInfo->FileNameBuf, Name, NameLen * sizeof(WCHAR));                \
    DirInfo->Size = sizeof(FSP_FSCTL_DIR_INFO) + NameLen * sizeof(WCHAR);       \
                                                                                \
    if (!FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred)) { \
        return STATUS_SUCCESS;                                                  \
    }                                                                           \
}                                                                               \

    // add . and .. if not root node
    if (FolderNode->Parent) {
        ADD_DIR_INFO(FolderNode, L".", 1);
        ADD_DIR_INFO(const_cast<container_type*>(FolderNode->Parent), L"..", 2);
    }

    for (; iter != FolderNode->end(); ++iter) {
        ADD_DIR_INFO(&iter->second, iter->first.c_str(), iter->first.size());
    }
    // EOF value
    FspFileSystemAddDirInfo(NULL, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
}