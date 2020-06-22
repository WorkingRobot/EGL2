#include "compression.h"

#ifndef LOG_SECTION
#define LOG_SECTION "Compressor"
#endif

#include "../Logger.h"
#include "storage.h"

Compressor::Compressor(uint32_t storageFlags) :
	StorageFlags(storageFlags) {
	switch (StorageFlags & StorageCompMethodMask)
	{
	case StorageDecompressed:
	{
		CompressFunc = std::make_pair<std::shared_ptr<char[]>, size_t>;
		break;
	}
	case StorageZstd:
	{
		switch (StorageFlags & StorageCompLevelMask)
		{
		case StorageCompressFastest:
			CLevel = -3;
			break;
		case StorageCompressFast:
			CLevel = -1;
			break;
		case StorageCompressNormal:
			CLevel = 1;
			break;
		case StorageCompressSlow:
			CLevel = 3;
			break;
		case StorageCompressSlowest:
			CLevel = 5;
			break;
		}
		CCtx = std::make_unique<CtxManager<void*>>([]() { return ZSTD_createCCtx(); }, [](void* cctx) { ZSTD_freeCCtx((ZSTD_CCtx*)cctx); });

		CompressFunc = [this](std::shared_ptr<char[]>& buffer, size_t buffer_size) {
			auto outBuf = std::shared_ptr<char[]>(new char[ZSTD_COMPRESSBOUND(buffer_size)]);
			size_t outSize;
			{
				std::unique_lock<std::mutex> lock;
				auto& cctx = CCtx->GetCtx(lock);
				outSize = ZSTD_compressCCtx((ZSTD_CCtx*)cctx, outBuf.get(), ZSTD_COMPRESSBOUND(buffer_size), buffer.get(), buffer_size, CLevel);
			}
			return std::make_pair(outBuf, outSize);
		};
		break;
	}
	case StorageLZ4:
	{
		switch (StorageFlags & StorageCompLevelMask)
		{
		case StorageCompressFastest:
			CLevel = LZ4HC_CLEVEL_MIN;
			break;
		case StorageCompressFast:
			CLevel = 6;
			break;
		case StorageCompressNormal:
			CLevel = LZ4HC_CLEVEL_DEFAULT;
			break;
		case StorageCompressSlow:
			CLevel = LZ4HC_CLEVEL_OPT_MIN;
			break;
		case StorageCompressSlowest:
			CLevel = LZ4HC_CLEVEL_MAX;
			break;
		}
		
		CCtx = std::make_unique<CtxManager<void*>>([]() { return _aligned_malloc(LZ4_sizeofStateHC(), 8); }, &_aligned_free);

		CompressFunc = [this](std::shared_ptr<char[]>& buffer, size_t buffer_size) {
			LOG_DEBUG("CREATING COMPRESSED BUF");
			auto outBuf = std::shared_ptr<char[]>(new char[LZ4_COMPRESSBOUND(buffer_size)]);
			size_t outSize;
			{
				std::unique_lock<std::mutex> lock;
				LOG_DEBUG("GETTING LZ4 CCTX");
				auto& cctx = CCtx->GetCtx(lock);
				LOG_DEBUG("COMPRESSING LZ4 DATA");
				outSize = LZ4_compress_HC_extStateHC(cctx, buffer.get(), outBuf.get(), buffer_size, LZ4_COMPRESSBOUND(buffer_size), CLevel);
			}
			LOG_DEBUG("RETURNING OUT DATA");
			return std::make_pair(outBuf, outSize);
		};
		break;
	}
	}

	ZlibDCtx = std::make_unique<CtxManager<libdeflate_decompressor*>>(&libdeflate_alloc_decompressor, &libdeflate_free_decompressor);
	ZstdDCtx = std::make_unique<CtxManager<ZSTD_DCtx*>>([]() {return ZSTD_createDCtx(); }, [](ZSTD_DCtx* ctx) { ZSTD_freeDCtx(ctx); });
}

Compressor::~Compressor() {

}

Compressor::buffer_value Compressor::StorageCompress(std::shared_ptr<char[]> buffer, size_t buffer_size)
{
	return CompressFunc(buffer, buffer_size);
}

Compressor::buffer_value Compressor::ZlibDecompress(FILE* File, size_t& inBufSize)
{
	uint32_t uncompressedSize;
	fread(&uncompressedSize, sizeof(uint32_t), 1, File);

	auto pos = ftell(File);
	fseek(File, 0, SEEK_END);
	inBufSize = ftell(File) - pos;
	fseek(File, pos, SEEK_SET);

	auto inBuffer = std::make_unique<char[]>(inBufSize);
	fread(inBuffer.get(), 1, inBufSize, File);

	auto outBuffer = std::shared_ptr<char[]>(new char[uncompressedSize]);

	{
		std::unique_lock<std::mutex> lock;
		auto& dctx = ZlibDCtx->GetCtx(lock);
		libdeflate_zlib_decompress(dctx, inBuffer.get(), inBufSize, outBuffer.get(), uncompressedSize, NULL);
	}

	return std::make_pair(outBuffer, uncompressedSize);
}

Compressor::buffer_value Compressor::ZstdDecompress(FILE* File, size_t& inBufSize)
{
	uint32_t uncompressedSize;
	fread(&uncompressedSize, sizeof(uint32_t), 1, File);

	auto pos = ftell(File);
	fseek(File, 0, SEEK_END);
	inBufSize = ftell(File) - pos;
	fseek(File, pos, SEEK_SET);

	auto inBuffer = std::make_unique<char[]>(inBufSize);
	fread(inBuffer.get(), 1, inBufSize, File);

	auto outBuffer = std::shared_ptr<char[]>(new char[uncompressedSize]);

	{
		std::unique_lock<std::mutex> lock;
		auto& dctx = ZstdDCtx->GetCtx(lock);
		ZSTD_decompressDCtx(dctx, outBuffer.get(), uncompressedSize, inBuffer.get(), inBufSize);
	}

	return std::make_pair(outBuffer, uncompressedSize);
}

Compressor::buffer_value Compressor::LZ4Decompress(FILE* File, size_t& inBufSize)
{
	uint32_t uncompressedSize;
	fread(&uncompressedSize, sizeof(uint32_t), 1, File);

	auto pos = ftell(File);
	fseek(File, 0, SEEK_END);
	inBufSize = ftell(File) - pos;
	fseek(File, pos, SEEK_SET);

	auto inBuffer = std::make_unique<char[]>(inBufSize);
	fread(inBuffer.get(), 1, inBufSize, File);

	auto outBuffer = std::shared_ptr<char[]>(new char[uncompressedSize]);
	LZ4_decompress_safe(inBuffer.get(), outBuffer.get(), inBufSize, uncompressedSize);

	return std::make_pair(outBuffer, uncompressedSize);
}