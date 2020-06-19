#include "GameUpdateChecker.h"

#ifndef LOG_SECTION
#define LOG_SECTION "UpdateChecker"
#endif

#include "../Logger.h"

GameUpdateChecker::GameUpdateChecker(fs::path cachePath, UpdateCallback callback, std::chrono::milliseconds checkInterval) :
	Auth(cachePath),
	Callback(callback),
	CheckInterval(checkInterval)
{
	LOG_DEBUG("Initializing force update");
	ForceUpdate();
	LOG_DEBUG("Creating update thread");
	UpdateThread = std::thread(&GameUpdateChecker::Thread, this);
	UpdateThread.detach(); // causes some exception when the deconstructer is trying to join it otherwise
}

GameUpdateChecker::~GameUpdateChecker() {
	UpdateFlag.cancel();
}

void GameUpdateChecker::SetInterval(std::chrono::milliseconds newInterval)
{
	CheckInterval.store(newInterval);
	UpdateWakeup = std::chrono::steady_clock::time_point::min(); // force update
}

void GameUpdateChecker::StopUpdateThread()
{
	UpdateFlag.cancel();
}

bool GameUpdateChecker::ForceUpdate() {
	auto info = Auth.GetLatestManifest();
	auto id = Auth.GetManifestId(info.first);
	if (LatestId == id) {
		return false;
	}
	LatestUrl = info.first;
	LatestId = id;
	LatestVersion = info.second;
	return true;
}

std::string& GameUpdateChecker::GetLatestId()
{
	return LatestId;
}

std::string& GameUpdateChecker::GetLatestUrl()
{
	return LatestUrl;
}

std::string& GameUpdateChecker::GetLatestVersion()
{
	return LatestVersion;
}

Manifest GameUpdateChecker::GetManifest(const std::string& Url)
{
	return Auth.GetManifest(Url);
}

void GameUpdateChecker::Thread() {
	while (!UpdateFlag.cancelled()) {
		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		} while (std::chrono::steady_clock::now() < UpdateWakeup);
		if (ForceUpdate()) {
			Callback(LatestUrl, LatestVersion);
			UpdateWakeup = std::chrono::steady_clock::now() + std::chrono::seconds(60); // fixes load balancing issues
		}
		else {
			UpdateWakeup = std::chrono::steady_clock::now() + CheckInterval.load();
		}
	}
}