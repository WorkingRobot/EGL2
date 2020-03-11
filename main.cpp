/**
 * @file memfs-main.c
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

#include <winfsp/winfsp.h>
#include "memfs/memfs.h"
#include <sddl.h>

#define info(format, ...)               FspServiceLog(EVENTLOG_INFORMATION_TYPE, format, __VA_ARGS__)
#define warn(format, ...)               FspServiceLog(EVENTLOG_WARNING_TYPE, format, __VA_ARGS__)
#define fail(format, ...)               FspServiceLog(EVENTLOG_ERROR_TYPE, format, __VA_ARGS__)

static void InitializeFilesystem(MEMFS* Memfs) {
    CreateFsFile(Memfs, L"\\eee", false);
    //CreateFsFile(Memfs, L"\\eee\\text.txt", false);
}

static PVOID FileOpen(PCWSTR fileName, UINT64* fileSize) {
    wprintf(L"OPENING %s\n", fileName);
    *fileSize = 400;
    return (void*)34;
}

static void FileClose(PVOID Handle) {
    wprintf(L"CLOSING %d\n", (int)Handle);
    // do nothing
}

static void FileRead(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead) {
    wprintf(L"READING %d\n", (int)Handle);
    memset(Buffer, 0x0F, length);
    *bytesRead = length;
}

NTSTATUS SvcStart(FSP_SERVICE* Service, ULONG argc, PWSTR* argv)
{
    MEMFS* Memfs = 0;
    NTSTATUS Result;

    FspDebugLogSetHandle(GetStdHandle(STD_ERROR_HANDLE));

    MEMFS_FILE_PROVIDER* Provider = CreateProvider(FileOpen, FileClose, FileRead);

    Result = MemfsCreateFunnel(
        MemfsDisk, // flags
        INFINITE, // file timeout
        1024, // max file nodes/files
        0, // file system name
        0, // volume prefix
        Provider,
        &Memfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot create MEMFS");
        goto exit;
    }

    InitializeFilesystem(Memfs);

    FspFileSystemSetDebugLog(MemfsFileSystem(Memfs), 0); // can also be -1 for all flags

    {
        PSECURITY_DESCRIPTOR RootSecurity;

        const TCHAR* rootSddl =
            SDDL_DACL SDDL_DELIMINATOR
                SDDL_ACE_COND_BEGIN
                    SDDL_ACCESS_ALLOWED SDDL_SEPERATOR // Allowed (ace_type)
                    SDDL_OBJECT_INHERIT SDDL_CONTAINER_INHERIT SDDL_SEPERATOR // Inherit to containers and objects (ace_flags)
                    SDDL_GENERIC_READ SDDL_GENERIC_EXECUTE SDDL_SEPERATOR // Allow reads and executes (rights)
                    SDDL_SEPERATOR // object_guid
                    SDDL_SEPERATOR // inherit_object_guid
                    SDDL_EVERYONE // Give rights to everyone (account_sid)
                SDDL_ACE_COND_END;
        // D:(A;OICI;GRGX;;;WD)

        if (!ConvertStringSecurityDescriptorToSecurityDescriptor(rootSddl, SDDL_REVISION_1, &RootSecurity, NULL)) {
            fail(L"invalid sddl: %08x", FspNtStatusFromWin32(GetLastError()));
            goto exit;
        }
        Result = FspFileSystemSetMountPointEx(MemfsFileSystem(Memfs), L"C:\\aaaa", RootSecurity);
        LocalFree(RootSecurity);
        if (!NT_SUCCESS(Result))
        {
            fail(L"cannot mount MEMFS %08x", Result);
            goto exit;
        }
    }

    Result = MemfsStart(Memfs);
    if (!NT_SUCCESS(Result))
    {
        fail(L"cannot start MEMFS %08x", Result);
        goto exit;
    }

    Service->UserContext = Memfs;
    Result = STATUS_SUCCESS;
    
exit:
    if (!NT_SUCCESS(Result) && 0 != Memfs)
        MemfsDelete(Memfs);

    return Result;
}

NTSTATUS SvcStop(FSP_SERVICE* Service)
{
    MEMFS* Memfs = (MEMFS*)Service->UserContext;

    MemfsStop(Memfs);
    MemfsDelete(Memfs);

    return STATUS_SUCCESS;
}

int wmain(int argc, wchar_t** argv)
{
    return FspServiceRun(L"EGL2", SvcStart, SvcStop, 0);
}