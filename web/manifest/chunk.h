#pragma once

#include <string>

struct Chunk {
	char Guid[16];
	uint64_t Hash;
	char ShaHash[20];
	uint8_t Group;
	uint32_t WindowSize; // amount of data the chunk provides
	uint64_t FileSize; // total chunk file size

	std::string GetGuid();
	std::string GetFilePath();
	std::string GetUrl();
};