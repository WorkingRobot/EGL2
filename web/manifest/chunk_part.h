#pragma once

#include "chunk.h"

#include <memory>

struct ChunkPart {
	std::shared_ptr<Chunk> Chunk;
	uint32_t Offset;
	uint32_t Size;
};