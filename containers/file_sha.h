#pragma once

#include <fstream>
#include <filesystem>
#include <openssl/sha.h>

namespace fs = std::filesystem;

inline const bool SHAFile(fs::path path, char OutHash[SHA_DIGEST_LENGTH]) {
    static constexpr int buffer_size = 1 << 14; // 16384
    char buffer[buffer_size];
    
    SHA_CTX ctx;
    SHA1_Init(&ctx);

    std::ifstream fp(path.c_str(), std::ios::in | std::ios::binary);

    if (!fp.good()) {
        return false;
    }

    while (fp.good()) {
        fp.read(buffer, buffer_size);
        SHA1_Update(&ctx, buffer, fp.gcount());
    }
    fp.close();

    SHA1_Final((unsigned char*)OutHash, &ctx);
    return true;
}