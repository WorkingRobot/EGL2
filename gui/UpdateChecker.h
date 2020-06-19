#pragma once
#include "../containers/cancel_flag.h"

#include <functional>
#include <thread>

struct UpdateInfo {
	std::string Version;
	std::string	Url;

	std::string	DownloadUrl;
	int DownloadCount;
	int DownloadSize;
};

typedef std::function<void(const UpdateInfo& Info)> UpdateCallback;

class UpdateChecker {
public:
	UpdateChecker(UpdateCallback callback, std::chrono::milliseconds checkInterval);
	~UpdateChecker();

	void SetInterval(std::chrono::milliseconds newInterval);

	void StopUpdateThread();

	bool ForceUpdate();

	UpdateInfo& GetLatestInfo();

private:
	void Thread();

	std::atomic<std::chrono::milliseconds> CheckInterval;

	std::string LatestVersion;
	UpdateInfo LatestInfo;

	UpdateCallback Callback;
	std::thread UpdateThread;
	std::chrono::steady_clock::time_point UpdateWakeup;
	cancel_flag UpdateFlag;
};