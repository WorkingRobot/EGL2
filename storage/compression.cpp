#include "compression.h"

#include "storage.h"

#include <libdeflate.h>
#include <lz4hc.h>

bool ZlibDecompress(FILE* File, DECOMPRESS_ALLOCATOR Allocator) {
	uint32_t uncompressedSize;
	fread(&uncompressedSize, sizeof(uint32_t), 1, File);

	auto pos = ftell(File);
	fseek(File, 0, SEEK_END);
	auto inBufSize = ftell(File) - pos;
	fseek(File, pos, SEEK_SET);

	char* inBuffer = new char[inBufSize];
	fread(inBuffer, 1, inBufSize, File);

	auto decompressor = libdeflate_alloc_decompressor();

	auto& outBuffer = Allocator(uncompressedSize);
	libdeflate_zlib_decompress(decompressor, inBuffer, inBufSize, outBuffer.get(), uncompressedSize, NULL);

	libdeflate_free_decompressor(decompressor);
	delete[] inBuffer;
	return true;
}

bool LZ4Decompress(FILE* File, DECOMPRESS_ALLOCATOR Allocator) {
	uint32_t uncompressedSize;
	fread(&uncompressedSize, sizeof(uint32_t), 1, File);

	auto pos = ftell(File);
	fseek(File, 0, SEEK_END);
	auto inBufSize = ftell(File) - pos;
	fseek(File, pos, SEEK_SET);

	char* inBuffer = new char[inBufSize];
	fread(inBuffer, 1, inBufSize, File);

	auto& outBuffer = Allocator(uncompressedSize);
	LZ4_decompress_fast(inBuffer, outBuffer.get(), uncompressedSize);

	delete[] inBuffer;
	return true;
}

bool ZlibCompress(uint32_t Flags, const char* Buffer, uint32_t BufferSize, char** POutBuffer, uint32_t* POutBufferSize) {
	int compression_level;
	if (Flags & StorageCompressFastest) {
		compression_level = 1;
	}
	else if (Flags & StorageCompressFast) {
		compression_level = 4;
	}
	else if (Flags & StorageCompressNormal) {
		compression_level = 6;
	}
	else if (Flags & StorageCompressSlow) {
		compression_level = 9;
	}
	else if (Flags & StorageCompressSlowest) {
		compression_level = 12;
	}
	else {
		return false;
	}

	auto compressor = libdeflate_alloc_compressor(compression_level);

	uint32_t outBufSize = libdeflate_zlib_compress_bound(compressor, BufferSize);
	char* outBuffer = new char[outBufSize];

	uint32_t compressedSize = libdeflate_zlib_compress(compressor, Buffer, BufferSize, outBuffer, outBufSize);

	*POutBuffer = outBuffer;
	*POutBufferSize = compressedSize;

	libdeflate_free_compressor(compressor);
}

bool LZ4Compress(uint32_t Flags, const char* Buffer, uint32_t BufferSize, char** POutBuffer, uint32_t* POutBufferSize) {
	int compression_level;
	if (Flags & StorageCompressFastest) {
		compression_level = LZ4HC_CLEVEL_MIN;
	}
	else if (Flags & StorageCompressFast) {
		compression_level = 6;
	}
	else if (Flags & StorageCompressNormal) {
		compression_level = LZ4HC_CLEVEL_DEFAULT;
	}
	else if (Flags & StorageCompressSlow) {
		compression_level = LZ4HC_CLEVEL_OPT_MIN; // uses "HC" at this point
	}
	else if (Flags & StorageCompressSlowest) {
		compression_level = LZ4HC_CLEVEL_MAX;
	}
	else {
		return false;
	}

	uint32_t outBufSize = LZ4_COMPRESSBOUND(BufferSize);
	char* outBuffer = new char[outBufSize];
	
	uint32_t compressedSize = LZ4_compress_HC(Buffer, outBuffer, BufferSize, outBufSize, compression_level);

	*POutBuffer = outBuffer;
	*POutBufferSize = compressedSize;
}

void DeleteCompressBuffer(char* OutBuffer) {
	delete[] OutBuffer;
}