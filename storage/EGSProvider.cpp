#include "EGSProvider.h"

#ifndef LOG_SECTION
#define LOG_SECTION "EGSProvider"
#endif

#include "../Logger.h"
#include "../Stats.h"
#include "sha.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <numeric>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <ShlObj_core.h>

namespace fs = std::filesystem;

EGSProvider::EGSProvider()
{
	fs::path LauncherDat;
	{
		PWSTR progDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &progDataFolder) != S_OK) {
			LOG_ERROR("Can't get ProgramData folder");
			return;
		}
		LauncherDat = fs::path(progDataFolder) / "Epic" / "UnrealEngineLauncher" / "LauncherInstalled.dat";
		CoTaskMemFree(progDataFolder);
	}
	if (!fs::is_regular_file(LauncherDat)) {
		LOG_DEBUG("EGS isn't installed");
		return;
	}

	rapidjson::Document d;
	{
		auto launcherFp = fopen(LauncherDat.string().c_str(), "rb");
		if (!launcherFp) {
			LOG_ERROR("Couldn't open install data file");
			return;
		}

		char readBuffer[8192];
		rapidjson::FileReadStream is(launcherFp, readBuffer, sizeof(readBuffer));
		d.ParseStream(is);
		fclose(launcherFp);
	}
	if (d.HasParseError()) {
		LOG_ERROR("Reading EGS data: JSON Parse Error %d @ %zu", d.GetParseError(), d.GetErrorOffset());
		return;
	}

	auto& installList = d["InstallationList"].GetArray();
	auto val = std::find_if(installList.Begin(), installList.End(), [](rapidjson::Value& item) {
		return !strcmp(item["AppName"].GetString(), "Fortnite");
	});
	if (val == installList.End()) {
		LOG_DEBUG("Fortnite isn't installed");
		return;
	}

	InstallDir = (*val)["InstallLocation"].GetString();
	auto egsPath = InstallDir / ".egstore";
	if (!fs::is_directory(egsPath)) {
		LOG_ERROR("Fortnite doesn't have a .egstore folder");
		return;
	}

	fs::path manifestPath;
	for (auto& n : fs::directory_iterator(egsPath)) {
		if (n.path().extension() == ".manifest") {
			manifestPath = n;
			break;
		}
	}
	if (manifestPath.empty()) {
		LOG_ERROR("Fortnite doesn't have a version manifest attatched");
		return;
	}

	auto manifestFp = fopen(manifestPath.string().c_str(), "rb");
	if (!manifestFp) {
		LOG_ERROR("Couldn't open manifest file");
		return;
	}

	Build = std::make_unique<Manifest>(manifestFp);
	int i = 0;
	for (auto c = Build->ChunkManifestList.begin(); c != Build->ChunkManifestList.end(); ++c) {
		ChunkLookup[(*c)->Guid] = i++;
	}
}

EGSProvider::~EGSProvider()
{
}

bool EGSProvider::available()
{
	return (bool)Build;
}

bool EGSProvider::isChunkAvailable(std::shared_ptr<Chunk>& chunk)
{
	if (!available()) {
		LOG_DEBUG("(CHUNK) Not available");
		return false;
	}
	return ChunkLookup.find(chunk->Guid) != ChunkLookup.end();
}

class ChunkSection {
public:
	ChunkSection(const File& f, size_t o, const ChunkPart& p) : file(f), offset(o), part(p) { }

	inline uint32_t PartOffset() const {
		return part.get().Offset;
	}

	inline uint32_t PartSize() const {
		return part.get().Size;
	}

	inline size_t FileOffset() const {
		return offset;
	}

	inline const std::string& FileName() const {
		return file.get().FileName;
	}

private:
	std::reference_wrapper<const File> file;
	size_t offset;
	std::reference_wrapper<const ChunkPart> part;

};

std::shared_ptr<char[]> EGSProvider::getChunk(std::shared_ptr<Chunk>& chunk)
{
	LOG_DEBUG("GETTING CHUNK PARTS");
	std::vector<ChunkSection> parts;
	for (auto& f : Build->FileManifestList) {
		size_t offset = 0;
		for (auto& c : f.ChunkParts) {
			if (!memcmp(chunk->Guid, c.Chunk->Guid, 16)) {
				parts.emplace_back(std::cref(f), offset, std::cref(c));
			}
			offset += c.Size;
		}
	}
	LOG_DEBUG("SORTING CHUNK PARTS");
	std::sort(parts.begin(), parts.end(), [](ChunkSection& a, ChunkSection& b) {
		return a.PartOffset() < b.PartOffset();
	});

	LOG_DEBUG("CREATING CHUNK PARTS");
	auto ret = std::shared_ptr<char[]>(new char[chunk->WindowSize]);
	auto amtRead = 0;
	for (auto& part : parts) {
		if (amtRead == part.PartOffset()) {
			LOG_DEBUG("OPENING EGL FILE %s", part.FileName().c_str());
			std::ifstream fp(InstallDir / part.FileName(), std::ios::in | std::ios::binary);
			if (!fp.good()) {
				LOG_ERROR("Couldn't open file (%s) to get %s", (InstallDir / part.FileName()).string().c_str(), chunk->GetGuid().c_str());
				return nullptr;
			}
			LOG_DEBUG("SEEKING EGL FILE %llu", part.FileOffset());
			fp.seekg(part.FileOffset(), std::ios::beg);
			if (!fp.good()) {
				LOG_ERROR("Couldn't seek in file (%s) to %zu to get %s", (InstallDir / part.FileName()).string().c_str(), part.FileOffset(), chunk->GetGuid().c_str());
				return nullptr;
			}
			LOG_DEBUG("READING EGL FILE %d amt into buffer at %d", part.PartSize(), amtRead);
			fp.read(ret.get() + amtRead, part.PartSize());
			LOG_DEBUG("CLOSING CHUNK FILE");
			fp.close();
			Stats::FileReadCount.fetch_add(part.PartSize(), std::memory_order_relaxed);
			amtRead += part.PartSize();
		}
	}
	if (amtRead != chunk->WindowSize) {
		LOG_ERROR("Couldn't get entire chunk of %s", chunk->GetGuid().c_str());
		return nullptr;
	}
	if (!VerifyHash(ret.get(), chunk->WindowSize, chunk->ShaHash)) {
		LOG_ERROR("Chunk %s has invalid hash", chunk->GetGuid().c_str());
		return nullptr;
	}
	LOG_DEBUG("RETURNING EGL DATA");
	return ret;
}
