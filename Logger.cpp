#include "Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

bool Logger::Setup() {
	auto stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (stdoutHandle != INVALID_HANDLE_VALUE) {
		DWORD outMode;
		if (GetConsoleMode(stdoutHandle, &outMode)) {
			return SetConsoleMode(stdoutHandle, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		}
	}
	return false;
}