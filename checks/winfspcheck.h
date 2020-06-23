#pragma once

enum class WinFspCheckResult {
	LOADED,		// successfully loaded
	NO_PATH,	// cannot get program files x86 folder
	NO_DLL,		// no dll in winfsp folder
	CANNOT_LOAD	// cannot load dll
};

WinFspCheckResult LoadWinFsp();