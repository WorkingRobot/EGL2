#include "EGSProvider.h"

#ifndef LOG_SECTION
#define LOG_SECTION "EGSProvider"
#endif

#include "../Logger.h"

#include <filesystem>
#include <numeric>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>
#include <ShlObj_core.h>

namespace fs = std::filesystem;

EGSProvider::EGSProvider()
{
	LOG_INFO("Checking for existing EGS install");
	fs::path LauncherDat;
	{
		PWSTR progDataFolder;
		if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &progDataFolder) != S_OK) {
			LOG_DEBUG("Can't get ProgramData folder");
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
			LOG_DEBUG("Couldn't open install data file");
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
		LOG_DEBUG("Fortnite doesn't have a .egstore folder");
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
		LOG_DEBUG("Fortnite doesn't have a version manifest attatched");
		return;
	}

	auto manifestFp = fopen(manifestPath.string().c_str(), "rb");
	if (!manifestFp) {
		LOG_DEBUG("Couldn't open manifest file");
		return;
	}

	Build = std::make_unique<Manifest>(manifestFp);
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
		return false;
	}
	return std::any_of(Build->ChunkManifestList.begin(), Build->ChunkManifestList.end(), [&](std::shared_ptr<Chunk>& c) {
		return !memcmp(chunk->Guid, c->Guid, 16);
	});
}

std::shared_ptr<char[]> EGSProvider::getChunk(std::shared_ptr<Chunk>& chunk)
{
	std::vector<std::pair<File&, std::vector<ChunkPart>::iterator>> parts;
	for (auto& f : Build->FileManifestList) {
		for (auto c = f.ChunkParts.begin(); c != f.ChunkParts.end(); ++c) {
			if (!memcmp(chunk->Guid, c->Chunk->Guid, 16)) {
				parts.emplace_back(f, c);
			}
		}
	}
	std::sort(parts.begin(), parts.end(), [](std::pair<File&, std::vector<ChunkPart>::iterator>& a, std::pair<File&, std::vector<ChunkPart>::iterator>& b) {
		return a.second->Offset > b.second->Offset;
	});

	auto ret = std::shared_ptr<char[]>(new char[chunk->WindowSize]);
	auto amtRead = 0;
	for (auto& part : parts) {
		if (amtRead < part.second->Offset) {
			auto offset = std::accumulate(part.first.ChunkParts.begin(), part.second, (size_t)0, [](size_t offset, ChunkPart& part) {
				return offset + part.Size;
			});
			auto fp = fopen((InstallDir / part.first.FileName).string().c_str(), "rb");
			if (!fp) {
				return nullptr;
			}
			fseek(fp, offset, SEEK_SET);
			fread(ret.get() + amtRead, part.second->Size, 1, fp);
			fclose(fp);
			amtRead += part.second->Size;
		}
	}
	return ret;
}
