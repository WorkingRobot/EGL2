#pragma once

#include "../http.h"
#include "DeviceCodeAuth.h"

#include <filesystem>

namespace fs = std::filesystem;

struct DeviceAuthData {
	std::string AccountId;
	std::string DeviceId;
	std::string Secret;
};

class PersonalAuth {
public:
	PersonalAuth(const fs::path& filePath, DevCodeCallback devSetupCb);
	~PersonalAuth();

	bool GetExchangeCode(std::string& code);
	void Recreate();

private:
	void UpdateIfExpired(bool force = false);

	bool UseOAuthResp(const std::string& data);
	bool UseRefreshToken(const std::string& token);
	bool UseDeviceAuth(const DeviceAuthData& auth);

	bool CreateDeviceAuth(DeviceAuthData& auth);

	static bool SaveDeviceAuth(const fs::path& filePath, const DeviceAuthData& auth);
	static bool ReadDeviceAuth(const fs::path& filePath, DeviceAuthData& auth);

	std::string AccountId;
	std::string DisplayName;
	std::string AccessToken;
	std::string RefreshToken;
	time_t ExpiresAt;

	fs::path FilePath;
	DevCodeCallback DevSetupCb;
};