#pragma once

#include <memory>
#include <filesystem>

#include "containers/cancel_flag.h"
#include "filesystem/memfs.h"
#include "web/manifest.h"
#include "storage/storage.h"

namespace fs = std::filesystem;

typedef std::function<void(uint32_t max)> ProgressSetMaxHandler;
typedef std::function<void()> ProgressIncrHandler;
typedef std::function<void(const char* error)> ErrorHandler;
typedef std::function<bool()> EnforceSymlinkCreationHandler;

class MountedBuild {
public:
	MountedBuild(MANIFEST* manifest, fs::path mountDir, fs::path cachePath, ErrorHandler error);
	~MountedBuild();

	bool SetupCacheDirectory();
	bool SetupGameDirectory(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount, fs::path gameDir, EnforceSymlinkCreationHandler enforceSymlinkCreation);
	bool StartStorage(uint32_t storageFlags);
	bool PreloadAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount);
	void PurgeUnusedChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag);
	void VerifyAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount);
	void LaunchGame(fs::path gameDir, const char* additionalArgs);
	bool Mount();
	bool Unmount();
	bool Mounted();

private:
	void LogError(const char* format, ...);

	PVOID FileOpen(PCWSTR fileName, UINT64* fileSize);
	void FileClose(PVOID Handle);
	void FileRead(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead);

	fs::path MountDir;
	fs::path CacheDir;
	MANIFEST* Manifest;
	STORAGE* Storage;
	MEMFS* Memfs;
	MEMFS_FILE_PROVIDER* Provider;
	ErrorHandler Error;
};