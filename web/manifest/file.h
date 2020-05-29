#pragma once

#include "chunk_part.h"

#include <string>
#include <vector>

struct File {
	std::string FileName;
	char ShaHash[20];
	std::vector<ChunkPart> ChunkParts;

	uint64_t GetFileSize();

	bool GetChunkIndex(uint64_t Offset, uint32_t& ChunkIndex, uint32_t& ChunkOffset);
};