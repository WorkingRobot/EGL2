
#include "memfs.h"
#include <sddl.h>
#include <VersionHelpers.h>
#include <cassert>
#include <map>
#include <unordered_map>

/*
 * Custom Read/Write Support
 */

typedef struct _MEMFS_FILE_PROVIDER
{
    std::function<PVOID(PCWSTR fileName, UINT64* fileSize)> Open;
    std::function<void(PVOID Handle)> Close;
    std::function<void(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead)> Read;
} MEMFS_FILE_PROVIDER;

MEMFS_FILE_PROVIDER* CreateProvider(
    std::function<PVOID(PCWSTR fileName, UINT64* fileSize)> Open,
    std::function<void(PVOID Handle)> Close,
    std::function<void(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead)> Read)
{
    auto Provider = new MEMFS_FILE_PROVIDER;
    Provider->Open = Open;
    Provider->Close = Close;
    Provider->Read = Read;
    return Provider;
}

void CloseProvider(MEMFS_FILE_PROVIDER* Provider)
{
    delete Provider;
}

static inline
UINT64 MemfsGetSystemTime(VOID)
{
    FILETIME FileTime;
    GetSystemTimeAsFileTime(&FileTime);
    return ((PLARGE_INTEGER)&FileTime)->QuadPart;
}

static inline
int MemfsFileNameCompare(PWSTR a, int alen, PWSTR b, int blen, BOOLEAN CaseInsensitive)
{
    PWSTR p, endp, partp, q, endq, partq;
    WCHAR c, d;
    int plen, qlen, len, res;

    if (-1 == alen)
        alen = lstrlenW(a);
    if (-1 == blen)
        blen = lstrlenW(b);

    for (p = a, endp = p + alen, q = b, endq = q + blen; endp > p&& endq > q;)
    {
        c = d = 0;
        for (; endp > p && (L':' == *p || L'\\' == *p); p++)
            c = *p;
        for (; endq > q && (L':' == *q || L'\\' == *q); q++)
            d = *q;

        if (L':' == c)
            c = 1;
        else if (L'\\' == c)
            c = 2;
        if (L':' == d)
            d = 1;
        else if (L'\\' == d)
            d = 2;

        res = c - d;
        if (0 != res)
            return res;

        for (partp = p; endp > p && L':' != *p && L'\\' != *p; p++)
            ;
        for (partq = q; endq > q && L':' != *q && L'\\' != *q; q++)
            ;

        plen = (int)(p - partp);
        qlen = (int)(q - partq);

        len = plen < qlen ? plen : qlen;

        if (CaseInsensitive)
        {
            res = CompareStringW(LOCALE_INVARIANT, NORM_IGNORECASE, partp, plen, partq, qlen);
            if (0 != res)
                res -= 2;
            else
                res = _wcsnicmp(partp, partq, len);
        }
        else
            res = wcsncmp(partp, partq, len);

        if (0 == res)
            res = plen - qlen;

        if (0 != res)
            return res;
    }

    return -(endp <= p) + (endq <= q);
}

static inline
BOOLEAN MemfsFileNameHasPrefix(PWSTR a, PWSTR b, BOOLEAN CaseInsensitive)
{
    int alen = (int)wcslen(a);
    int blen = (int)wcslen(b);

    return alen >= blen && 0 == MemfsFileNameCompare(a, blen, b, blen, CaseInsensitive) &&
        (alen == blen || (1 == blen && L'\\' == b[0]) ||
            (L'\\' == a[blen]));
}

typedef struct _MEMFS_FILE_NODE
{
    WCHAR FileName[MEMFS_MAX_PATH];
    FSP_FSCTL_FILE_INFO FileInfo;
    PVOID FileData;
    volatile LONG RefCount;
} MEMFS_FILE_NODE;

struct MEMFS_FILE_NODE_LESS
{
    MEMFS_FILE_NODE_LESS(BOOLEAN CaseInsensitive) : CaseInsensitive(CaseInsensitive)
    {
    }
    bool operator()(PWSTR a, PWSTR b) const
    {
        return 0 > MemfsFileNameCompare(a, -1, b, -1, CaseInsensitive);
    }
    BOOLEAN CaseInsensitive;
};
typedef std::map<PWSTR, MEMFS_FILE_NODE*, MEMFS_FILE_NODE_LESS> MEMFS_FILE_NODE_MAP;

typedef struct _MEMFS
{
    FSP_FILE_SYSTEM* FileSystem;
    MEMFS_FILE_NODE_MAP* FileNodeMap;
    MEMFS_FILE_PROVIDER* FileProvider;
    ULONG MaxFileNodes;
    UINT64 VolumeTotal;
    UINT64 VolumeFree;
    WCHAR VolumeLabel[32];
    PVOID Security;
    SIZE_T SecuritySize;
} MEMFS;

static inline
NTSTATUS MemfsFileNodeCreate(MEMFS_FILE_PROVIDER* Provider, PWSTR FileName, MEMFS_FILE_NODE** PFileNode)
{
    static UINT64 IndexNumber = 1;
    MEMFS_FILE_NODE* FileNode;

    *PFileNode = 0;

    FileNode = (MEMFS_FILE_NODE*)malloc(sizeof * FileNode);
    if (0 == FileNode)
        return STATUS_INSUFFICIENT_RESOURCES;

    memset(FileNode, 0, sizeof * FileNode);
    wcscpy_s(FileNode->FileName, sizeof FileNode->FileName / sizeof(WCHAR), FileName);
    FileNode->FileInfo.CreationTime =
        FileNode->FileInfo.LastAccessTime =
        FileNode->FileInfo.LastWriteTime =
        FileNode->FileInfo.ChangeTime = MemfsGetSystemTime();
    FileNode->FileData = Provider->Open(FileName, &FileNode->FileInfo.FileSize);
    FileNode->FileInfo.IndexNumber = IndexNumber++;

    *PFileNode = FileNode;

    return STATUS_SUCCESS;
}

static inline
VOID MemfsFileNodeDelete(MEMFS_FILE_PROVIDER* Provider, MEMFS_FILE_NODE* FileNode)
{
    Provider->Close(FileNode->FileData);
    free(FileNode);
}

static inline
VOID MemfsFileNodeReference(MEMFS_FILE_NODE* FileNode)
{
    InterlockedIncrement(&FileNode->RefCount);
}

static inline
VOID MemfsFileNodeDereference(MEMFS_FILE_PROVIDER* Provider, MEMFS_FILE_NODE* FileNode)
{
    if (0 == InterlockedDecrement(&FileNode->RefCount))
        MemfsFileNodeDelete(Provider, FileNode);
}

static inline
VOID MemfsFileNodeGetFileInfo(MEMFS_FILE_NODE* FileNode, FSP_FSCTL_FILE_INFO* FileInfo)
{
    * FileInfo = FileNode->FileInfo;
}

static inline
VOID MemfsFileNodeMapDump(MEMFS_FILE_PROVIDER* Provider, MEMFS_FILE_NODE_MAP* FileNodeMap)
{
    for (MEMFS_FILE_NODE_MAP::iterator p = FileNodeMap->begin(), q = FileNodeMap->end(); p != q; ++p)
        FspDebugLog("%c %04lx %6lu %S\n",
            FILE_ATTRIBUTE_DIRECTORY & p->second->FileInfo.FileAttributes ? 'd' : 'f',
            (ULONG)p->second->FileInfo.FileAttributes,
            (ULONG)p->second->FileInfo.FileSize,
            p->second->FileName);
}

static inline
BOOLEAN MemfsFileNodeMapIsCaseInsensitive(MEMFS_FILE_NODE_MAP* FileNodeMap)
{
    return FileNodeMap->key_comp().CaseInsensitive;
}

static inline
NTSTATUS MemfsFileNodeMapCreate(BOOLEAN CaseInsensitive, MEMFS_FILE_NODE_MAP** PFileNodeMap)
{
    *PFileNodeMap = 0;
    try
    {
        *PFileNodeMap = new MEMFS_FILE_NODE_MAP(MEMFS_FILE_NODE_LESS(CaseInsensitive));
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

static inline
VOID MemfsFileNodeMapDelete(MEMFS_FILE_PROVIDER* Provider, MEMFS_FILE_NODE_MAP* FileNodeMap)
{
    for (MEMFS_FILE_NODE_MAP::iterator p = FileNodeMap->begin(), q = FileNodeMap->end(); p != q; ++p)
        MemfsFileNodeDelete(Provider, p->second);

    delete FileNodeMap;
}

static inline
SIZE_T MemfsFileNodeMapCount(MEMFS_FILE_NODE_MAP* FileNodeMap)
{
    return FileNodeMap->size();
}

static inline
MEMFS_FILE_NODE* MemfsFileNodeMapGet(MEMFS_FILE_NODE_MAP* FileNodeMap, PWSTR FileName)
{
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->find(FileName);
    if (iter == FileNodeMap->end())
        return 0;
    return iter->second;
}

static inline
MEMFS_FILE_NODE* MemfsFileNodeMapGetParent(MEMFS_FILE_NODE_MAP* FileNodeMap, PWSTR FileName0,
    PNTSTATUS PResult)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    WCHAR FileName[MEMFS_MAX_PATH];
    wcscpy_s(FileName, sizeof FileName / sizeof(WCHAR), FileName0);
    FspPathSuffix(FileName, &Remain, &Suffix, Root);
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->find(Remain);
    FspPathCombine(FileName, Suffix);
    if (iter == FileNodeMap->end())
    {
        *PResult = STATUS_OBJECT_PATH_NOT_FOUND;
        return 0;
    }
    if (0 == (iter->second->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        *PResult = STATUS_NOT_A_DIRECTORY;
        return 0;
    }
    return iter->second;
}

static inline
VOID MemfsFileNodeMapTouchParent(MEMFS_FILE_NODE_MAP* FileNodeMap, MEMFS_FILE_NODE* FileNode)
{
    NTSTATUS Result;
    MEMFS_FILE_NODE* Parent;
    if (L'\\' == FileNode->FileName[0] && L'\0' == FileNode->FileName[1])
        return;
    Parent = MemfsFileNodeMapGetParent(FileNodeMap, FileNode->FileName, &Result);
    if (0 == Parent)
        return;
    Parent->FileInfo.LastAccessTime =
        Parent->FileInfo.LastWriteTime =
        Parent->FileInfo.ChangeTime = MemfsGetSystemTime();
}

static inline
NTSTATUS MemfsFileNodeMapInsert(MEMFS_FILE_NODE_MAP* FileNodeMap, MEMFS_FILE_NODE* FileNode,
    PBOOLEAN PInserted)
{
    *PInserted = 0;
    try
    {
        *PInserted = FileNodeMap->insert(MEMFS_FILE_NODE_MAP::value_type(FileNode->FileName, FileNode)).second;
        if (*PInserted)
        {
            MemfsFileNodeReference(FileNode);
            MemfsFileNodeMapTouchParent(FileNodeMap, FileNode);
        }
        return STATUS_SUCCESS;
    }
    catch (...)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
}

static inline
VOID MemfsFileNodeMapRemove(MEMFS_FILE_PROVIDER* Provider, MEMFS_FILE_NODE_MAP* FileNodeMap, MEMFS_FILE_NODE* FileNode)
{
    if (FileNodeMap->erase(FileNode->FileName))
    {
        MemfsFileNodeMapTouchParent(FileNodeMap, FileNode);
        MemfsFileNodeDereference(Provider, FileNode);
    }
}

static inline
BOOLEAN MemfsFileNodeMapHasChild(MEMFS_FILE_NODE_MAP* FileNodeMap, MEMFS_FILE_NODE* FileNode)
{
    BOOLEAN Result = FALSE;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->upper_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
        FspPathSuffix(iter->second->FileName, &Remain, &Suffix, Root);
        Result = 0 == MemfsFileNameCompare(Remain, -1, FileNode->FileName, -1,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap));
        FspPathCombine(iter->second->FileName, Suffix);
        break;
    }
    return Result;
}

static inline
BOOLEAN MemfsFileNodeMapEnumerateChildren(MEMFS_FILE_NODE_MAP* FileNodeMap, MEMFS_FILE_NODE* FileNode,
    PWSTR PrevFileName0, BOOLEAN(*EnumFn)(MEMFS_FILE_NODE*, PVOID), PVOID Context)
{
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;
    MEMFS_FILE_NODE_MAP::iterator iter;
    BOOLEAN IsDirectoryChild;
    if (0 != PrevFileName0)
    {
        WCHAR PrevFileName[MEMFS_MAX_PATH + 256];
        size_t Length0 = wcslen(FileNode->FileName);
        size_t Length1 = 1 != Length0 || L'\\' != FileNode->FileName[0];
        size_t Length2 = wcslen(PrevFileName0);
        assert(MEMFS_MAX_PATH + 256 > Length0 + Length1 + Length2);
        memcpy(PrevFileName, FileNode->FileName, Length0 * sizeof(WCHAR));
        memcpy(PrevFileName + Length0, L"\\", Length1 * sizeof(WCHAR));
        memcpy(PrevFileName + Length0 + Length1, PrevFileName0, Length2 * sizeof(WCHAR));
        PrevFileName[Length0 + Length1 + Length2] = L'\0';
        iter = FileNodeMap->upper_bound(PrevFileName);
    }
    else
        iter = FileNodeMap->upper_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
        if (!MemfsFileNameHasPrefix(iter->second->FileName, FileNode->FileName,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap)))
            break;
        FspPathSuffix(iter->second->FileName, &Remain, &Suffix, Root);
        IsDirectoryChild = 0 == MemfsFileNameCompare(Remain, -1, FileNode->FileName, -1,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap));
        FspPathCombine(iter->second->FileName, Suffix);
        if (IsDirectoryChild)
        {
            if (!EnumFn(iter->second, Context))
                return FALSE;
        }
    }
    return TRUE;
}

static inline
BOOLEAN MemfsFileNodeMapEnumerateDescendants(MEMFS_FILE_NODE_MAP* FileNodeMap, MEMFS_FILE_NODE* FileNode,
    BOOLEAN(*EnumFn)(MEMFS_FILE_NODE*, PVOID), PVOID Context)
{
    WCHAR Root[2] = L"\\";
    MEMFS_FILE_NODE_MAP::iterator iter = FileNodeMap->lower_bound(FileNode->FileName);
    for (; FileNodeMap->end() != iter; ++iter)
    {
        if (!MemfsFileNameHasPrefix(iter->second->FileName, FileNode->FileName,
            MemfsFileNodeMapIsCaseInsensitive(FileNodeMap)))
            break;
        if (!EnumFn(iter->second, Context))
            return FALSE;
    }
    return TRUE;
}

typedef struct _MEMFS_FILE_NODE_MAP_ENUM_CONTEXT
{
    BOOLEAN Reference;
    MEMFS_FILE_NODE** FileNodes;
    ULONG Capacity, Count;
} MEMFS_FILE_NODE_MAP_ENUM_CONTEXT;

static inline
BOOLEAN MemfsFileNodeMapEnumerateFn(MEMFS_FILE_NODE* FileNode, PVOID Context0)
{
    MEMFS_FILE_NODE_MAP_ENUM_CONTEXT* Context = (MEMFS_FILE_NODE_MAP_ENUM_CONTEXT*)Context0;

    if (Context->Capacity <= Context->Count)
    {
        ULONG Capacity = 0 != Context->Capacity ? Context->Capacity * 2 : 16;
        PVOID P = realloc(Context->FileNodes, Capacity * sizeof Context->FileNodes[0]);
        if (0 == P)
        {
            FspDebugLog(__FUNCTION__ ": cannot allocate memory; aborting\n");
            abort();
        }

        Context->FileNodes = (MEMFS_FILE_NODE**)P;
        Context->Capacity = Capacity;
    }

    Context->FileNodes[Context->Count++] = FileNode;
    if (Context->Reference)
        MemfsFileNodeReference(FileNode);

    return TRUE;
}

static inline
VOID MemfsFileNodeMapEnumerateFree(MEMFS_FILE_PROVIDER* Provider, MEMFS_FILE_NODE_MAP_ENUM_CONTEXT* Context)
{
    if (Context->Reference)
    {
        for (ULONG Index = 0; Context->Count > Index; Index++)
        {
            MEMFS_FILE_NODE* FileNode = Context->FileNodes[Index];
            MemfsFileNodeDereference(Provider, FileNode);
        }
    }
    free(Context->FileNodes);
}

NTSTATUS CreateFsFile(MEMFS* Memfs,
    PWSTR FileName, BOOLEAN Directory) {
    MEMFS_FILE_NODE* FileNode;
    MEMFS_FILE_NODE* ParentNode;
    NTSTATUS Result;
    BOOLEAN Inserted;

    if (MEMFS_MAX_PATH <= wcslen(FileName))
        return STATUS_OBJECT_NAME_INVALID;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 != FileNode)
        return STATUS_OBJECT_NAME_COLLISION;

    ParentNode = MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
    if (0 == ParentNode)
        return Result;

    if (MemfsFileNodeMapCount(Memfs->FileNodeMap) >= Memfs->MaxFileNodes)
        return STATUS_CANNOT_MAKE;

    Result = MemfsFileNodeCreate(Memfs->FileProvider, FileName, &FileNode);
    if (!NT_SUCCESS(Result))
        return Result;

    FileNode->FileInfo.FileAttributes =
        (Directory ? FILE_ATTRIBUTE_DIRECTORY : 0)
        | FILE_ATTRIBUTE_READONLY;

    FileNode->FileInfo.AllocationSize = 0;

    Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, FileNode, &Inserted);
    if (!NT_SUCCESS(Result) || !Inserted)
    {
        MemfsFileNodeDelete(Memfs->FileProvider, FileNode);
        if (NT_SUCCESS(Result))
            Result = STATUS_OBJECT_NAME_COLLISION; /* should not happen! */
        return Result;
    }

    MemfsFileNodeReference(FileNode);

    return STATUS_SUCCESS;
}

/*
 * FSP_FILE_SYSTEM_INTERFACE
 */

static NTSTATUS SetFileSizeInternal(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize);

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM* FileSystem,
    FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;

    VolumeInfo->TotalSize = Memfs->VolumeTotal;
    VolumeInfo->FreeSize = Memfs->VolumeFree;
    VolumeInfo->VolumeLabelLength = wcslen(Memfs->VolumeLabel) * sizeof(WCHAR);
    wcscpy_s(VolumeInfo->VolumeLabel, Memfs->VolumeLabel);

    return STATUS_SUCCESS;
}

static NTSTATUS SetVolumeLabel(FSP_FILE_SYSTEM* FileSystem,
    PWSTR VolumeLabel,
    FSP_FSCTL_VOLUME_INFO* VolumeInfo)
{
    return STATUS_ACCESS_DENIED;
}

static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM* FileSystem,
    PWSTR FileName, PUINT32 PFileAttributes,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;
    MEMFS_FILE_NODE* FileNode;
    NTSTATUS Result;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return Result;
    }

    if (0 != PFileAttributes)
        *PFileAttributes = FileNode->FileInfo.FileAttributes;

    if (0 != PSecurityDescriptorSize)
    {
        if (Memfs->SecuritySize > *PSecurityDescriptorSize)
        {
            *PSecurityDescriptorSize = Memfs->SecuritySize;
            return STATUS_BUFFER_OVERFLOW;
        }

        *PSecurityDescriptorSize = Memfs->SecuritySize;

        if (0 != SecurityDescriptor)
            memcpy(SecurityDescriptor, Memfs->Security, Memfs->SecuritySize);
    }

    return STATUS_SUCCESS;
}

NTSTATUS Create(FSP_FILE_SYSTEM* FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize,
    PVOID* PFileNode, FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_ACCESS_DENIED;
}

NTSTATUS Open(FSP_FILE_SYSTEM* FileSystem,
    PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess,
    PVOID* PFileNode, FSP_FSCTL_FILE_INFO* FileInfo)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;
    MEMFS_FILE_NODE* FileNode;
    NTSTATUS Result;

    if (MEMFS_MAX_PATH <= wcslen(FileName))
        return STATUS_OBJECT_NAME_INVALID;

    FileNode = MemfsFileNodeMapGet(Memfs->FileNodeMap, FileName);
    if (0 == FileNode)
    {
        Result = STATUS_OBJECT_NAME_NOT_FOUND;
        MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileName, &Result);
        return Result;
    }

    if (CreateOptions & FILE_DELETE_ON_CLOSE) // disable deletions
    {
        return STATUS_ACCESS_DENIED;
    }

    MemfsFileNodeReference(FileNode);
    *PFileNode = FileNode;
    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS Overwrite(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    return STATUS_ACCESS_DENIED;
}

static VOID Cleanup(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, PWSTR FileName, ULONG Flags)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;
    MEMFS_FILE_NODE* MainFileNode = FileNode;

    assert(0 != Flags); /* FSP_FSCTL_VOLUME_PARAMS::PostCleanupWhenModifiedOnly ensures this */

    if (Flags & FspCleanupSetArchiveBit)
    {
        if (0 == (MainFileNode->FileInfo.FileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            MainFileNode->FileInfo.FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    }

    if (Flags & (FspCleanupSetLastAccessTime | FspCleanupSetLastWriteTime | FspCleanupSetChangeTime))
    {
        UINT64 SystemTime = MemfsGetSystemTime();

        if (Flags & FspCleanupSetLastAccessTime)
            MainFileNode->FileInfo.LastAccessTime = SystemTime;
        if (Flags & FspCleanupSetLastWriteTime)
            MainFileNode->FileInfo.LastWriteTime = SystemTime;
        if (Flags & FspCleanupSetChangeTime)
            MainFileNode->FileInfo.ChangeTime = SystemTime;
    }

    if (Flags & FspCleanupSetAllocationSize)
    {
        UINT64 AllocationUnit = MEMFS_SECTOR_SIZE * MEMFS_SECTORS_PER_ALLOCATION_UNIT;
        UINT64 AllocationSize = (FileNode->FileInfo.FileSize + AllocationUnit - 1) /
            AllocationUnit * AllocationUnit;

        SetFileSizeInternal(FileSystem, FileNode, AllocationSize, TRUE);
    }

    if ((Flags & FspCleanupDelete) && !MemfsFileNodeMapHasChild(Memfs->FileNodeMap, FileNode))
    {
        MemfsFileNodeMapRemove(Memfs->FileProvider, Memfs->FileNodeMap, FileNode);
    }
}

static VOID Close(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;

    MemfsFileNodeDereference(Memfs->FileProvider, FileNode);
}

static NTSTATUS Read(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length,
    PULONG PBytesTransferred)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;
    UINT64 EndOffset;

    if (Offset >= FileNode->FileInfo.FileSize)
        return STATUS_END_OF_FILE;

    EndOffset = Offset + Length;
    if (EndOffset > FileNode->FileInfo.FileSize)
        EndOffset = FileNode->FileInfo.FileSize;

    Memfs->FileProvider->Read(FileNode->FileData, Buffer, Offset, (size_t)(EndOffset - Offset), PBytesTransferred);

    return STATUS_SUCCESS;
}

static NTSTATUS Write(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length,
    BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo,
    PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo)
{
    *PBytesTransferred = 0;
    return STATUS_ACCESS_DENIED;
}

NTSTATUS Flush(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;

    if (0 != FileNode)
    {
        MemfsFileNodeGetFileInfo(FileNode, FileInfo);
    }

    return STATUS_SUCCESS;
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, UINT32 FileAttributes,
    UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;

    if (INVALID_FILE_ATTRIBUTES != FileAttributes)
        FileNode->FileInfo.FileAttributes = FileAttributes;
    if (0 != CreationTime)
        FileNode->FileInfo.CreationTime = CreationTime;
    if (0 != LastAccessTime)
        FileNode->FileInfo.LastAccessTime = LastAccessTime;
    if (0 != LastWriteTime)
        FileNode->FileInfo.LastWriteTime = LastWriteTime;
    if (0 != ChangeTime)
        FileNode->FileInfo.ChangeTime = ChangeTime;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS SetFileSizeInternal(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize)
{
    return STATUS_INSUFFICIENT_RESOURCES; // might set to access denied, i'm unsure
}

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize,
    FSP_FSCTL_FILE_INFO* FileInfo)
{
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;
    NTSTATUS Result;

    // i mean, this always returns insufficient resources...
    Result = SetFileSizeInternal(FileSystem, FileNode0, NewSize, SetAllocationSize);
    if (!NT_SUCCESS(Result))
        return Result;

    MemfsFileNodeGetFileInfo(FileNode, FileInfo);

    return STATUS_SUCCESS;
}

static NTSTATUS CanDelete(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, PWSTR FileName)
{
    return STATUS_ACCESS_DENIED;
}

static NTSTATUS Rename(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0,
    PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists)
{
    return STATUS_ACCESS_DENIED;
}

static NTSTATUS GetSecurity(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0,
    PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize)
{
    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;

    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;

    if (Memfs->SecuritySize > * PSecurityDescriptorSize)
    {
        *PSecurityDescriptorSize = Memfs->SecuritySize;
        return STATUS_BUFFER_OVERFLOW;
    }

    *PSecurityDescriptorSize = Memfs->SecuritySize;
    if (0 != SecurityDescriptor)
        memcpy(SecurityDescriptor, Memfs->Security, Memfs->SecuritySize);

    return STATUS_SUCCESS;
}

static NTSTATUS SetSecurity(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0,
    SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor)
{
    return STATUS_ACCESS_DENIED;
}

typedef struct _MEMFS_READ_DIRECTORY_CONTEXT
{
    PVOID Buffer;
    ULONG Length;
    PULONG PBytesTransferred;
} MEMFS_READ_DIRECTORY_CONTEXT;

static BOOLEAN AddDirInfo(MEMFS_FILE_NODE* FileNode, PWSTR FileName,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    UINT8 DirInfoBuf[sizeof(FSP_FSCTL_DIR_INFO) + sizeof FileNode->FileName];
    FSP_FSCTL_DIR_INFO* DirInfo = (FSP_FSCTL_DIR_INFO*)DirInfoBuf;
    WCHAR Root[2] = L"\\";
    PWSTR Remain, Suffix;

    if (0 == FileName)
    {
        FspPathSuffix(FileNode->FileName, &Remain, &Suffix, Root);
        FileName = Suffix;
        FspPathCombine(FileNode->FileName, Suffix);
    }

    memset(DirInfo->Padding, 0, sizeof DirInfo->Padding);
    DirInfo->Size = (UINT16)(sizeof(FSP_FSCTL_DIR_INFO) + wcslen(FileName) * sizeof(WCHAR));
    DirInfo->FileInfo = FileNode->FileInfo;
    memcpy(DirInfo->FileNameBuf, FileName, DirInfo->Size - sizeof(FSP_FSCTL_DIR_INFO));

    return FspFileSystemAddDirInfo(DirInfo, Buffer, Length, PBytesTransferred);
}

static BOOLEAN ReadDirectoryEnumFn(MEMFS_FILE_NODE* FileNode, PVOID Context0)
{
    MEMFS_READ_DIRECTORY_CONTEXT* Context = (MEMFS_READ_DIRECTORY_CONTEXT*)Context0;

    return AddDirInfo(FileNode, 0,
        Context->Buffer, Context->Length, Context->PBytesTransferred);
}

static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM* FileSystem,
    PVOID FileNode0, PWSTR Pattern, PWSTR Marker,
    PVOID Buffer, ULONG Length, PULONG PBytesTransferred)
{
    assert(0 == Pattern);

    MEMFS* Memfs = (MEMFS*)FileSystem->UserContext;
    MEMFS_FILE_NODE* FileNode = (MEMFS_FILE_NODE*)FileNode0;
    MEMFS_FILE_NODE* ParentNode;
    MEMFS_READ_DIRECTORY_CONTEXT Context;
    NTSTATUS Result;

    Context.Buffer = Buffer;
    Context.Length = Length;
    Context.PBytesTransferred = PBytesTransferred;

    if (L'\0' != FileNode->FileName[1])
    {
        /* if this is not the root directory add the dot entries */

        ParentNode = MemfsFileNodeMapGetParent(Memfs->FileNodeMap, FileNode->FileName, &Result);
        if (0 == ParentNode)
            return Result;

        if (0 == Marker)
        {
            if (!AddDirInfo(FileNode, L".", Buffer, Length, PBytesTransferred))
                return STATUS_SUCCESS;
        }
        if (0 == Marker || (L'.' == Marker[0] && L'\0' == Marker[1]))
        {
            if (!AddDirInfo(ParentNode, L"..", Buffer, Length, PBytesTransferred))
                return STATUS_SUCCESS;
            Marker = 0;
        }
    }

    if (MemfsFileNodeMapEnumerateChildren(Memfs->FileNodeMap, FileNode, Marker,
        ReadDirectoryEnumFn, &Context))
        FspFileSystemAddDirInfo(0, Buffer, Length, PBytesTransferred);

    return STATUS_SUCCESS;
}

static FSP_FILE_SYSTEM_INTERFACE MemfsInterface =
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
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/*
 * Public API
 */

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
    MEMFS** PMemfs)
{
    NTSTATUS Result;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    BOOLEAN CaseInsensitive = !!(Flags & MemfsCaseInsensitive);
    PWSTR DevicePath = MemfsNet == (Flags & MemfsDeviceMask) ?
        L"" FSP_FSCTL_NET_DEVICE_NAME : L"" FSP_FSCTL_DISK_DEVICE_NAME;
    MEMFS* Memfs;
    MEMFS_FILE_NODE* RootNode;
    BOOLEAN Inserted;

    *PMemfs = 0;

    Memfs = (MEMFS*)malloc(sizeof * Memfs);
    if (0 == Memfs)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(Memfs, 0, sizeof * Memfs);
    Memfs->MaxFileNodes = MaxFileNodes;
    Memfs->FileProvider = FileProvider;
    Memfs->Security = malloc(SecuritySize);
    memcpy(Memfs->Security, Security, SecuritySize);
    Memfs->SecuritySize = SecuritySize;

    Result = MemfsFileNodeMapCreate(CaseInsensitive, &Memfs->FileNodeMap);
    if (!NT_SUCCESS(Result))
    {
        free(Memfs);
        return Result;
    }

    memset(&VolumeParams, 0, sizeof VolumeParams);
    VolumeParams.Version = sizeof FSP_FSCTL_VOLUME_PARAMS;
    VolumeParams.SectorSize = MEMFS_SECTOR_SIZE;
    VolumeParams.SectorsPerAllocationUnit = MEMFS_SECTORS_PER_ALLOCATION_UNIT;
    VolumeParams.VolumeCreationTime = MemfsGetSystemTime();
    VolumeParams.VolumeSerialNumber = (UINT32)(MemfsGetSystemTime() / (10000 * 1000));
    VolumeParams.FileInfoTimeout = FileInfoTimeout;
    VolumeParams.CaseSensitiveSearch = !CaseInsensitive;
    VolumeParams.CasePreservedNames = 1;
    VolumeParams.UnicodeOnDisk = 1;
    VolumeParams.PersistentAcls = 1;
    VolumeParams.ReparsePoints = 1;
    VolumeParams.ReparsePointsAccessCheck = 0;
    VolumeParams.PostCleanupWhenModifiedOnly = 1;
    VolumeParams.FlushAndPurgeOnCleanup = false;
    VolumeParams.AllowOpenInKernelMode = 1;
    if (0 != VolumePrefix)
        wcscpy_s(VolumeParams.Prefix, VolumePrefix);
    wcscpy_s(VolumeParams.FileSystemName, FileSystemName);

    Result = FspFileSystemCreate(DevicePath, &VolumeParams, &MemfsInterface, &Memfs->FileSystem);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeMapDelete(Memfs->FileProvider, Memfs->FileNodeMap);
        free(Memfs);
        return Result;
    }

    Memfs->FileSystem->UserContext = Memfs;

    wcscpy_s(Memfs->VolumeLabel, VolumeLabel);
    Memfs->VolumeTotal = VolumeTotal;
    Memfs->VolumeFree = VolumeFree;

    /*
     * Create root directory.
     */

    Result = MemfsFileNodeCreate(Memfs->FileProvider, L"\\", &RootNode);
    if (!NT_SUCCESS(Result))
    {
        MemfsDelete(Memfs);
        return Result;
    }

    RootNode->FileInfo.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;

    Result = MemfsFileNodeMapInsert(Memfs->FileNodeMap, RootNode, &Inserted);
    if (!NT_SUCCESS(Result))
    {
        MemfsFileNodeDelete(Memfs->FileProvider, RootNode);
        MemfsDelete(Memfs);
        return Result;
    }

    *PMemfs = Memfs;

    return STATUS_SUCCESS;
}

VOID MemfsDelete(MEMFS* Memfs)
{
    FspFileSystemDelete(Memfs->FileSystem);

    MemfsFileNodeMapDelete(Memfs->FileProvider, Memfs->FileNodeMap);

    CloseProvider(Memfs->FileProvider);

    free(Memfs);
}

NTSTATUS MemfsStart(MEMFS* Memfs)
{
    return FspFileSystemStartDispatcher(Memfs->FileSystem, 0);
}

VOID MemfsStop(MEMFS* Memfs)
{
    FspFileSystemStopDispatcher(Memfs->FileSystem);
}

FSP_FILE_SYSTEM* MemfsFileSystem(MEMFS* Memfs)
{
    return Memfs->FileSystem;
}