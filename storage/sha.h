#pragma once

#include <cstdint>
#include <openssl/sha.h>

bool VerifyHash(const char* input, uint32_t inputSize, const char Sha[20]) {
	char calculatedHash[20];
	SHA1((const uint8_t*)input, inputSize, (uint8_t*)calculatedHash);

	return !memcmp(Sha, calculatedHash, 20);
}