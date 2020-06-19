#include "UpdateChecker.h"

#define GH_REPO "WorkingRobot/EGL2"
#define GH_RELEASE_URL "https://api.github.com/repos/" GH_REPO "/releases/latest"

#ifndef LOG_SECTION
#define LOG_SECTION "UpdateChecker"
#endif

#include "../Logger.h"
#include "../web/http.h"
#include "versioninfo.h"

#include <rapidjson/document.h>

UpdateChecker::UpdateChecker(UpdateCallback callback, std::chrono::milliseconds checkInterval) :
	Callback(callback),
	CheckInterval(checkInterval),
	LatestVersion(VERSION_STRING)
{
	LOG_DEBUG("Creating update thread");
	UpdateThread = std::thread(&UpdateChecker::Thread, this);
	UpdateThread.detach(); // causes some exception when the deconstructer is trying to join it otherwise
}

UpdateChecker::~UpdateChecker() {
	UpdateFlag.cancel();
}

void UpdateChecker::SetInterval(std::chrono::milliseconds newInterval)
{
	CheckInterval.store(newInterval);
	UpdateWakeup = std::chrono::steady_clock::time_point::min(); // force update
}

void UpdateChecker::StopUpdateThread()
{
	UpdateFlag.cancel();
}

bool UpdateChecker::ForceUpdate() {
	auto conn = Client::CreateConnection();
	conn->SetUrl(GH_RELEASE_URL);
	conn->SetUserAgent(GH_REPO);

	if (!Client::Execute(conn, cancel_flag(), true)) {
		LOG_WARN("Could not check for EGL2 update");
		return false;
	}

	if (conn->GetResponseCode() != 200) {
		return false;
	}

	rapidjson::Document releaseInfo;
	releaseInfo.Parse(conn->GetResponseBody().c_str());
	
	if (releaseInfo.HasParseError()) {
		LOG_ERROR("Getting release info: JSON Parse Error %d @ %zu", releaseInfo.GetParseError(), releaseInfo.GetErrorOffset());
		return false;
	}

	auto version = releaseInfo["tag_name"].GetString();
	if (LatestVersion == version) {
		LOG_DEBUG("NO EGL2 UPDATE");
		return false;
	}

	LatestInfo.Version = version;
	LatestInfo.Url = releaseInfo["html_url"].GetString();

	auto& exeAsset = releaseInfo["assets"][0];
	LatestInfo.DownloadUrl = exeAsset["browser_download_url"].GetString();
	LatestInfo.DownloadCount = exeAsset["download_count"].GetInt();
	LatestInfo.DownloadSize = exeAsset["size"].GetInt();
	return true;
}

UpdateInfo& UpdateChecker::GetLatestInfo()
{
	return LatestInfo;
}

void UpdateChecker::Thread() {
	while (!UpdateFlag.cancelled()) {
		do {
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
		} while (std::chrono::steady_clock::now() < UpdateWakeup);
		if (ForceUpdate()) {
			Callback(LatestInfo);
		}
		UpdateWakeup = std::chrono::steady_clock::now() + CheckInterval.load();
	}
}