#include "winfspcheck.h"

#include <filesystem>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShlObj_core.h>

namespace fs = std::filesystem;

WinFspCheckResult LoadWinFsp() {
    static bool alreadyLoaded = false;
    if (alreadyLoaded) {
        return WinFspCheckResult::LOADED;
    }

    fs::path DataFolder;
    {
        PWSTR appDataFolder;
        if (SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, NULL, &appDataFolder) != S_OK) {
            return WinFspCheckResult::NO_PATH;
        }
        DataFolder = appDataFolder;
        CoTaskMemFree(appDataFolder);
    }
    DataFolder = DataFolder / "WinFsp" / "bin" / "winfsp-x64.dll";

    if (fs::status(DataFolder).type() != fs::file_type::regular) {
        return WinFspCheckResult::NO_DLL;
    }
    if (LoadLibraryA(DataFolder.string().c_str()) == NULL) {
        return WinFspCheckResult::CANNOT_LOAD;
    }
    alreadyLoaded = true;
    return WinFspCheckResult::LOADED;
}