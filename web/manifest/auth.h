#pragma once

#include "manifest.h"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

class ManifestAuth {
public:
	ManifestAuth(fs::path& cachePath);
	~ManifestAuth();

	std::pair<std::string, std::string> ManifestAuth::GetLatestManifest();
	std::string GetManifestId(const std::string& Url);
	bool IsManifestCached(const std::string& Url);
	Manifest GetManifest(const std::string& Url);

private:
	void UpdateIfExpired(bool force = false);

	fs::path CachePath;

	std::string AccessToken;
	time_t ExpiresAt;
};