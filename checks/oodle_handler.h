#pragma once

// https://encode.su/threads/2577-Open-source-Kraken-Mermaid-Selkie-LZNA-BitKnit-decompression/page2#post_49929
// Quite an interesting thread, too. Thanks for all the work you do RAD :)

// Legally, I can't distribute the dll, so I'll grab it from a legal source.
// https://origin.warframe.com/origin/00000000/index.txt.lzma

#include <filesystem>

namespace fs = std::filesystem;

enum class OodleHandlerResult {
	LOADED,			// successfully loaded
	NET_ERROR,		// failed to download
	LZMA_ERROR,		// some lzma error
	INDEX_ERROR,	// cannot parse warframe's index
	CANNOT_LOAD,	// cannot load dll
	CANNOT_WRITE	// cannot write dll
};

OodleHandlerResult LoadOodle(const fs::path& cachePath);