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
    StorageDecompressed         = 0x00000001, // Chunks saved as raw blocks
    StorageZstd                 = 0x00000002, // Chunks are recompressed with Zlib
    StorageLZ4                  = 0x00000003, // Chunks are recompressed with LZ4
    StorageSelkie               = 0x00000004, // Chunks are recompressed with Oodle Selkie
    StorageCompMethodMask       = 0x0000000F, // Compresssion method mask

    StorageCompressFastest      = 0x00000010,
    StorageCompressFast         = 0x00000020,
    StorageCompressNormal       = 0x00000030,
    StorageCompressSlow         = 0x00000040,
    StorageCompressSlowest      = 0x00000050,
    StorageCompLevelMask        = 0x000000F0, // Compression level mask

    StorageVerifyHashes         = 0x00001000, // Verify SHA hashes of downloaded chunks when reading and redownload if invalid
};

enum {
    ChunkFlagDecompressed = 0x01,
    ChunkFlagZstd         = 0x02,
    ChunkFlagZlib         = 0x04,
    ChunkFlagLZ4          = 0x08,
    ChunkFlagOodle        = 0x09,

    ChunkFlagCompCount    =    5,
    ChunkFlagCompMask     = 0x0F,
};

enum class CHUNK_STATUS {
    Unavailable, // Readable from download
    Grabbing,    // Downloading
    Available,   // Readable from local copy
    Reading,     // Reading from local copy
    Readable     // Readable from memory
};

struct CHUNK_POOL_DATA {
    std::pair<std::shared_ptr<char[]>, size_t> Buffer;
    std::condition_variable CV;
    std::mutex CV_Mutex;
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
    bool GetChunkMetadata(std::shared_ptr<Chunk> Chunk, uint16_t& flags, size_t& fileSize);

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