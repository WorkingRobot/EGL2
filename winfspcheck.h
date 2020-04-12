#pragma once

enum class WinFspCheckResult {
	LOADED			 = 0, // successfully loaded
	NO_PATH			 = 1, // cannot get program files x86 folder
	NO_DLL			 = 3, // no dll in winfsp folder
	CANNOT_LOAD		 = 4, // cannot load dll
};

WinFspCheckResult LoadWinFsp();