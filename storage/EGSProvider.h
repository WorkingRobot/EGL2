#pragma once

#include "../web/manifest/manifest.h"

#include <filesystem>

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
};