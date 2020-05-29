#pragma once

#include "feature_level.h"
#include "file.h"

#include <rapidjson/document.h>
#include <vector>

struct Manifest {
public:
	Manifest(const rapidjson::Document& jsonData, const std::string& url);
	~Manifest();

	uint64_t GetDownloadSize();
	uint64_t GetInstallSize();

	EFeatureLevel FeatureLevel;
	bool bIsFileData;
	uint32_t AppID;
	std::string AppName;
	std::string BuildVersion;
	std::string LaunchExe;
	std::string LaunchCommand;
	//std::set<std::string> PrereqIds;
	//std::string PrereqName;
	//std::string PrereqPath;
	//std::string PrereqArgs;
	std::vector<File> FileManifestList;
	std::vector<std::shared_ptr<Chunk>> ChunkManifestList;

	std::string CloudDir;
};