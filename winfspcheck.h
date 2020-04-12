#pragma once

enum class WinFspCheckResult {
	LOADED			 = 0, // successfully loaded
	CANNOT_ENUMERATE = 1, // cannot enumerate over drivers
	NOT_FOUND		 = 2, // cannot find driver
	NO_DLL			 = 3, // no dll in driver folder
	CANNOT_LOAD		 = 4, // cannot load dll
};

WinFspCheckResult LoadWinFsp();