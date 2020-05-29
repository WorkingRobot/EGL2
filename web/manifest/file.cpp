#include "file.h"

#include <numeric>

uint64_t File::GetFileSize() {
	return std::accumulate(ChunkParts.begin(), ChunkParts.end(), 0ull,
		[](uint64_t sum, const ChunkPart& curr) {
			return sum + curr.Size;
		});
}

bool File::GetChunkIndex(uint64_t Offset, uint32_t& ChunkIndex, uint32_t& ChunkOffset)
{
	for (ChunkIndex = 0; ChunkIndex < ChunkParts.size(); ++ChunkIndex) {
		if (Offset < ChunkParts[ChunkIndex].Size) {
			ChunkOffset = Offset;
			return true;
		}
		Offset -= ChunkParts[ChunkIndex].Size;
	}
	return false;
}
