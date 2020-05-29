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
        LOG_ERROR("can't create cachedir %s", CacheDir.string().c_str());
        return false;
    }

    char cachePartFolder[3];
    for (int i = 0; i < 256; ++i) {
        sprintf(cachePartFolder, "%02X", i);
        fs::create_directory(CacheDir / cachePartFolder);
    }

    auto oldGameDir = CacheDir / "game";
    if (fs::is_directory(oldGameDir)) {
        LOG_INFO("Removing old game folder %s", oldGameDir.string().c_str());
        std::error_code ec;
        for (auto& f : fs::recursive_directory_iterator(oldGameDir)) {
            fs::permissions(f, fs::perms::_All_write, ec);
            if (ec) {
                LOG_INFO(f.path().string().c_str());
                LOG_ERROR(ec.message().c_str());
            }
        }
        fs::remove_all(oldGameDir, ec);
        if (ec) {
            LOG_ERROR("Could not remove old game folder: %s", ec.message().c_str());
        }
    }
    return true;
}

void MountedBuild::PreloadFile(File& File, uint32_t ThreadCount, cancel_flag& cancelFlag) {
    std::deque<std::thread> threads;
    int n = 0;
    for (auto& chunkPart : File.ChunkParts) {
        if (cancelFlag.cancelled()) {
            break;
        }
        if (StorageData.IsChunkDownloaded(chunkPart)) {
            continue;
        }

        // cheap semaphore, keeps thread count low instead of having 81k threads pile up
        while (threads.size() >= ThreadCount) {
            threads.front().join();
            threads.pop_front();
        }

        
        threads.emplace_back([&, this]() {
            StorageData.GetChunkPart(chunkPart);
        });
    }

    if (cancelFlag.cancelled()) {
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

bool MountedBuild::SetupGameDirectory(uint32_t threadCount) {
    auto gameDir = CacheDir / GAME_DIR;

    if (!fs::is_directory(gameDir) && !fs::create_directories(gameDir)) {
        LOG_ERROR("can't create gamedir");
        return false;
    }

    bool symlinkCreationEnforced = false;
    cancel_flag flag;
    for (auto& file : Build.FileManifestList) {
        fs::path filePath = file.FileName;
        fs::path folderPath = filePath.parent_path();

        if (!fs::create_directories(gameDir / folderPath) && !fs::is_directory(gameDir / folderPath)) {
            LOG_ERROR("can't create %s\n", (gameDir / folderPath).string().c_str());
            goto continueFileLoop;
        }
        do {
            if (folderPath.filename() == "Binaries") {
                if (!CompareFile(file, gameDir / filePath)) {
                    PreloadFile(file, threadCount, flag);
                    if (fs::status(gameDir / filePath).type() == fs::file_type::regular) {
                        fs::permissions(gameDir / filePath, fs::perms::_All_write, fs::perm_options::add); // copying over a file from the drive gives it the read-only attribute, this overrides that
                    }
                    if (!fs::copy_file(MountDir / filePath, gameDir / filePath, fs::copy_options::overwrite_existing)) {
                        LOG_ERROR("failed to copy %s", filePath.string().c_str());
                    }
                }
                goto continueFileLoop;
            }
            folderPath = folderPath.parent_path();
        } while (folderPath != folderPath.root_path());

        if (fs::is_symlink(gameDir / filePath)) {
            if (fs::read_symlink(gameDir / filePath) != MountDir / filePath) {
                fs::remove(gameDir / filePath); // remove if exists and is invalid
                fs::create_symlink(MountDir / filePath, gameDir / filePath);
            }
        }
        else {
            fs::create_symlink(MountDir / filePath, gameDir / filePath);
        }
    continueFileLoop:
        ;
    }
    return true;
}

bool MountedBuild::PreloadAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount) {
    LOG_DEBUG("preloading");
    setMax(GetMissingChunkCount());

    std::deque<std::thread> threads;

    int n = 0;
    for (auto& chunk : Build.ChunkManifestList) {
        if (cancelFlag.cancelled()) {
            break;
        }
        if (StorageData.IsChunkDownloaded(chunk)) {
            //LogError("already downloaded", ChunkCount);
            //progress();
            continue;
        }

        // cheap semaphore, keeps thread count low instead of having 81k threads pile up
        while (threads.size() >= threadCount) {
            threads.front().join();
            threads.pop_front();
        }

        
        threads.emplace_back([&, this]() {
            StorageData.GetChunk(chunk);
            progress();
        });
    }

    if (cancelFlag.cancelled()) {
        for (auto& thread : threads) {
            thread.detach();
        }
    }
    else {
        for (auto& thread : threads) {
            thread.join();
        }
    }
    LOG_DEBUG("preloaded");
    return true;
}

#define HTONLL(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define NTOHLL(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
auto guidHash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto guidEqual = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
void MountedBuild::PurgeUnusedChunks() {
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

void MountedBuild::VerifyAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount) {
    setMax((std::min)(Build.ChunkManifestList.size(), (size_t)std::count_if(fs::recursive_directory_iterator(CacheDir), fs::recursive_directory_iterator(), [this](const fs::directory_entry& f) { return f.is_regular_file() && ValidChunkFile(CacheDir, f.path()) == 0; })));

    std::deque<std::thread> threads;

    for (auto& chunk : Build.ChunkManifestList) {
        if (cancelFlag.cancelled()) {
            break;
        }
        if (!StorageData.IsChunkDownloaded(chunk)) {
            continue;
        }

        // cheap semaphore, keeps thread count low instead of having 81k threads pile up
        while (threads.size() >= threadCount) {
            threads.front().join();
            progress();
            threads.pop_front();
        }

        threads.emplace_back(std::thread([=, this]() {
            if (!StorageData.VerifyChunk(chunk)) {
                LOG_WARN("Invalid hash");
                StorageData.DeleteChunk(chunk);
                StorageData.GetChunk(chunk);
            }
        }));
    }

    if (cancelFlag.cancelled()) {
        for (auto& thread : threads) {
            thread.detach();
        }
    }
    else {
        for (auto& thread : threads) {
            thread.join();
            progress();
        }
    }
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
            auto chunkBuffer = StorageData.GetChunkPart(*chunkPart);
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