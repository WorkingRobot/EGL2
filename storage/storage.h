#pragma once

#include "../containers/cancel_flag.h"
#include "../web/http.h"
#include "../web/manifest/manifest.h"
#include "compression.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <functional>
#include <filesystem>
namespace fs = std::filesystem;

enum
{
    StorageDecompressed         = 0x00000001, // Chunks decompressed to solid blocks
    StorageZstd                 = 0x00000002, // Chunks are recompressed with Zlib
    StorageLZ4                  = 0x00000003, // Chunks are recompressed with LZ4
    StorageCompMethodMask       = 0x0000000F, // Chunks are recompressed with LZ4

    StorageCompressFastest      = 0x00000010, // Zlib = 1
    StorageCompressFast         = 0x00000020, // Zlib = 4
    StorageCompressNormal       = 0x00000030, // Zlib = 6
    StorageCompressSlow         = 0x00000040, // Zlib = 9
    StorageCompressSlowest      = 0x00000050, // Zlib = 12
    StorageCompLevelMask        = 0x000000F0, // Chunks are recompressed with LZ4

    StorageVerifyHashes         = 0x00001000, // Verify SHA hashes of downloaded chunks when reading and redownload if invalid
};

enum class CHUNK_STATUS {
    Unavailable, // Readable from download
    Grabbing,    // Downloading
    Available,   // Readable from local copy
    Reading,     // Reading from local copy
    Readable     // Readable from memory
};

struct CHUNK_POOL_DATA {
    std::shared_ptr<char[]> Buffer;
    uint32_t BufferSize;
    std::condition_variable CV;
    std::mutex Mutex;
    std::atomic<CHUNK_STATUS> Status;
};

auto hash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto equal = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
// this can be an ordered map, but i'm unsure about the memory usage of this, even if all 81k chunks are read
typedef std::deque<std::pair<char*, CHUNK_POOL_DATA>> STORAGE_CHUNK_POOL_LOOKUP;

class Storage {
public:
    Storage(uint32_t Flags, uint32_t ChunkPoolCapacity, fs::path CacheLocation, std::string CloudDir);
    ~Storage();

    bool IsChunkDownloaded(std::shared_ptr<Chunk> Chunk);
    bool IsChunkDownloaded(ChunkPart& ChunkPart);
    bool VerifyChunk(std::shared_ptr<Chunk> Chunk, cancel_flag& flag);
    void DeleteChunk(std::shared_ptr<Chunk> Chunk);
    std::shared_ptr<char[]> GetChunk(std::shared_ptr<Chunk> Chunk, cancel_flag& flag);
    std::shared_ptr<char[]> GetChunkPart(ChunkPart& ChunkPart, cancel_flag& flag);
    Compressor::buffer_value DownloadChunk(std::shared_ptr<Chunk> Chunk, cancel_flag& flag, bool forceDownload = false);

private:
    CHUNK_POOL_DATA& GetPoolData(std::shared_ptr<Chunk> Chunk);
    CHUNK_STATUS GetUnpooledChunkStatus(std::shared_ptr<Chunk> Chunk);
    bool ReadChunk(fs::path Path, Compressor::buffer_value& ReadBuffer, cancel_flag& flag);
    void WriteChunk(fs::path Path, uint32_t DecompressedSize, Compressor::buffer_value& Buffer);

    fs::path CachePath;
    uint32_t Flags;
    std::string CloudDir; // CloudDir also includes the /ChunksV3/ part, though
    Compressor Compressor;
    std::mutex ChunkPoolMutex;
    STORAGE_CHUNK_POOL_LOOKUP ChunkPool;
    uint32_t ChunkPoolCapacity;
};