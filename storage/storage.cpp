#include "storage.h"
#include "../web/http.h"
#include "../containers/iterable_queue.h"
#include "compression.h"
#include "sha.h"

#include <filesystem>
#include <queue>
#include <libdeflate.h>

namespace fs = std::filesystem;

#define STORAGE_CHUNKS_RESERVE 64

enum class CHUNK_STATUS {
    Unavailable, // Readable from download
    Grabbing,    // Downloading
    Available,   // Readable from local copy
    Reading,     // Reading from local copy
    Readable     // Readable from memory
};

typedef struct _CHUNK_POOL_DATA {
    std::unique_ptr<char[]> Buffer;
    std::condition_variable CV;
    std::mutex Mutex;
    CHUNK_STATUS Status;
} CHUNK_POOL_DATA;

auto hash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
auto equal = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
// this can be an ordered map, but i'm unsure about the memory usage of this, even if all 81k chunks are read
typedef iterable_queue<std::pair<char*, std::shared_ptr<CHUNK_POOL_DATA>>> STORAGE_CHUNK_POOL_LOOKUP;

typedef struct _STORAGE {
    fs::path cachePath;
    uint32_t flags;
    std::string CloudDir; // CloudDir also includes the /ChunksV3/ part, though
    std::unique_ptr<httplib::Client> Client;
    std::unique_ptr<std::mutex> ChunkPoolMutex;
    STORAGE_CHUNK_POOL_LOOKUP ChunkPool;
} STORAGE;

bool StorageCreate(
    uint32_t Flags,
    const wchar_t* CacheLocation,
    const char* ChunkHost,
    const char* CloudDir,
    STORAGE** PStorage) {
    STORAGE* Storage = new STORAGE;
    Storage->cachePath = fs::path(CacheLocation);
    Storage->flags = Flags;
    Storage->CloudDir = CloudDir;
    Storage->Client = std::make_unique<httplib::Client>(ChunkHost);
    Storage->ChunkPoolMutex = std::make_unique<std::mutex>();
    *PStorage = Storage;
    return true;
}

void StorageDelete(STORAGE* Storage) {
    // delete buffers and stuff in pool lookup table
    delete Storage;
}

inline CHUNK_STATUS StorageGetChunkStatus(STORAGE* Storage, char Guid[16]) {
    char GuidString[33];
    sprintf(GuidString, "%016llX%016llX", ntohll(*(uint64_t*)Guid), ntohll(*(uint64_t*)(Guid + 8)));

    char GuidFolder[3];
    memcpy(GuidFolder, GuidString, 2);
    GuidFolder[2] = '\0';

    if (fs::status(Storage->cachePath / GuidFolder / GuidString).type() != fs::file_type::regular) {
        return CHUNK_STATUS::Unavailable;
    }
    else {
        return CHUNK_STATUS::Available;
    }
}

std::weak_ptr<CHUNK_POOL_DATA> StorageGetPoolData(STORAGE* Storage, char Guid[16]) {
    std::lock_guard<std::mutex> statusLock(*Storage->ChunkPoolMutex);
    for (auto& chunk : Storage->ChunkPool) {
        if (!memcmp(Guid, chunk.first, 16)) {
            return chunk.second;
        }
    }

    if (Storage->ChunkPool.size() == STORAGE_CHUNKS_RESERVE)
    {
        Storage->ChunkPool.pop();
    }

    auto data = std::make_shared<CHUNK_POOL_DATA>();

    data->Buffer = std::unique_ptr<char[]>();
    data->Status = StorageGetChunkStatus(Storage, Guid);

    Storage->ChunkPool.push(std::make_pair(Guid, data));
    return data;
}

std::unique_ptr<char[]>& StorageGetBuffer(STORAGE* Storage, char Guid[16]) {
    return StorageGetPoolData(Storage, Guid).lock()->Buffer;
}

void StorageSetBuffer(std::shared_ptr<CHUNK_POOL_DATA> data, uint32_t size) {
    data->Buffer.reset(new char[size]);
}

#pragma pack(push, 1)
typedef struct _STORAGE_CHUNK_HEADER {
    uint16_t version;
    uint16_t flags;
} STORAGE_CHUNK_HEADER;

#define CHUNK_HEADER_MAGIC 0xB1FE3AA2
typedef struct _STORAGE_CDN_CHUNK_HEADER {
    uint32_t Magic;
    uint32_t Version;
    uint32_t HeaderSize;
    uint32_t DataSizeCompressed;
    char Guid[16];
    uint64_t RollingHash;
    uint8_t StoredAs; // EChunkStorageFlags
} STORAGE_CDN_CHUNK_HEADER;

typedef struct _STORAGE_CDN_CHUNK_HEADER_V2 {
    char SHAHash[20];
    uint8_t HashType; // EChunkHashFlags
} STORAGE_CDN_CHUNK_HEADER_V2;

typedef struct _STORAGE_CDN_CHUNK_HEADER_V3 {
    uint32_t DataSizeUncompressed;
} STORAGE_CDN_CHUNK_HEADER_V3;
#pragma pack(pop)

inline bool StorageRead(fs::path path, std::function<std::unique_ptr<char[]>&(uint32_t size)> allocator) {
    auto fp = fopen(path.string().c_str(), "rb");
    STORAGE_CHUNK_HEADER header;
    fread(&header, sizeof(STORAGE_CHUNK_HEADER), 1, fp);
    if (header.version != 0) {
        printf("bad version!\n");
        return false;
    }
    if (header.flags & StorageDecompressed) {
        auto pos = ftell(fp);
        fseek(fp, 0, SEEK_END);
        auto inBufSize = ftell(fp) - pos;
        fseek(fp, pos, SEEK_SET);

        auto& buffer = allocator(inBufSize);
        fread(buffer.get(), 1, inBufSize, fp);
        fclose(fp);
        return true;
    }
    else if (header.flags & StorageCompressZlib) { // zlib compressed
        auto result = ZlibDecompress(fp, allocator);

        fclose(fp);
        return result;
    }
    else if (header.flags & StorageCompressLZ4) { // lz4 compressed
        auto result = LZ4Decompress(fp, allocator);
        
        fclose(fp);
        return result;
    }
    fclose(fp);
    printf("unknown read flag!\n");
    return false;
}

inline void StorageWrite(const char* Path, uint16_t Flags, uint32_t DecompressedSize, const char* Buffer, uint32_t BufferSize) {
    auto fp = fopen(Path, "wb");
    if (!fp)
        printf("ERRNO %s: %d\n", Path, errno);
    STORAGE_CHUNK_HEADER chunkHeader;
    chunkHeader.version = 0;
    chunkHeader.flags = Flags;
    fwrite(&chunkHeader, sizeof(STORAGE_CHUNK_HEADER), 1, fp);
    if (!(Flags & StorageDecompressed)) { // Compressed chunks write the decompressed size
        fwrite(&DecompressedSize, sizeof(uint32_t), 1, fp);
    }
    fwrite(Buffer, 1, BufferSize, fp);
    fclose(fp);
}

bool StorageChunkDownloaded(STORAGE* Storage, MANIFEST_CHUNK* Chunk) {
    return StorageGetChunkStatus(Storage, ManifestChunkGetGuid(Chunk)) != CHUNK_STATUS::Unavailable;
}

bool StorageVerifyChunk(STORAGE* Storage, MANIFEST_CHUNK* Chunk) {
    if (StorageChunkDownloaded(Storage, Chunk)) {
        auto guid = ManifestChunkGetGuid(Chunk);

        char GuidString[33];
        sprintf(GuidString, "%016llX%016llX", ntohll(*(uint64_t*)guid), ntohll(*(uint64_t*)(guid + 8)));

        char GuidFolder[3];
        memcpy(GuidFolder, GuidString, 2);
        GuidFolder[2] = '\0';

        std::unique_ptr<char[]> _buf;
        uint32_t _bufSize;
        StorageRead(Storage->cachePath / GuidFolder / GuidString, [&](uint32_t size) -> std::unique_ptr<char[]>& {
            _buf.reset(new char[size]);
            _bufSize = size;
            return _buf;
        });

        if (!VerifyHash(_buf.get(), _bufSize, ManifestChunkGetSha1(Chunk))) {
            fs::remove(Storage->cachePath / GuidFolder / GuidString);
            StorageDownloadChunk(Storage, Chunk, [](const char* buf, uint32_t bufSize) {});
        }
        return true;
    }
    else {
        return false;
    }
}

void StorageDownloadChunk(STORAGE* Storage, MANIFEST_CHUNK* Chunk, std::function<void(const char* Buffer, uint32_t BufferSize)> DataCallback) {
    std::vector<char> chunkData;
    {
        chunkData.reserve(8192);
        bool chunkDataReserved = false;

        char UrlBuffer[256];
        strcpy(UrlBuffer, Storage->CloudDir.c_str());
        ManifestChunkAppendUrl(Chunk, UrlBuffer);
        Storage->Client->Get(UrlBuffer,
            [&](const char* data, uint64_t data_length) {
                chunkData.insert(chunkData.end(), data, data + data_length);
                return true;
            },
            [&](uint64_t len, uint64_t total) {
                if (!chunkDataReserved) {
                    chunkData.reserve(total);
                    chunkDataReserved = true;
                }
                return true;
            }
        );
    }

    uint32_t decompressedSize = 1024 * 1024;

    auto headerv1 = *(STORAGE_CDN_CHUNK_HEADER*)chunkData.data();
    auto chunkPos = sizeof(STORAGE_CDN_CHUNK_HEADER);
    if (headerv1.Magic != CHUNK_HEADER_MAGIC) {
        printf("magic invalid\n");
        return;
    }
    if (headerv1.Version >= 2) {
        auto headerv2 = *(STORAGE_CDN_CHUNK_HEADER_V2*)(chunkData.data() + chunkPos);
        chunkPos += sizeof(STORAGE_CDN_CHUNK_HEADER_V2);
        if (headerv1.Version >= 3) {
            auto headerv3 = *(STORAGE_CDN_CHUNK_HEADER_V3*)(chunkData.data() + chunkPos);
            decompressedSize = headerv3.DataSizeUncompressed;

            if (headerv1.Version > 3) { // version past 3
                chunkPos = headerv1.HeaderSize;
            }
        }
    }

    if (headerv1.StoredAs & 0x02) // encrypted
    {
        printf("encrypted?\n");
        return; // no support yet, i have never seen this used in practice
    }

    char GuidString[33];
    {
        auto guid = ManifestChunkGetGuid(Chunk);
        sprintf(GuidString, "%016llX%016llX", ntohll(*(uint64_t*)guid), ntohll(*(uint64_t*)(guid + 8)));
    }

    char GuidFolder[3];
    memcpy(GuidFolder, GuidString, 2);
    GuidFolder[2] = '\0';

    auto guidPathStr = (Storage->cachePath / GuidFolder / GuidString).string();
    auto guidPath = guidPathStr.c_str();
    auto bufferPtr = chunkData.data() + chunkPos;

    if (headerv1.StoredAs & 0x01) // compressed
    {
        auto data = std::make_unique<char[]>(decompressedSize);
        {
            auto decompressor = libdeflate_alloc_decompressor();
            auto result = libdeflate_zlib_decompress(decompressor, bufferPtr, headerv1.DataSizeCompressed, data.get(), decompressedSize, NULL);
            DataCallback(data.get(), decompressedSize);
            libdeflate_free_decompressor(decompressor);
        }

        if (Storage->flags & StorageCompressed) {
            StorageWrite(guidPath, StorageCompressZlib, decompressedSize, bufferPtr, headerv1.DataSizeCompressed);
        }
        else if (Storage->flags & StorageDecompressed) {
            StorageWrite(guidPath, StorageDecompressed, 0, data.get(), decompressedSize);
        }
        else if (Storage->flags & StorageCompressZlib) {
            char* compressedBuffer;
            uint32_t compressedBufferSize;
            ZlibCompress(Storage->flags, data.get(), decompressedSize, &compressedBuffer, &compressedBufferSize);
            StorageWrite(guidPath, StorageCompressZlib, decompressedSize, compressedBuffer, compressedBufferSize);
            delete[] compressedBuffer;
        }
        else if (Storage->flags & StorageCompressLZ4) {
            char* compressedBuffer;
            uint32_t compressedBufferSize;
            LZ4Compress(Storage->flags, data.get(), decompressedSize, &compressedBuffer, &compressedBufferSize);
            StorageWrite(guidPath, StorageCompressLZ4, decompressedSize, compressedBuffer, compressedBufferSize);
            delete[] compressedBuffer;
        }
    }
    else {
        DataCallback(bufferPtr, decompressedSize);

        if (Storage->flags & (StorageCompressed | StorageDecompressed)) {
            StorageWrite(guidPath, StorageDecompressed, 0, bufferPtr, decompressedSize);
        }
        else if (Storage->flags & StorageCompressZlib) {
            char* compressedBuffer;
            uint32_t compressedBufferSize;
            ZlibCompress(Storage->flags, bufferPtr, decompressedSize, &compressedBuffer, &compressedBufferSize);
            StorageWrite(guidPath, StorageCompressZlib, decompressedSize, compressedBuffer, compressedBufferSize);
            delete[] compressedBuffer;
        }
        else if (Storage->flags & StorageCompressLZ4) {
            char* compressedBuffer;
            uint32_t compressedBufferSize;
            LZ4Compress(Storage->flags, bufferPtr, decompressedSize, &compressedBuffer, &compressedBufferSize);
            StorageWrite(guidPath, StorageCompressLZ4, decompressedSize, compressedBuffer, compressedBufferSize);
            delete[] compressedBuffer;
        }
    }
}

// thread safe, downloads if needed, etc.
void StorageDownloadChunkPart(STORAGE* Storage, MANIFEST_CHUNK* Chunk, uint32_t Offset, uint32_t Size, char* Buffer) {
    auto guid = ManifestChunkGetGuid(Chunk);
    auto data = StorageGetPoolData(Storage, guid).lock();
    switch (data->Status)
    {
    case CHUNK_STATUS::Unavailable:
redownloadChunk:
    {
        // download
        data->Status = CHUNK_STATUS::Grabbing;
        
        StorageDownloadChunk(Storage, Chunk, [&](const char* Buffer_, uint32_t BufferSize) {
            std::unique_lock<std::mutex> lck(data->Mutex);
            StorageSetBuffer(data, BufferSize);
            memcpy(data->Buffer.get(), Buffer_, BufferSize);

            memcpy(Buffer, Buffer_ + Offset, Size);

            data->Status = CHUNK_STATUS::Readable;
            data->CV.notify_all();
        });
        break;
    }
    case CHUNK_STATUS::Available:
    {
        // read from file
        data->Status = CHUNK_STATUS::Reading;

        char GuidString[33];
        sprintf(GuidString, "%016llX%016llX", ntohll(*(uint64_t*)guid), ntohll(*(uint64_t*)(guid + 8)));

        char GuidFolder[3];
        memcpy(GuidFolder, GuidString, 2);
        GuidFolder[2] = '\0';

        char* _buf;
        uint32_t _bufSize;
        StorageRead(Storage->cachePath / GuidFolder / GuidString, [&](uint32_t size) -> std::unique_ptr<char[]>& {
            auto data = StorageGetPoolData(Storage, guid).lock();
            StorageSetBuffer(data, size);
            _buf = data->Buffer.get();
            _bufSize = size;
            return data->Buffer;
        });
        
        if (Storage->flags & StorageVerifyHashes) {
            if (!VerifyHash(_buf, _bufSize, ManifestChunkGetSha1(Chunk))) {
                fs::remove(Storage->cachePath / GuidFolder / GuidString);
                data->Status = CHUNK_STATUS::Unavailable;
                StorageDownloadChunkPart(Storage, Chunk, Offset, Size, Buffer);
                return;
            }
        }

        std::unique_lock<std::mutex> lck(data->Mutex);
        data->Status = CHUNK_STATUS::Readable;
        data->CV.notify_all();

        memcpy(Buffer, StorageGetBuffer(Storage, guid).get() + Offset, Size);
        break;
    }
    case CHUNK_STATUS::Grabbing: // downloading from server, wait until mutex releases
    case CHUNK_STATUS::Reading: // reading from file, wait until mutex releases
    {
        std::unique_lock<std::mutex> lck(data->Mutex);
        while (data->Status != CHUNK_STATUS::Readable) data->CV.wait(lck);

        memcpy(Buffer, StorageGetBuffer(Storage, guid).get() + Offset, Size);
        break;
    }
    case CHUNK_STATUS::Readable: // available in memory pool
    {
        memcpy(Buffer, StorageGetBuffer(Storage, guid).get() + Offset, Size);
        break;
    }
    default:
        // h o w
        break;
    }
}

void StorageDownloadChunkPart(STORAGE* Storage, MANIFEST_CHUNK_PART* ChunkPart, char* Buffer) {
    uint32_t Offset, Size;
    ManifestFileChunkGetData(ChunkPart, &Offset, &Size);
    StorageDownloadChunkPart(Storage, ManifestFileChunkGetChunk(ChunkPart), Offset, Size, Buffer);
}