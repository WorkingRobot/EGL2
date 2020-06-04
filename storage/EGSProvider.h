#pragma once

#include "../web/manifest/manifest.h"

#include <filesystem>
#include <unordered_map>

namespace fs = std::filesystem;

class EGSProvider {
public:
	inline static bool Available() {
		return GetInstance().available();
	}

	inline static bool IsChunkAvailable(std::shared_ptr<Chunk>& Chunk) {
		return GetInstance().isChunkAvailable(Chunk);
	}

	inline static std::shared_ptr<char[]> GetChunk(std::shared_ptr<Chunk>& Chunk) {
		return GetInstance().getChunk(Chunk);
	}

private:
	inline static EGSProvider& GetInstance() {
		static EGSProvider instance;
		return instance;
	}

	EGSProvider(EGSProvider const&) = delete;
	EGSProvider& operator=(EGSProvider const&) = delete;
	
	EGSProvider();
	~EGSProvider();

	bool available();

	bool isChunkAvailable(std::shared_ptr<Chunk>& Chunk);

	std::shared_ptr<char[]> getChunk(std::shared_ptr<Chunk>& Chunk);

	fs::path InstallDir;
	std::unique_ptr<Manifest> Build;

	static constexpr auto guidHash = [](const char* n) { return (*((uint64_t*)n)) ^ (*(((uint64_t*)n) + 1)); };
	static constexpr auto guidEqual = [](const char* a, const char* b) {return !memcmp(a, b, 16); };
	typedef std::unordered_map<char*, uint32_t, decltype(guidHash), decltype(guidEqual)> MANIFEST_CHUNK_LOOKUP;
	MANIFEST_CHUNK_LOOKUP ChunkLookup;
};