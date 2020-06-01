#pragma once

#include "containers/cancel_flag.h"
#include "filesystem/egfs.h"
#include "web/manifest/manifest.h"
#include "storage/storage.h"

#include <filesystem>
namespace fs = std::filesystem;

typedef std::function<void(uint32_t max)> ProgressSetMaxHandler;
typedef std::function<void()> ProgressIncrHandler;

class MountedBuild {
public:
	MountedBuild(Manifest manifest, fs::path mountDir, fs::path cachePath, uint32_t storageFlags, uint32_t memoryPoolCapacity);
	~MountedBuild();

	static bool SetupCacheDirectory(fs::path CacheDir);
	bool SetupGameDirectory(uint32_t threadCount);
	bool PreloadAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount);
	void PurgeUnusedChunks();
	void VerifyAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount);
	uint32_t GetMissingChunkCount();
	void LaunchGame(const char* additionalArgs);

private:
	void PreloadFile(File& File, uint32_t ThreadCount, cancel_flag& cancelFlag);

	void FileRead(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead);

	fs::path MountDir;
	fs::path CacheDir;
	Manifest Build;
	Storage StorageData;
	std::unique_ptr<EGFS> Egfs;
};