#pragma once

#include "containers/cancel_flag.h"
#include "filesystem/egfs.h"
#include "web/manifest.h"
#include "storage/storage.h"

#include <filesystem>
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
	bool SetupGameDirectory(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount, EnforceSymlinkCreationHandler enforceSymlinkCreation);
	bool StartStorage(uint32_t storageFlags);
	bool PreloadAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount);
	void PurgeUnusedChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag);
	void VerifyAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount);
	void LaunchGame(const char* additionalArgs);
	bool Mount();
	bool Unmount();
	bool Mounted();

private:
	void LogError(const char* format, ...);

	void FileRead(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead);

	fs::path MountDir;
	fs::path CacheDir;
	MANIFEST* Manifest;
	STORAGE* Storage;
	EGFS* Egfs;
	ErrorHandler Error;
};