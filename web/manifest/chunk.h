#pragma once

#include <string>

struct Chunk {
	char Guid[16];
	uint64_t Hash;
	char ShaHash[20];
	uint8_t Group;
	uint64_t Size;

	std::string GetGuid();
	std::string GetFilePath();
	std::string GetUrl();
};