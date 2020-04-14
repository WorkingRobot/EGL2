#pragma once

#include "../web/manifest.h"

#include <functional>

typedef struct _STORAGE STORAGE;

enum
{
    StorageDecompressed         = 0x00000001, // Chunks decompressed to solid blocks
    StorageCompressed           = 0x00000002, // Chunks stay compressed in their downloaded form (this flag isn't used in chunk cache files)

    StorageCompressZlib         = 0x00000004, // Chunks are recompressed with Zlib
    StorageCompressLZ4          = 0x00000008, // Chunks are recompressed with LZ4

    StorageCompressFastest      = 0x00000010, // Zlib = 1
    StorageCompressFast         = 0x00000020, // Zlib = 4
    StorageCompressNormal       = 0x00000040, // Zlib = 6
    StorageCompressSlow         = 0x00000080, // Zlib = 9
    StorageCompressSlowest      = 0x00000100, // Zlib = 12

    StorageVerifyHashes         = 0x00001000, // Verify SHA hashes of downloaded chunks when reading and redownload if invalid
};

bool StorageCreate(
    uint32_t Flags,
    const wchar_t* CacheLocation,
    const char* ChunkHost,
    const char* CloudDir,
    STORAGE** PStorage);
void StorageDelete(STORAGE* Storage);

bool StorageChunkDownloaded(STORAGE* Storage, MANIFEST_CHUNK* Chunk);
bool StorageVerifyChunk(STORAGE* Storage, MANIFEST_CHUNK* Chunk);
void StorageDownloadChunk(STORAGE* Storage, MANIFEST_CHUNK* Chunk, std::function<void(const char* Buffer, uint32_t BufferSize)> DataCallback);
void StorageDownloadChunkPart(STORAGE* Storage, MANIFEST_CHUNK* Chunk, uint32_t Offset, uint32_t Size, char* Buffer);
void StorageDownloadChunkPart(STORAGE* Storage, MANIFEST_CHUNK_PART* ChunkPart, char* Buffer);
