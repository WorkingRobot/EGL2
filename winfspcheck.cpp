#include "winfspcheck.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>

#include <filesystem>
namespace fs = std::filesystem;


bool alreadyLoaded = false;

WinFspCheckResult LoadWinFsp() {
    if (alreadyLoaded) {
        return WinFspCheckResult::LOADED;
    }

    DWORD driverByteCount;
    if (EnumDeviceDrivers(NULL, 0, &driverByteCount)) {

        DWORD driverCount = driverByteCount / sizeof(LPVOID);
        LPVOID* drivers = new LPVOID[driverCount];
        WinFspCheckResult result = WinFspCheckResult::NOT_FOUND;

        if (EnumDeviceDrivers(drivers, driverByteCount, &driverByteCount)) {
            char driverFilename[MAX_PATH];

            for (int i = 0; i < driverCount; ++i) {
                if (GetDeviceDriverFileNameA(drivers[i], driverFilename, MAX_PATH) && strstr(driverFilename, "winfsp"))
                {

                    fs::path dll;
                    if (strncmp(driverFilename, "\\??\\", 4) == 0) {
                        dll = driverFilename + 4;
                    }
                    else {
                        dll = driverFilename;
                    }
                    dll = dll.replace_extension(".dll");

                    if (fs::status(dll).type() != fs::file_type::regular) {
                        result = WinFspCheckResult::NO_DLL;
                        continue;
                    }
                    if (LoadLibraryA(dll.string().c_str()) == NULL) {
                        result = WinFspCheckResult::CANNOT_LOAD;
                        continue;
                    }
                    alreadyLoaded = true;
                    result = WinFspCheckResult::LOADED;
                    break;
                }
            }
        }

        delete[] drivers;
        return result;
    }
    return WinFspCheckResult::CANNOT_ENUMERATE;
}