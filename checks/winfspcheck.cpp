#include "winfspcheck.h"

#define WINFSP_INSTALL_REGKEY   L"Software\\WOW6432Node\\WinFsp"
#define WINFSP_INSTALL_REGVALUE L"InstallDir"

#include <filesystem>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ShlObj_core.h>

namespace fs = std::filesystem;

fs::path GetRegInstallDir() {
    HKEY key;
    auto Result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WINFSP_INSTALL_REGKEY, NULL, KEY_READ, &key);

    if (Result != ERROR_SUCCESS) { // should be ERROR_NO_MATCH or ERROR_FILE_NOT_FOUND
        return nullptr; // In reality, it should always exist, so this shouldn't run
    }
    else {
        WCHAR data[MAX_PATH];
        DWORD type = REG_SZ;
        DWORD size = sizeof(data);
        Result = RegQueryValueEx(key, WINFSP_INSTALL_REGVALUE, NULL, &type, (LPBYTE)&data, &size);
        RegCloseKey(key);
        return Result == ERROR_SUCCESS ? data : nullptr;
    }
}

WinFspCheckResult LoadWinFsp() {
    static bool alreadyLoaded = false;
    if (alreadyLoaded) {
        return WinFspCheckResult::LOADED;
    }

    fs::path InstallDir = GetRegInstallDir();
    if (InstallDir.empty()) {
        {
            PWSTR appDataFolder;
            if (SHGetKnownFolderPath(FOLDERID_ProgramFilesX86, 0, NULL, &appDataFolder) != S_OK) {
                return WinFspCheckResult::NO_PATH;
            }
            InstallDir = fs::path(appDataFolder) / "WinFsp";
            CoTaskMemFree(appDataFolder);
        }
    }

    InstallDir = InstallDir / "bin" / "winfsp-x64.dll";

    if (fs::status(InstallDir).type() != fs::file_type::regular) {
        return WinFspCheckResult::NO_DLL;
    }
    if (LoadLibraryA(InstallDir.string().c_str()) == NULL) {
        return WinFspCheckResult::CANNOT_LOAD;
    }
    alreadyLoaded = true;
    return WinFspCheckResult::LOADED;
}