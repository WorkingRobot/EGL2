#include "Stats.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

constexpr inline ULONGLONG ConvertTime(FILETIME& ft) {
	return ULARGE_INTEGER{ ft.dwLowDateTime, ft.dwHighDateTime }.QuadPart;
}

void Stats::StartUpdateThread(ch::milliseconds refreshRate, std::function<void(StatsUpdateData&)> updateCallback) {
	UpdateThread = std::thread([refreshRate, updateCallback]() {
		StatsUpdateData updateData;
		memset(&updateData, 0, sizeof StatsUpdateData);

		auto refreshScale = 1000.f / refreshRate.count();

		uint64_t prevSysIdle = 0;
		uint64_t prevSysUse = 0;
		uint64_t prevProcUse = 0;
		FILETIME sysIdleFt, sysKernelFt, sysUserFt;
		FILETIME procCreateFt, procExitFt, procKernelFt, procUserFt;

		PROCESS_MEMORY_COUNTERS_EX pmc;

		uint64_t prevRead = 0;
		uint64_t prevWrite = 0;
		uint64_t prevProvide = 0;
		uint64_t prevDownload = 0;
		uint64_t prevLatOp = 0;
		uint64_t prevLatNs = 0;
		while (!UpdateFlag.cancelled()) {
			// cpu calculation
			if (GetSystemTimes(&sysIdleFt, &sysKernelFt, &sysUserFt) && GetProcessTimes(GetCurrentProcess(), &procCreateFt, &procExitFt, &procKernelFt, &procUserFt)) {
				uint64_t sysIdle = ConvertTime(sysIdleFt);
				uint64_t sysUse = ConvertTime(sysKernelFt) + ConvertTime(sysUserFt);
				uint64_t procUse = ConvertTime(procKernelFt) + ConvertTime(procUserFt);

				if (prevSysIdle)
				{
					uint64_t sysTotal = sysUse - prevSysUse;
					uint64_t procTotal = procUse - prevProcUse;

					if (sysTotal > 0) {
						updateData.cpu = float((double)procTotal / sysTotal * 100);
					}
				}

				prevSysIdle = sysIdle;
				prevSysUse = sysUse;
				prevProcUse = procUse;
			}

			// ram calculation
			if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
				updateData.ram = pmc.PrivateUsage;
			}

			// thread count calculation
			{
				auto procId = GetCurrentProcessId();
				HANDLE hnd = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, procId);
				if (hnd != INVALID_HANDLE_VALUE) {
					THREADENTRY32 threadEntry;
					threadEntry.dwSize = sizeof THREADENTRY32;

					if (Thread32First(hnd, &threadEntry)) {
						int threadCount = 0;
						do {
							if (threadEntry.th32OwnerProcessID == procId) {
								threadCount++;
							}
						} while (Thread32Next(hnd, &threadEntry));
						updateData.threads = threadCount;
					}

					CloseHandle(hnd);
				}
			}

			// other atomic stuff to pass
			uint64_t read = FileReadCount.load(std::memory_order_relaxed);
			uint64_t write = FileWriteCount.load(std::memory_order_relaxed);
			uint64_t provide = ProvideCount.load(std::memory_order_relaxed);
			uint64_t download = DownloadCount.load(std::memory_order_relaxed);
			uint64_t latOp = LatOpCount.load(std::memory_order_relaxed);
			uint64_t latNs = LatNsCount.load(std::memory_order_relaxed);

			updateData.read = (read - prevRead) * refreshScale;
			updateData.write = (write - prevWrite) * refreshScale;
			updateData.provide = (provide - prevProvide) * refreshScale;
			updateData.download = (download - prevDownload) * refreshScale;
			updateData.latency = ((double)(latNs - prevLatNs) / (latOp - prevLatOp)) / 1000000 * refreshScale;

			prevRead = read;
			prevWrite = write;
			prevProvide = provide;
			prevDownload = download;
			prevLatOp = latOp;
			prevLatNs = latNs;

			if (UpdateFlag.cancelled()) {
				return;
			}
			updateCallback(updateData);

			std::this_thread::sleep_for(refreshRate);
		}
	});
	UpdateThread.detach();
}

void Stats::StopUpdateThread() {
	UpdateFlag.cancel();
}