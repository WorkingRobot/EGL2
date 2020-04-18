#pragma once

#define EGFS_MAX_PATH 260

#include "dirtree.h"

#include <winfsp/winfsp.h>

#include <functional>
#include <filesystem>
namespace fs = std::filesystem;

struct EGFS_CALLBACKS {
	std::function<void(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead)> Read;
};

struct EGFS_PARAMS {
	WCHAR FileSystemName[16];
	WCHAR VolumePrefix[32]; // can be 192 length, but why would you
	WCHAR VolumeLabel[32];
	UINT64 VolumeTotal;
	UINT64 VolumeFree;
	PVOID Security;
	SIZE_T SecuritySize;
	EGFS_CALLBACKS Callbacks;

	UINT16 SectorSize;
	UINT16 SectorsPerAllocationUnit;
	UINT64 VolumeCreationTime;
	UINT32 VolumeSerialNumber;
	UINT32 FileInfoTimeout;
	bool CaseSensitiveSearch;
	bool CasePreservedNames;
	bool UnicodeOnDisk;
	bool PersistentAcls;
	bool ReparsePoints;
	bool ReparsePointsAccessCheck;
	bool PostCleanupWhenModifiedOnly;
	bool FlushAndPurgeOnCleanup;
	bool AllowOpenInKernelMode;

	UINT32 LogFlags;
};

struct EGFS_FILE {
	EGFS_FILE(UINT64 FileSize, PVOID Context, UINT64 CreationTime = 0, UINT64 AccessTime = 0, UINT64 WriteTime = 0, UINT64 ChangeTime = 0) :
		Context(Context),
		FileSize(FileSize),
		CreationTime(CreationTime),
		AccessTime(AccessTime),
		WriteTime(WriteTime),
		ChangeTime(ChangeTime)
	{ }

	PVOID Context;

	UINT64 FileSize;
	UINT64 CreationTime;
	UINT64 AccessTime;
	UINT64 WriteTime;
	UINT64 ChangeTime; // ???
};

class EGFS {
public:
	using container_type = typename DirTree<EGFS_FILE>;
	using node_type = typename container_type::node_type;

	EGFS(EGFS_PARAMS* Params, NTSTATUS& ErrorCode);
	~EGFS();

	bool SetMountPoint(PCWSTR MountDir, PVOID Security);
	void AddFile(fs::path& Path, PVOID Context, UINT64 FileSize);

	bool Start();
	bool Stop();
	bool Started();

private:
	RootTree<EGFS_FILE> Files;

	FSP_FILE_SYSTEM* FileSystem;
	EGFS_CALLBACKS Callbacks;

	PVOID Security;
	SIZE_T SecuritySize;

	WCHAR VolumeLabel[32];
	UINT64 VolumeTotal;
	UINT64 VolumeFree;

	bool IsStarted;
	
	static UINT32 GetFileAttributes(node_type* file);
	static void GetFileInfo(container_type* file, FSP_FSCTL_FILE_INFO* info);
	static void GetFileInfo(node_type* file, FSP_FSCTL_FILE_INFO* info);
	static UINT64 GetFileSize(node_type* file);

	// Interface
	static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM* FileSystem, FSP_FSCTL_VOLUME_INFO* VolumeInfo);
	static NTSTATUS SetVolumeLabel(FSP_FILE_SYSTEM* FileSystem, PWSTR VolumeLabel, FSP_FSCTL_VOLUME_INFO* VolumeInfo);
	static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, PUINT32 PFileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize);
	static NTSTATUS Create(FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, UINT32 FileAttributes, PSECURITY_DESCRIPTOR SecurityDescriptor, UINT64 AllocationSize, PVOID* PFileNode, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS Open(FSP_FILE_SYSTEM* FileSystem, PWSTR FileName, UINT32 CreateOptions, UINT32 GrantedAccess, PVOID* PFileNode, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS Overwrite(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, UINT32 FileAttributes, BOOLEAN ReplaceFileAttributes, UINT64 AllocationSize, FSP_FSCTL_FILE_INFO* FileInfo);
	static VOID Cleanup(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR FileName, ULONG Flags);
	static VOID Close(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0);
	static NTSTATUS Read(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length, PULONG PBytesTransferred);
	static NTSTATUS Write(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PVOID Buffer, UINT64 Offset, ULONG Length, BOOLEAN WriteToEndOfFile, BOOLEAN ConstrainedIo, PULONG PBytesTransferred, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS Flush(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, UINT32 FileAttributes, UINT64 CreationTime, UINT64 LastAccessTime, UINT64 LastWriteTime, UINT64 ChangeTime, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS SetFileSize(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, UINT64 NewSize, BOOLEAN SetAllocationSize, FSP_FSCTL_FILE_INFO* FileInfo);
	static NTSTATUS CanDelete(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR FileName);
	static NTSTATUS Rename(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR FileName, PWSTR NewFileName, BOOLEAN ReplaceIfExists);
	static NTSTATUS GetSecurity(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PSECURITY_DESCRIPTOR SecurityDescriptor, SIZE_T* PSecurityDescriptorSize);
	static NTSTATUS SetSecurity(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR ModificationDescriptor);
	static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM* FileSystem, PVOID FileNode0, PWSTR Pattern, PWSTR Marker, PVOID Buffer, ULONG Length, PULONG PBytesTransferred);
	
	static FSP_FILE_SYSTEM_INTERFACE FspInterface;
};