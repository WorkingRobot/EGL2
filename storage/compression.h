#pragma once

#include <functional>
#include <memory>

typedef std::function<std::unique_ptr<char[]>& (uint32_t size)> DECOMPRESS_ALLOCATOR;

bool ZlibDecompress(FILE* File, DECOMPRESS_ALLOCATOR Allocator);

bool LZ4Decompress(FILE* File, DECOMPRESS_ALLOCATOR Allocator);

bool ZlibCompress(uint32_t Flags, const char* Buffer, uint32_t BufferSize, char** OutBuffer, uint32_t* POutBufferSize);

bool LZ4Compress(uint32_t Flags, const char* Buffer, uint32_t BufferSize, char** OutBuffer, uint32_t* POutBufferSize);
