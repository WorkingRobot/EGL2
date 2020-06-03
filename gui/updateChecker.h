#pragma once
#include "../containers/cancel_flag.h"
#include "../web/manifest/auth.h"

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <thread>

namespace fs = std::filesystem;

typedef std::function<void(const std::string& Url, const std::string& Version)> UpdateCallback;

class UpdateChecker {
public:
	UpdateChecker(fs::path cachePath, UpdateCallback callback, std::chrono::milliseconds checkInterval);
	~UpdateChecker();

	void SetInterval(std::chrono::milliseconds newInterval);

	void StopUpdateThread();

	bool ForceUpdate();

	std::string& GetLatestId();
	std::string& GetLatestUrl();
	std::string& GetLatestVersion();

	static std::string GetReadableVersion(const std::string_view& version) {
		auto start = version.find('-') + 1;
		auto start2 = version.find('-', start);
		auto end2 = version.find('-', start2 + 1) + 1;
		auto end = version.find('-', end2);

		char buf[64];
		sprintf_s(buf, "%.*s (CL: %.*s)", start2 - start, version.data() + start, end - end2, version.data() + end2);
		return buf;
	}

	Manifest GetManifest(const std::string& Url);

private:
	void Thread();

	std::atomic<std::chrono::milliseconds> CheckInterval;

	ManifestAuth Auth;
	std::string LatestId;
	std::string LatestUrl;
	std::string LatestVersion;

	UpdateCallback Callback;
	std::thread UpdateThread;
	std::chrono::steady_clock::time_point UpdateWakeup;
	cancel_flag UpdateFlag;
};