#pragma once

#include "containers/cancel_flag.h"

#include <atomic>
#include <functional>
#include <thread>
#include <wx/string.h>

namespace ch = std::chrono;

struct StatsUpdateData {
#define DEFINE_STAT(name, type) type name;

	DEFINE_STAT(cpu, float)
	DEFINE_STAT(ram, size_t)
	DEFINE_STAT(read, size_t)
	DEFINE_STAT(write, size_t)
	DEFINE_STAT(provide, size_t)
	DEFINE_STAT(download, size_t)
	DEFINE_STAT(latency, float)
	DEFINE_STAT(threads, int)

#undef DEFINE_STAT
};

class Stats {
public:
	Stats() = delete;
	Stats(const Stats&) = delete;
	Stats& operator=(const Stats&) = delete;

	static inline const wxString GetReadableSize(size_t size) {
		if (!size) {
			return "0 B";
		}

		static constexpr const char* suffix[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB" };
		int i = 0;
		auto sizeD = (double)size;
		while (sizeD >= 1024) {
			sizeD /= 1024;
			i++;
		}
		return wxString::Format("%.*f %s", 2 - (int)floor(log10(sizeD)), sizeD, suffix[i]);
	}

	template<class Callback, class ...Args>
	static inline void StartUpdateThread(ch::milliseconds refreshRate, Callback&& updateCallback, Args&&... args) {
		if (ThreadRunning) {
			return;
		}
		RefreshRate = refreshRate;

		std::thread([=]() {
			ThreadRunning = true;
			while (!Flag.cancelled()) {
				UpdateData();
				if (Flag.cancelled()) {
					break;
				}
				if (!updateCallback(Data, args...)) {
					break;
				}
				std::this_thread::sleep_for(RefreshRate);
			}
			ThreadRunning = false;
		}).detach();
	}

	static inline void StopUpdateThread() {
		Flag.cancel();
	}

	static inline std::atomic_uint64_t ProvideCount = 0; // std::atomic_uint_fast64_t is the same in msvc
	static inline std::atomic_uint64_t FileReadCount = 0;
	static inline std::atomic_uint64_t FileWriteCount = 0;
	static inline std::atomic_uint64_t DownloadCount = 0;
	static inline std::atomic_uint64_t LatOpCount = 0;
	static inline std::atomic_uint64_t LatNsCount = 0;

private:
	static inline StatsUpdateData Data;
	static inline ch::milliseconds RefreshRate;
	static inline bool ThreadRunning = false;
	static inline cancel_flag Flag;
	static void UpdateData();
};