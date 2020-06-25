#include "MountedBuild.h"

#define GAME_DIR   "workaround"
#define LOG_FLAGS  0 // can also be -1 for all flags
#define MB_SDDL_OWNER "S-1-5-18" // Local System
#define MB_SDDL_DATA  "P(A;ID;FRFX;;;WD)" // Protected from inheritance, allows it and it's children to give read and execure access to everyone
#define SDDL_ROOT  L"D:" MB_SDDL_DATA
#define SDDL_FILE  L"O:" MB_SDDL_OWNER "G:" MB_SDDL_OWNER "D:" MB_SDDL_DATA

#ifndef LOG_SECTION
#define LOG_SECTION "MountedBuild"
#endif

#include "Logger.h"
#include "Stats.h"
#include "containers/file_sha.h"
#include "web/manifest/manifest.h"

#include <algorithm>
#include <deque>
#include <numeric>
#include <sddl.h>
#include <set>
#include <unordered_set>

MountedBuild::MountedBuild(Manifest manifest, fs::path mountDir, fs::path cachePath, uint32_t storageFlags, uint32_t memoryPoolCapacity) :
    Build(manifest),
    MountDir(mountDir),
    CacheDir(cachePath),
    StorageData(storageFlags, memoryPoolCapacity, CacheDir, Build.CloudDir)
{
    LOG_DEBUG("new (v: %s, mount: %s, cache: %s)", Build.BuildVersion.c_str(), MountDir.string().c_str(), CacheDir.string().c_str());

    LOG_DEBUG("creating params");
    {
        PVOID securityDescriptor;
        ULONG securityDescriptorSize;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptor(SDDL_FILE, SDDL_REVISION_1, &securityDescriptor, &securityDescriptorSize)) {
            LOG_ERROR("invalid sddl (%08x)", FspNtStatusFromWin32(GetLastError()));
        }

        EGFS_PARAMS params;

        wcscpy_s(params.FileSystemName, L"EGFS");
        params.VolumePrefix[0] = '\0';
        wcscpy_s(params.VolumeLabel, L"EGL2");
        params.VolumeTotal = Build.GetInstallSize();
        params.VolumeFree = params.VolumeTotal - Build.GetDownloadSize();
        params.Security = securityDescriptor;
        params.SecuritySize = securityDescriptorSize;

        params.OnRead = [this](PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead) {
            FileRead(Handle, Buffer, offset, length, bytesRead);
        };

        params.SectorSize = 512;
        params.SectorsPerAllocationUnit = 1; // sectors per cluster (in hardware terms)
        params.VolumeCreationTime = 0;
        params.VolumeSerialNumber = 0;
        params.FileInfoTimeout = INFINITE; // https://github.com/billziss-gh/winfsp/issues/19#issuecomment-289853591
        params.CaseSensitiveSearch = true;
        params.CasePreservedNames = true;
        params.UnicodeOnDisk = true;
        params.PersistentAcls = true;
        params.ReparsePoints = true;
        params.ReparsePointsAccessCheck = false;
        params.PostCleanupWhenModifiedOnly = true;
        params.FlushAndPurgeOnCleanup = false;
        params.AllowOpenInKernelMode = true;

        params.LogFlags = 0; // -1 enables all (slowdowns imminent due to console spam, though)

        NTSTATUS Result;
        Egfs = std::make_unique<EGFS>(&params, Result);
        if (!NT_SUCCESS(Result)) {
            LOG_ERROR("could not create egfs (%08x)", Result);
            return;
        }
        LocalFree(securityDescriptor);
    }

    LOG_DEBUG("adding files");
    for (auto& file : Build.FileManifestList) {
        Egfs->AddFile(file.FileName, &file, file.GetFileSize());
    }

    LOG_DEBUG("setting mount point");
    {
        PVOID rootSecurity;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptor(SDDL_ROOT, SDDL_REVISION_1, &rootSecurity, NULL)) {
            LOG_ERROR("invalid root sddl (%08x)", FspNtStatusFromWin32(GetLastError()));
            return;
        }
        Egfs->SetMountPoint(MountDir.native().c_str(), rootSecurity);
        LocalFree(rootSecurity);
    }

    LOG_DEBUG("starting");
    Egfs->Start();
}

MountedBuild::~MountedBuild() {

}

// 0: valid
// 1: bad parent
// 2: at base dir
inline int ValidChunkFile(fs::path& CacheDir, fs::path ChunkPath) {
    if (ChunkPath.parent_path() == CacheDir) {
        return 2;
    }
    if (ChunkPath.parent_path().parent_path() != CacheDir) {
        return 1;
    }
    auto name = ChunkPath.parent_path().filename().string();
    return (name.size() == 2 && isxdigit(name[0]) && isxdigit(name[1])) ? 0 : 1;
}

bool MountedBuild::SetupCacheDirectory(fs::path CacheDir) {
    if (!fs::is_directory(CacheDir) && !fs::create_directories(CacheDir)) {
        LOG_ERROR("can't create cachedir", CacheDir.string().c_str());
        return false;
    }

    if (!fs::is_directory(CacheDir / GAME_DIR) && !fs::create_directory(CacheDir / GAME_DIR)) {
        LOG_ERROR("can't create gamedir");
        return false;
    }

    char cachePartFolder[3];
    for (int i = 0; i < 256; ++i) {
        sprintf(cachePartFolder, "%02X", i);
        if (!fs::is_directory(CacheDir / cachePartFolder) && !fs::create_directory(CacheDir / cachePartFolder)) {
            LOG_ERROR("can't create dir %s", cachePartFolder);
            return false;
        }
    }
    return true;
}

void MountedBuild::PreloadFile(File& File, uint32_t ThreadCount, cancel_flag& flag) {
    std::deque<std::thread> threads;
    int n = 0;
    for (auto& chunkPart : File.ChunkParts) {
        if (flag.cancelled()) {
            break;
        }
        if (StorageData.IsChunkDownloaded(chunkPart)) {
            continue;
        }

        // lightweight "semaphore"
        while (threads.size() >= ThreadCount) {
            threads.front().join();
            threads.pop_front();
        }

        
        threads.emplace_back([&, this]() {
            StorageData.GetChunkPart(chunkPart, flag);
        });
    }

    if (flag.cancelled()) {
        for (auto& thread : threads) {
            thread.detach();
        }
    }
    else {
        for (auto& thread : threads) {
            thread.join();
        }
    }
}

bool inline CompareFile(File& File, fs::path FilePath) {
    if (fs::status(FilePath).type() != fs::file_type::regular) {
        return false;
    }

    if (fs::file_size(FilePath) != File.GetFileSize()) {
        return false;
    }

    char FileSha[20];
    if (!SHAFile(FilePath, FileSha)) {
        return false;
    }

    return !memcmp(FileSha, File.ShaHash, 20);
}

void MountedBuild::SetupGameDirectory(ProgressSetMaxHandler setMax, ProgressIncrHandler onProg, ProgressFinishHandler onFinish, cancel_flag& flag, uint32_t threadCount) {
    LOG_DEBUG("setting up game dir");
    setMax(Build.FileManifestList.size());

    std::deque<std::thread> threads;
    threads.emplace_back([&, this]() { // checking symlinks really doesn't take much time at all, just check for the workaround binaries
        setMax(std::count_if(Build.FileManifestList.begin(), Build.FileManifestList.end(), [](File& file) {
            fs::path folderPath = file.FileName;
            do {
                if (folderPath.filename() == "Binaries") {
                    return true;
                }
                folderPath = folderPath.parent_path();
            } while (folderPath != folderPath.root_path());
            return false;
        }));
    });

    auto gameDir = CacheDir / GAME_DIR;
    for (auto& file : Build.FileManifestList) {
        // lightweight "semaphore"
        while (threads.size() >= threadCount) {
            threads.front().join();
            threads.pop_front();
        }

        if (flag.cancelled()) {
            break;
        }

        threads.emplace_back([&, gameDir, this]() {
            std::error_code ec;
            fs::path filePath = gameDir / file.FileName;
            fs::path folderPath = filePath.parent_path();

            if (!fs::create_directories(gameDir / folderPath, ec) && !fs::is_directory(gameDir / folderPath)) {
                LOG_ERROR("Can't create folder %s for %s, error %s", folderPath.string().c_str(), filePath.string().c_str(), ec.message().c_str());
                return;
            }
            do {
                if (folderPath.filename() == "Binaries") {
                    if (!CompareFile(file, filePath)) {
                        LOG_DEBUG("Preloading %s", file.FileName.c_str());
                        PreloadFile(file, threadCount, flag);
                        LOG_DEBUG("Copying %s", file.FileName.c_str());
                        if (!fs::remove(filePath, ec)) {
                            LOG_ERROR("Could not delete file to overwrite %s, error %s", filePath.string().c_str(), ec.message().c_str());
                        }
                        if (!fs::copy_file(MountDir / file.FileName, filePath, fs::copy_options::overwrite_existing, ec)) {
                            LOG_ERROR("Could not copy file %s, error %s", filePath.string().c_str(), ec.message().c_str());
                        }
                        if (fs::status(filePath).type() == fs::file_type::regular) {
                            SetFileAttributes((filePath).c_str(), FILE_ATTRIBUTE_NORMAL); // copying over a file from the drive gives it the read-only attribute, this overrides that
                        }
                        LOG_DEBUG("Set up %s", file.FileName.c_str());
                    }
                    onProg();
                    return;
                }
                folderPath = folderPath.parent_path();
            } while (folderPath != folderPath.root_path());

            if (fs::is_symlink(filePath)) {
                if (fs::read_symlink(filePath) != MountDir / file.FileName) {
                    LOG_DEBUG("Replacing symlink %s", file.FileName.c_str());
                    fs::remove(filePath, ec); // remove if exists and is invalid
                    if (!ec) {
                        fs::create_symlink(MountDir / file.FileName, filePath, ec);
                    }
                    if (ec) {
                        LOG_ERROR("Can't replace symlink %s, error %s", filePath, ec.message().c_str());
                    }
                    else {
                        LOG_DEBUG("Set up %s", file.FileName.c_str());
                    }
                }
            }
            else {
                LOG_DEBUG("Creating symlink %s", file.FileName.c_str());
                fs::create_symlink(MountDir / file.FileName, filePath, ec);
                if (ec) {
                    LOG_ERROR("Can't create symlink %s, error %s", filePath, ec.message().c_str());
                }
                else {
                    LOG_DEBUG("Set up %s", file.FileName.c_str());
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    onFinish();
    LOG_DEBUG("set up game dir");
}

void MountedBuild::PreloadAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler onProg, ProgressFinishHandler onFinish, cancel_flag& flag, uint32_t threadCount) {
    LOG_DEBUG("preloading");
    setMax(Build.ChunkManifestList.size());

    auto purgeThread = std::thread([&, this] {
        PurgeUnusedChunks(flag);
    });

    auto setMaxThread = std::thread([&, this] {
        setMax(GetMissingChunkCount());
    });

    auto threads = std::make_unique<std::thread[]>(threadCount);

    std::mutex iterMtx;
    std::condition_variable iterCv;

    auto chunkIter = Build.ChunkManifestList.begin();
    auto chunkEnd = Build.ChunkManifestList.end();
    std::atomic_bool chunkDone = false;

    auto GetChunk = [&] {
        std::unique_lock<std::mutex> lk(iterMtx);
        for (; chunkIter != chunkEnd; ++chunkIter) {
            if (!StorageData.IsChunkDownloaded(*chunkIter)) {
                return chunkIter++;
            }
        }
        chunkDone = true;
        lk.unlock();
        iterCv.notify_all();
        return chunkEnd;
    };

    auto threadJob = [&] {
        while (!chunkDone && !flag.cancelled()) {
            auto chunk = GetChunk();
            if (chunk == chunkEnd) {
                return;
            }
            StorageData.DownloadChunk(*chunk, flag);
            onProg();
        }
    };

    Client::SetPoolSize(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        threads[i] = std::thread(threadJob);
    }

    std::unique_lock<std::mutex> lk(iterMtx);
    iterCv.wait(lk, [&] { return chunkDone || flag.cancelled(); });

    Client::SetPoolSize(-1);

    for (int i = 0; i < threadCount; ++i) {
        threads[i].join();
    }

    purgeThread.join();
    setMaxThread.join();
    onFinish();
    LOG_DEBUG("preloaded");
}

void MountedBuild::VerifyAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler onProg, ProgressFinishHandler onFinish, cancel_flag& flag, uint32_t threadCount) {
    LOG_DEBUG("verifying");
    setMax(Build.ChunkManifestList.size());

    std::deque<std::thread> threads;
    threads.emplace_back([&, this]() {
        size_t cnt = std::count_if(fs::recursive_directory_iterator(CacheDir), fs::recursive_directory_iterator(),
            [this](const fs::directory_entry& f) {
                return f.is_regular_file() && ValidChunkFile(CacheDir, f.path()) == 0;
            });
        setMax((std::min)(Build.ChunkManifestList.size(), cnt));
    });

    for (auto& chunk : Build.ChunkManifestList) {
        if (!StorageData.IsChunkDownloaded(chunk)) {
            continue;
        }

        // lightweight "semaphore"
        while (threads.size() >= threadCount) {
            threads.front().join();
            threads.pop_front();
        }

        if (flag.cancelled()) {
            break;
        }

        threads.emplace_back(std::thread([=, this, &flag]() {
            if (!StorageData.VerifyChunk(chunk, flag)) {
                if (flag.cancelled()) return;
                LOG_WARN("Invalid hash for %s", chunk->GetGuid().c_str());
                StorageData.DeleteChunk(chunk);
                if (flag.cancelled()) return;
                StorageData.DownloadChunk(chunk, flag, true);
            }
            onProg();
        }));
    }
    for (auto& thread : threads) {
        thread.join();
    }
    onFinish();
    LOG_DEBUG("preloaded");
}

#define HTONLL(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define NTOHLL(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
auto guidHash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto guidEqual = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
void MountedBuild::PurgeUnusedChunks(cancel_flag& flag) {
    LOG_DEBUG("purging");
    std::unordered_set<char*, decltype(guidHash), decltype(guidEqual)> ManifestGuids;
    ManifestGuids.reserve(Build.ChunkManifestList.size());
    for (auto& chunk : Build.ChunkManifestList) {
        ManifestGuids.insert(chunk->Guid);
    }

    char guidBuffer[16];
    char guidBuffer2[16];
    auto iterator = fs::recursive_directory_iterator(CacheDir);
    for (auto& p : iterator) {
        if (flag.cancelled()) {
            return;
        }

        if (!p.is_regular_file()) {
            continue;
        }

        auto res = ValidChunkFile(CacheDir, p.path());
        if (res == 1) {
            iterator.pop();
            continue;
        }
        else if (res == 2) {
            continue;
        }

        auto filename = p.path().filename().string();
        if (filename.size() == 32 && sscanf(filename.c_str(), "%016llX%016llX", guidBuffer2, guidBuffer2 + 8) == 2) {
            *(unsigned long long*)guidBuffer = HTONLL(*(unsigned long long*)guidBuffer2);
            *(unsigned long long*)(guidBuffer + 8) = HTONLL(*(unsigned long long*)(guidBuffer2 + 8));
            if (!ManifestGuids.erase(guidBuffer)) {
                fs::remove(p);
            }
        }
    }
    LOG_DEBUG("purged");
}

uint32_t MountedBuild::GetMissingChunkCount()
{
   return std::count_if(Build.ChunkManifestList.begin(), Build.ChunkManifestList.end(), [&](auto& a) { return !StorageData.IsChunkDownloaded(a); });
}

void MountedBuild::LaunchGame(const char* additionalArgs) {
    std::string CmdBuf = Build.LaunchCommand + " " + additionalArgs;
    fs::path exePath = CacheDir / GAME_DIR / Build.LaunchExe;

    PROCESS_INFORMATION pi;
    STARTUPINFOA si;

    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    CreateProcessA(exePath.string().c_str(), (LPSTR)CmdBuf.c_str(), NULL, NULL, FALSE, DETACHED_PROCESS, NULL, exePath.parent_path().string().c_str(), &si, &pi);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

void MountedBuild::FileRead(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead) {
    auto startTime = std::chrono::steady_clock::now();

    auto file = (File*)Handle;
    //LOG_DEBUG("Reading %s, at %d: %d", file->FileName.c_str(), offset, length);
    uint32_t ChunkStartIndex, ChunkStartOffset;
    if (file->GetChunkIndex(offset, ChunkStartIndex, ChunkStartOffset)) {
        uint32_t BytesRead = 0;
        for (auto chunkPart = file->ChunkParts.begin() + ChunkStartIndex; chunkPart != file->ChunkParts.end(); chunkPart++) {
            auto chunkBuffer = StorageData.GetChunkPart(*chunkPart, cancel_flag());
            if (((int64_t)length - (int64_t)BytesRead) > (int64_t)chunkPart->Size - (int64_t)ChunkStartOffset) { // copy the entire buffer over
                //LOG_DEBUG("Copying to %d, size %d", BytesRead, chunkPart->Size - ChunkStartOffset);
                memcpy((char*)Buffer + BytesRead, chunkBuffer.get() + ChunkStartOffset, chunkPart->Size - ChunkStartOffset);
                BytesRead += chunkPart->Size - ChunkStartOffset;
            }
            else { // copy what it needs to fill up the rest
                //LOG_DEBUG("Copying to %d, size %d", BytesRead, length - BytesRead);
                memcpy((char*)Buffer + BytesRead, chunkBuffer.get() + ChunkStartOffset, length - BytesRead);
                BytesRead += (int64_t)length - (int64_t)BytesRead;
                break;
            }
            ChunkStartOffset = 0;
        }
        Stats::ProvideCount.fetch_add(BytesRead, std::memory_order_relaxed);
        *bytesRead = BytesRead;
    }
    else {
        *bytesRead = 0;
    }

    auto endTime = std::chrono::steady_clock::now();
    Stats::LatOpCount.fetch_add(1, std::memory_order_relaxed);
    Stats::LatNsCount.fetch_add((endTime - startTime).count(), std::memory_order_relaxed);
}