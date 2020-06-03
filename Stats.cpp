#include "Stats.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

constexpr inline ULONGLONG ConvertTime(FILETIME& ft) {
	return ULARGE_INTEGER{ ft.dwLowDateTime, ft.dwHighDateTime }.QuadPart;
}

void Stats::UpdateData()
{
	memset(&Data, 0, sizeof StatsUpdateData);

	static auto refreshScale = 1000.f / RefreshRate.count();

	static uint64_t prevSysIdle = 0;
	static uint64_t prevSysUse = 0;
	static uint64_t prevProcUse = 0;
	static FILETIME sysIdleFt, sysKernelFt, sysUserFt;
	static FILETIME procCreateFt, procExitFt, procKernelFt, procUserFt;

	static PROCESS_MEMORY_COUNTERS_EX pmc;

	static uint64_t prevRead = 0;
	static uint64_t prevWrite = 0;
	static uint64_t prevProvide = 0;
	static uint64_t prevDownload = 0;
	static uint64_t prevLatOp = 0;
	static uint64_t prevLatNs = 0;

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
				Data.cpu = float((double)procTotal / sysTotal * 100);
			}
		}

		prevSysIdle = sysIdle;
		prevSysUse = sysUse;
		prevProcUse = procUse;
	}

	// ram calculation
	if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
		Data.ram = pmc.PrivateUsage;
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
				Data.threads = threadCount;
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

	Data.read = (read - prevRead) * refreshScale;
	Data.write = (write - prevWrite) * refreshScale;
	Data.provide = (provide - prevProvide) * refreshScale;
	Data.download = (download - prevDownload) * refreshScale;
	Data.latency = ((double)(latNs - prevLatNs) / (latOp - prevLatOp)) / 1000000 * refreshScale;

	prevRead = read;
	prevWrite = write;
	prevProvide = provide;
	prevDownload = download;
	prevLatOp = latOp;
	prevLatNs = latNs;
}
