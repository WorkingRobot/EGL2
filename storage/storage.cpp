#include "storage.h"

#ifndef LOG_SECTION
#define LOG_SECTION "Storage"
#endif

#include "../Logger.h"
#include "../Stats.h"
#include "EGSProvider.h"
#include "sha.h"

#include <libdeflate.h>

Storage::Storage(uint32_t Flags, uint32_t ChunkPoolCapacity, fs::path CacheLocation, std::string CloudDir) :
    Flags(Flags),
    ChunkPoolCapacity(ChunkPoolCapacity),
    CachePath(CacheLocation),
    CloudDir(CloudDir),
    Compressor(Flags)
{ }

Storage::~Storage()
{

}

bool Storage::IsChunkDownloaded(std::shared_ptr<Chunk> Chunk)
{
    return fs::status(CachePath / Chunk->GetFilePath()).type() == fs::file_type::regular;
}

bool Storage::IsChunkDownloaded(ChunkPart& ChunkPart)
{
    return IsChunkDownloaded(ChunkPart.Chunk);
}

bool Storage::VerifyChunk(std::shared_ptr<Chunk> Chunk, cancel_flag& flag)
{
    Compressor::buffer_value chunkData;
    if (!ReadChunk(CachePath / Chunk->GetFilePath(), chunkData, flag)) {
        return false;
    }
    Stats::ProvideCount.fetch_add(chunkData.second, std::memory_order_relaxed);
    return VerifyHash(chunkData.first.get(), chunkData.second, Chunk->ShaHash);
}

void Storage::DeleteChunk(std::shared_ptr<Chunk> Chunk)
{
    fs::remove(CachePath / Chunk->GetFilePath());
}

std::shared_ptr<char[]> Storage::GetChunk(std::shared_ptr<Chunk> Chunk, cancel_flag& flag)
{
    auto& data = GetPoolData(Chunk);
    SAFE_FLAG_RETURN(nullptr);
    switch (data.Status)
    {
    case CHUNK_STATUS::Unavailable:
    redownloadChunk:
    {
        SAFE_FLAG_RETURN(nullptr);
        // download
        data.Status = CHUNK_STATUS::Grabbing;

        auto chunkData = DownloadChunk(Chunk, flag);
        if (chunkData.first)
        {
            data.Buffer = chunkData;
            data.Status = CHUNK_STATUS::Readable;
            data.CV.notify_all();
        }
        else {
            data.Status = CHUNK_STATUS::Unavailable;
        }
        break;
    }
    case CHUNK_STATUS::Available:
    {
        // read from file
        data.Status = CHUNK_STATUS::Reading;

        Compressor::buffer_value chunkData;
        if (!ReadChunk(CachePath / Chunk->GetFilePath(), chunkData, flag)) {
            if (flag.cancelled()) {
                data.Status = CHUNK_STATUS::Available;
                break;
            }
            DeleteChunk(Chunk);
            goto redownloadChunk;
        }
        if (Flags & StorageVerifyHashes) {
            if (!VerifyHash(chunkData.first.get(), chunkData.second, Chunk->ShaHash)) {
                if (flag.cancelled()) {
                    data.Status = CHUNK_STATUS::Available;
                    break;
                }
                DeleteChunk(Chunk);
                goto redownloadChunk;
            }
        }
        
        {
            data.Buffer = chunkData;
            data.Status = CHUNK_STATUS::Readable;
            data.CV.notify_all();
        }
        break;
    }
    case CHUNK_STATUS::Grabbing: // downloading from server, wait until mutex releases
    case CHUNK_STATUS::Reading:  // reading from file, wait until mutex releases
    {
        std::unique_lock<std::mutex> lk(data.CV_Mutex);
        data.CV.wait(lk, [&] { return data.Status == CHUNK_STATUS::Readable || flag.cancelled(); });
        SAFE_FLAG_RETURN(nullptr);
        break;
    }
    case CHUNK_STATUS::Readable: // available in memory pool
    {
        break;
    }
    default:
        // h o w
        break;
    }
    return data.Buffer.first;
}

std::shared_ptr<char[]> Storage::GetChunkPart(ChunkPart& ChunkPart, cancel_flag& flag)
{
    auto chunk = GetChunk(ChunkPart.Chunk, flag);
    if (ChunkPart.Offset == 0 && ChunkPart.Size == ChunkPart.Chunk->WindowSize) {
        return chunk;
    }
    else {
        auto partBuffer = std::shared_ptr<char[]>(new char[ChunkPart.Size]);
        memcpy(partBuffer.get(), chunk.get() + ChunkPart.Offset, ChunkPart.Size);
        return partBuffer;
    }
}

CHUNK_POOL_DATA& Storage::GetPoolData(std::shared_ptr<Chunk> Chunk)
{
    std::lock_guard<std::mutex> statusLock(ChunkPoolMutex);
    for (auto& chunk : ChunkPool) {
        if (!memcmp(Chunk->Guid, chunk.first, 16)) {
            return chunk.second;
        }
    }

    while (ChunkPool.size() >= ChunkPoolCapacity)
    {
        ChunkPool.pop_front();
    }

    auto& data = ChunkPool.emplace_back();
    data.first = Chunk->Guid;
    data.second.Status = GetUnpooledChunkStatus(Chunk);
    return data.second;
}

CHUNK_STATUS Storage::GetUnpooledChunkStatus(std::shared_ptr<Chunk> Chunk)
{
    return IsChunkDownloaded(Chunk) ? CHUNK_STATUS::Available : CHUNK_STATUS::Unavailable;
}

enum {
    ChunkFlagDecompressed = 0x01,
    ChunkFlagZstd = 0x02,
    ChunkFlagZlib = 0x04,
    ChunkFlagLZ4 = 0x08,
    ChunkFlagCompMask = 0xF
};
#pragma pack(push, 1)
struct CHUNK_HEADER {
    uint16_t version;
    uint16_t flags;
};

#define CHUNK_HEADER_MAGIC 0xB1FE3AA2
struct CDN_CHUNK_HEADER {
    uint32_t Magic;
    uint32_t Version;
    uint32_t HeaderSize;
    uint32_t DataSizeCompressed;
    char Guid[16];
    uint64_t RollingHash;
    uint8_t StoredAs; // EChunkStorageFlags
};

struct CDN_CHUNK_HEADER_V2 {
    char SHAHash[20];
    uint8_t HashType; // EChunkHashFlags
};

struct CDN_CHUNK_HEADER_V3 {
    uint32_t DataSizeUncompressed;
};
#pragma pack(pop)

Compressor::buffer_value Storage::DownloadChunk(std::shared_ptr<Chunk> Chunk, cancel_flag& flag, bool forceDownload)
{
    std::shared_ptr<char[]> data;

    if (!forceDownload && EGSProvider::Available() && EGSProvider::IsChunkAvailable(Chunk)) {
        LOG_DEBUG("GETTING EGL DATA");
        data = EGSProvider::GetChunk(Chunk);
    }
    if (!data) { // EGSProvider GetChunk could return nullptr
        std::vector<char> chunkData;
        {
            auto chunkConn = Client::CreateConnection();
            chunkConn->SetUrl(CloudDir + Chunk->GetUrl());
            if (!Client::Execute(chunkConn, flag)) {
                SAFE_FLAG_RETURN(std::make_pair(nullptr, 0));
                LOG_WARN("Retrying...");
                return DownloadChunk(Chunk, flag);
            }

            chunkData.reserve(chunkConn->GetResponseBody().size());
            Stats::DownloadCount.fetch_add(chunkConn->GetResponseBody().size(), std::memory_order_relaxed);
            std::copy(chunkConn->GetResponseBody().begin(), chunkConn->GetResponseBody().end(), std::back_inserter(chunkData));
        }

        size_t decompressedSize = 1024 * 1024;

        auto headerv1 = *(CDN_CHUNK_HEADER*)chunkData.data();
        auto chunkPos = sizeof(CDN_CHUNK_HEADER);
        if (headerv1.Magic != CHUNK_HEADER_MAGIC) {
            LOG_ERROR("Downloaded chunk (%s) magic invalid: %08X", Chunk->GetGuid(), headerv1.Magic);
            LOG_WARN("Retrying...");
            return DownloadChunk(Chunk, flag);
        }
        if (headerv1.Version >= 2) {
            auto headerv2 = *(CDN_CHUNK_HEADER_V2*)(chunkData.data() + chunkPos);
            chunkPos += sizeof(CDN_CHUNK_HEADER_V2);
            if (headerv1.Version >= 3) {
                auto headerv3 = *(CDN_CHUNK_HEADER_V3*)(chunkData.data() + chunkPos);
                decompressedSize = headerv3.DataSizeUncompressed;

                if (headerv1.Version > 3) { // version past 3
                    chunkPos = headerv1.HeaderSize;
                }
            }
        }

        if (headerv1.StoredAs & 0x02) // encrypted
        {
            LOG_ERROR("Downloaded chunk (%s) is encrypted", Chunk->GetGuid());
            //return; // no support yet, i have never seen this used in practice
        }

        auto bufferPtr = chunkData.data() + chunkPos;

        data = std::shared_ptr<char[]>(new char[decompressedSize]);

        SAFE_FLAG_RETURN(std::make_pair(nullptr, 0));
        if (headerv1.StoredAs & 0x01) // compressed
        {
            auto decompressor = libdeflate_alloc_decompressor(); // TODO: use ctxmanager for this
            auto result = libdeflate_zlib_decompress(decompressor, bufferPtr, headerv1.DataSizeCompressed, data.get(), decompressedSize, NULL);
            libdeflate_free_decompressor(decompressor);
        }
        else {
            memcpy(data.get(), bufferPtr, decompressedSize);
        }
    }
    SAFE_FLAG_RETURN(std::make_pair(data, Chunk->WindowSize));
    WriteChunk(CachePath / Chunk->GetFilePath(), Chunk->WindowSize, Compressor.StorageCompress(data, Chunk->WindowSize));
    return std::make_pair(data, Chunk->WindowSize);
}

bool Storage::ReadChunk(fs::path Path, Compressor::buffer_value& ReadBuffer, cancel_flag& flag)
{
    auto fp = fopen(Path.string().c_str(), "rb");
    CHUNK_HEADER header;
    fread(&header, sizeof(CHUNK_HEADER), 1, fp);
    if (header.version != 0) {
        LOG_ERROR("Bad chunk version for %s: %hu", Path.string().c_str(), header.version);
        return false;
    }
    if (flag.cancelled()) {
        fclose(fp);
        return false;
    }
    switch (header.flags & ChunkFlagCompMask)
    {
    case ChunkFlagDecompressed:
    {
        auto pos = ftell(fp);
        fseek(fp, 0, SEEK_END);
        auto inBufSize = ftell(fp) - pos;
        fseek(fp, pos, SEEK_SET);

        auto buffer = std::shared_ptr<char[]>(new char[inBufSize]);
        fread(buffer.get(), 1, inBufSize, fp);
        fclose(fp);

        ReadBuffer = std::make_pair(buffer, inBufSize);
        Stats::FileReadCount.fetch_add(inBufSize, std::memory_order_relaxed);
        return true;
    }
    case ChunkFlagZstd:
    {
        size_t inBufSize;
        ReadBuffer = Compressor.ZstdDecompress(fp, inBufSize);
        fclose(fp);
        Stats::FileReadCount.fetch_add(inBufSize, std::memory_order_relaxed);
        return true;
    }
    case ChunkFlagZlib:
    {
        size_t inBufSize;
        ReadBuffer = Compressor.ZlibDecompress(fp, inBufSize);
        fclose(fp);
        Stats::FileReadCount.fetch_add(inBufSize, std::memory_order_relaxed);
        return true;
    }
    case ChunkFlagLZ4:
    {
        size_t inBufSize;
        ReadBuffer = Compressor.LZ4Decompress(fp, inBufSize);
        fclose(fp);
        Stats::FileReadCount.fetch_add(inBufSize, std::memory_order_relaxed);
        return true;
    }
    default:
    {
        fclose(fp);
        LOG_ERROR("Unknown read flag for %s: %hu", Path.string().c_str(), header.flags);
        return false;
    }
    }
}

void Storage::WriteChunk(fs::path Path, uint32_t DecompressedSize, Compressor::buffer_value& Buffer)
{
    LOG_DEBUG("OPENING CHUNK FILE");
    auto fp = fopen(Path.string().c_str(), "wb");
    LOG_DEBUG("CREATING CHUNK HEADER");
    CHUNK_HEADER chunkHeader;
    chunkHeader.version = 0;
    switch (Flags & StorageCompMethodMask)
    {
    case StorageDecompressed:
        chunkHeader.flags = ChunkFlagDecompressed;
        break;
    case StorageZstd:
        chunkHeader.flags = ChunkFlagZstd;
        break;
    case StorageLZ4:
        chunkHeader.flags = ChunkFlagLZ4;
        break;
    }
    LOG_DEBUG("WRITING CHUNK HEADER");
    fwrite(&chunkHeader, sizeof(CHUNK_HEADER), 1, fp);
    if ((Flags & StorageCompMethodMask) != StorageDecompressed) { // Compressed chunks write the decompressed size
        LOG_DEBUG("WRITING DECOMPRESSED SIZE");
        fwrite(&DecompressedSize, sizeof(uint32_t), 1, fp);
    }
    LOG_DEBUG("WRITING CHUNK DATA");
    fwrite(Buffer.first.get(), 1, Buffer.second, fp);
    LOG_DEBUG("CLOSING CHUNK FILE");
    fclose(fp);

    Stats::FileWriteCount.fetch_add(Buffer.second, std::memory_order_relaxed);
}
