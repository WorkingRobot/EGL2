#include "chunk.h"

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>

std::string Chunk::GetGuid() {
	char GuidBuffer[33];
	sprintf(GuidBuffer, "%016llX%016llX", ntohll(*(uint64_t*)Guid), ntohll(*(uint64_t*)(Guid + 8)));
	return GuidBuffer;
}

std::string Chunk::GetFilePath() {
	char PathBuffer[53];
	sprintf(PathBuffer, "FF/%016llX%016llX", ntohll(*(uint64_t*)Guid), ntohll(*(uint64_t*)(Guid + 8)));
	memcpy(PathBuffer, PathBuffer + 3, 2);
	return PathBuffer;
}

std::string Chunk::GetUrl() {
	char UrlBuffer[59];
	sprintf(UrlBuffer, "%02d/%016llX_%016llX%016llX.chunk", Group, Hash, ntohll(*(uint64_t*)Guid), ntohll(*(uint64_t*)(Guid + 8)));
	return UrlBuffer;
}