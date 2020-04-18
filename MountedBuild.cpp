#include "MountedBuild.h"

#include "containers/iterable_queue.h"
#include "containers/semaphore.h"
#include "containers/file_sha.h"

#include <set>
#include <unordered_set>
#include <sddl.h>

#define fail(format, ...)               FspServiceLog(EVENTLOG_ERROR_TYPE, format, ##__VA_ARGS__)

#define SDDL_OWNER "S-1-5-18" // Local System
#define SDDL_DATA  "P(A;ID;FRFX;;;WD)" // Protected from inheritance, allows it and it's children to give read and execure access to everyone
#define LOG_FLAGS  0 // can also be -1 for all flags

#define SDDL_ROOT  "D:" SDDL_DATA
#define SDDL_MEMFS "O:" SDDL_OWNER "G:" SDDL_OWNER "D:" SDDL_DATA

MountedBuild::MountedBuild(MANIFEST* manifest, fs::path mountDir, fs::path cachePath, ErrorHandler error) {
    this->Manifest = manifest;
    this->MountDir = mountDir;
    this->CacheDir = cachePath;
    this->Error = error;
    this->Storage = nullptr;
    this->Egfs = nullptr;
}

MountedBuild::~MountedBuild() {
    Unmount();
    if (Storage) {
        StorageDelete(Storage);
    }
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

bool MountedBuild::SetupCacheDirectory() {
    if (!fs::is_directory(CacheDir) && !fs::create_directories(CacheDir)) {
        LogError("can't create cachedir %s\n", CacheDir.string().c_str());
        return false;
    }

    char cachePartFolder[3];
    for (int i = 0; i < 256; ++i) {
        sprintf(cachePartFolder, "%02X", i);
        fs::create_directory(CacheDir / cachePartFolder);
    }
    return true;
}

void inline PreloadFile(STORAGE* Storage, MANIFEST_FILE* File, uint32_t ThreadCount, cancel_flag& cancelFlag) {
    MANIFEST_CHUNK_PART* ChunkParts;
    uint32_t ChunkCount;
    uint16_t ChunkStride;
    ManifestFileGetChunks(File, &ChunkParts, &ChunkCount, &ChunkStride);

    iterable_queue<std::thread> threads;
    for (int i = 0, n = 0; i < ChunkCount * ChunkStride && !cancelFlag.cancelled(); i += ChunkStride) {
        auto Chunk = ManifestFileChunkGetChunk((MANIFEST_CHUNK_PART*)((char*)ChunkParts + i));
        if (StorageChunkDownloaded(Storage, Chunk)) {
            continue;
        }

        // cheap semaphore, keeps thread count low instead of having 81k threads pile up
        while (threads.size() >= ThreadCount) {
            threads.front().join();
            threads.pop();
        }

        threads.push(std::thread(StorageDownloadChunk, Storage, Chunk, [&n, &ChunkCount](const char* buf, uint32_t bufSize)
        {
            printf("\r%d downloaded / %d total (%.2f%%)", ++n, ChunkCount, float(n * 100) / ChunkCount);
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
        }
    }

    printf("\r%d downloaded / %d total (%.2f%%)\n", ChunkCount, ChunkCount, float(100));
}

bool inline CompareFile(MANIFEST_FILE* File, fs::path FilePath) {
    if (fs::status(FilePath).type() != fs::file_type::regular) {
        return false;
    }

    if (fs::file_size(FilePath) != ManifestFileGetFileSize(File)) {
        return false;
    }

    char FileSha[20];
    if (!SHAFile(FilePath, FileSha)) {
        return false;
    }

    return !memcmp(FileSha, ManifestFileGetSha1(File), 20);
}

bool MountedBuild::SetupGameDirectory(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount, EnforceSymlinkCreationHandler enforceSymlinkCreation) {
    auto gameDir = CacheDir / "game";

    if (!fs::is_directory(gameDir) && !fs::create_directories(gameDir)) {
        LogError("can't create gamedir\n");
        return false;
    }

    bool symlinkCreationEnforced = false;
    MANIFEST_FILE* Files;
    uint32_t FileCount;
    uint16_t FileStride;
    char FilenameBuffer[128];
    ManifestGetFiles(Manifest, &Files, &FileCount, &FileStride);
    setMax(FileCount);
    for (int i = 0; i < FileCount * FileStride && !cancelFlag.cancelled(); i += FileStride) {
        auto File = (MANIFEST_FILE*)((char*)Files + i);
        ManifestFileGetName(File, FilenameBuffer);
        fs::path filePath = fs::path(FilenameBuffer);
        fs::path folderPath = filePath.parent_path();

        if (!fs::create_directories(gameDir / folderPath) && !fs::is_directory(gameDir / folderPath)) {
            LogError("can't create %s\n", (gameDir / folderPath).string().c_str());
            goto continueFileLoop;
        }
        do {
            if (folderPath.filename() == "Binaries") {
                if (!CompareFile(File, gameDir / filePath)) {
                    PreloadFile(Storage, File, threadCount, cancelFlag);
                    if (fs::status(gameDir / filePath).type() == fs::file_type::regular) {
                        fs::permissions(gameDir / filePath, fs::perms::_All_write, fs::perm_options::add); // copying over a file from the drive gives it the read-only attribute, this overrides that
                    }
                    if (!fs::copy_file(MountDir / filePath, gameDir / filePath, fs::copy_options::overwrite_existing)) {
                        LogError("failed to copy %s\n", filePath.string().c_str());
                    }
                }
                goto continueFileLoop;
            }
            folderPath = folderPath.parent_path();
        } while (folderPath != folderPath.root_path());

        if (!fs::is_symlink(gameDir / filePath)) {
            if (!symlinkCreationEnforced) {
                if (!enforceSymlinkCreation()) {
                    return false;
                }
                symlinkCreationEnforced = true;
            }
            fs::create_symlink(MountDir / filePath, gameDir / filePath);
        }
    continueFileLoop:
        progress();
    }
    return true;
}

bool MountedBuild::StartStorage(uint32_t storageFlags) {
    if (Storage) {
        return true;
    }
    char CloudDirHost[64];
    char CloudDirPath[64];
    ManifestGetCloudDir(Manifest, CloudDirHost, CloudDirPath);
    if (!StorageCreate(storageFlags, CacheDir.native().c_str(), CloudDirHost, CloudDirPath, &Storage)) {
        LogError("cannot create storage");
        return false;
    }
    return true;
}

bool MountedBuild::PreloadAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount) {
    std::shared_ptr<MANIFEST_CHUNK>* ChunkList;
    uint32_t ChunkCount;
    ManifestGetChunks(Manifest, &ChunkList, &ChunkCount);
    LogError("set chunk count to %u", ChunkCount);
    setMax(ChunkCount);

    iterable_queue<std::thread> threads;

    for (auto Chunk = ChunkList; Chunk != ChunkList + ChunkCount && !cancelFlag.cancelled(); Chunk++) {
        if (StorageChunkDownloaded(Storage, Chunk->get())) {
            //LogError("already downloaded", ChunkCount);
            progress();
            continue;
        }

        // cheap semaphore, keeps thread count low instead of having 81k threads pile up
        while (threads.size() >= threadCount) {
            threads.front().join();
            threads.pop();
        }

        threads.push(std::thread(StorageDownloadChunk, Storage, Chunk->get(), std::bind(progress)));
    }

    if (cancelFlag.cancelled()) {
        for (auto& thread : threads) {
            thread.detach();
            progress();
        }
    }
    else {
        for (auto& thread : threads) {
            thread.join();
            progress();
        }
    }
    return true;
}

#define HTONLL(x) ((1==htonl(1)) ? (x) : (((uint64_t)htonl((x) & 0xFFFFFFFFUL)) << 32) | htonl((uint32_t)((x) >> 32)))
#define NTOHLL(x) ((1==ntohl(1)) ? (x) : (((uint64_t)ntohl((x) & 0xFFFFFFFFUL)) << 32) | ntohl((uint32_t)((x) >> 32)))
auto guidHash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto guidEqual = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
void MountedBuild::PurgeUnusedChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag) {
    std::shared_ptr<MANIFEST_CHUNK>* ChunkList;
    uint32_t ChunkCount;
    ManifestGetChunks(Manifest, &ChunkList, &ChunkCount);

    std::unordered_set<char*, decltype(guidHash), decltype(guidEqual)> ManifestGuids;
    ManifestGuids.reserve(ChunkCount);
    for (auto Chunk = ChunkList; Chunk != ChunkList + ChunkCount; Chunk++) {
        ManifestGuids.insert(ManifestChunkGetGuid(Chunk->get()));
    }

    setMax(std::count_if(fs::recursive_directory_iterator(CacheDir), fs::recursive_directory_iterator(), [this](const fs::directory_entry& f) { return f.is_regular_file() && ValidChunkFile(CacheDir, f.path()) == 0; }));

    char guidBuffer[16];
    char guidBuffer2[16];
    auto iterator = fs::recursive_directory_iterator(CacheDir);
    for (auto& p : iterator) {
        if (cancelFlag.cancelled()) {
            break;
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
        progress();
    }
}

void MountedBuild::VerifyAllChunks(ProgressSetMaxHandler setMax, ProgressIncrHandler progress, cancel_flag& cancelFlag, uint32_t threadCount) {
    std::shared_ptr<MANIFEST_CHUNK>* ChunkList;
    uint32_t ChunkCount;
    ManifestGetChunks(Manifest, &ChunkList, &ChunkCount);
    setMax((std::min)(ChunkCount, (uint32_t)std::count_if(fs::recursive_directory_iterator(CacheDir), fs::recursive_directory_iterator(), [this](const fs::directory_entry& f) { return f.is_regular_file() && ValidChunkFile(CacheDir, f.path()) == 0; })));

    iterable_queue<std::thread> threads;

    for (auto Chunk = ChunkList; Chunk != ChunkList + ChunkCount && !cancelFlag.cancelled(); Chunk++) {
        if (!StorageChunkDownloaded(Storage, Chunk->get())) {
            continue;
        }

        // cheap semaphore, keeps thread count low instead of having 81k threads pile up
        while (threads.size() >= threadCount) {
            threads.front().join();
            progress();
            threads.pop();
        }

        threads.push(std::thread(StorageVerifyChunk, Storage, Chunk->get()));
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

void MountedBuild::LaunchGame(const char* additionalArgs) {
    char ExeBuf[MAX_PATH];
    char CmdBuf[512];
    ManifestGetLaunchInfo(Manifest, ExeBuf, CmdBuf);
    strcat(CmdBuf, " ");
    strcat(CmdBuf, additionalArgs);
    fs::path exePath = CacheDir / "game" / ExeBuf;

    PROCESS_INFORMATION pi;
    STARTUPINFOA si;

    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);

    CreateProcessA(exePath.string().c_str(), CmdBuf, NULL, NULL, FALSE, DETACHED_PROCESS, NULL, exePath.parent_path().string().c_str(), &si, &pi);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

bool MountedBuild::Mount() {
    if (Mounted()) {
        return true;
    }

    FspDebugLogSetHandle(GetStdHandle(STD_OUTPUT_HANDLE));

    {
        PVOID securityDescriptor;
        ULONG securityDescriptorSize;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(SDDL_MEMFS, SDDL_REVISION_1, &securityDescriptor, &securityDescriptorSize)) {
            fail(L"invalid sddl: %08x", FspNtStatusFromWin32(GetLastError()));
            return false;
        }

        auto downloadSize = ManifestDownloadSize(Manifest);
        auto installSize = ManifestInstallSize(Manifest);
        EGFS_PARAMS params;

        wcscpy_s(params.FileSystemName, L"EGFS");
        params.VolumePrefix[0] = 0;
        // VolumePrefix stays 0 for now, maybe change it later
        wcscpy_s(params.VolumeLabel, L"EGL2");
        params.VolumeTotal = installSize;
        params.VolumeFree = installSize - downloadSize;
        params.Security = securityDescriptor;
        params.SecuritySize = securityDescriptorSize;

        params.Callbacks.Read = std::bind(&MountedBuild::FileRead, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);

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
        Egfs = new EGFS(&params, Result);
        if (!NT_SUCCESS(Result)) {
            fail(L"could not create eglfs: %08x", Result);
            return false;
        }
        LocalFree(securityDescriptor);
    }

    {
        MANIFEST_FILE* Files;
        uint32_t FileCount;
        uint16_t FileStride;
        char FilenameBuffer[128];
        ManifestGetFiles(Manifest, &Files, &FileCount, &FileStride);
        for (int i = 0; i < FileCount * FileStride; i += FileStride) {
            auto File = (MANIFEST_FILE*)((char*)Files + i);
            ManifestFileGetName(File, FilenameBuffer);
            Egfs->AddFile(fs::path(FilenameBuffer), File, ManifestFileGetFileSize(File));
        }
    }

    {
        PVOID rootSecurity;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(SDDL_ROOT, SDDL_REVISION_1, &rootSecurity, NULL)) {
            fail(L"invalid root sddl: %08x", FspNtStatusFromWin32(GetLastError()));
            return false;
        }
        Egfs->SetMountPoint(MountDir.native().c_str(), rootSecurity);
        LocalFree(rootSecurity);
    }

    Egfs->Start();
    return true;
}

bool MountedBuild::Unmount() {
    if (!Mounted()) {
        return true;
    }

    delete Egfs;
    Egfs = nullptr;

    return true;
}

bool MountedBuild::Mounted() {
    return Egfs;
}

void MountedBuild::LogError(const char* format, ...)
{
    va_list argp;
    va_start(argp, format);
    char* buf = new char[snprintf(nullptr, 0, format, argp) + 1];
    vsprintf(buf, format, argp);
    va_end(argp);
    Error(buf);
    delete[] buf;
}

void MountedBuild::FileRead(PVOID Handle, PVOID Buffer, UINT64 offset, ULONG length, ULONG* bytesRead) {
    auto File = (MANIFEST_FILE*)Handle;
    uint32_t ChunkStartIndex, ChunkStartOffset;
    if (ManifestFileGetChunkIndex(File, offset, &ChunkStartIndex, &ChunkStartOffset)) {
        MANIFEST_CHUNK_PART* ChunkParts;
        uint32_t ChunkPartCount, BytesRead = 0;
        uint16_t StrideSize;
        ManifestFileGetChunks(File, &ChunkParts, &ChunkPartCount, &StrideSize);
        for (int i = ChunkStartIndex * StrideSize; i < ChunkPartCount * StrideSize; i += StrideSize) {
            auto chunkPart = (MANIFEST_CHUNK_PART*)((char*)ChunkParts + i);
            uint32_t ChunkOffset, ChunkSize;
            ManifestFileChunkGetData(chunkPart, &ChunkOffset, &ChunkSize);
            char* ChunkBuffer = new char[ChunkSize];
            StorageDownloadChunkPart(Storage, chunkPart, ChunkBuffer);
            if (((int64_t)length - (int64_t)BytesRead) > (int64_t)ChunkSize - (int64_t)ChunkStartOffset) { // copy the entire buffer over
                memcpy((char*)Buffer + BytesRead, ChunkBuffer + ChunkStartOffset, ChunkSize - ChunkStartOffset);
                BytesRead += ChunkSize - ChunkStartOffset;
            }
            else { // copy what it needs to fill up the rest
                memcpy((char*)Buffer + BytesRead, ChunkBuffer + ChunkStartOffset, length - BytesRead);
                BytesRead += (int64_t)length - (int64_t)BytesRead;
                delete[] ChunkBuffer;
                *bytesRead = BytesRead;
                return;
            }
            delete[] ChunkBuffer;
            ChunkStartOffset = 0;
        }
        *bytesRead = BytesRead;
    }
    else {
        *bytesRead = 0;
    }
}