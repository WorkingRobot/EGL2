#include "Localization.h"

#ifndef LOG_SECTION
#define LOG_SECTION "Locale"
#endif

#include "../Logger.h"
#include "../resources.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <zstd.h>

inline char* ReadString(char** bufPos) {
    uint16_t size = *(uint16_t*)(*bufPos);
    auto ret = new char[size + 1];
    memcpy(ret, *bufPos + sizeof(uint16_t), size);
    ret[size] = '\0';
    *bufPos += sizeof(uint16_t) + size;
    return ret;
}

bool Localization::InitializeLocales()
{
    auto resInfo = FindResource(NULL, MAKEINTRESOURCE(LOCALE_INFO), RT_RCDATA);
    if (!resInfo) {
        LOG_FATAL("Could not find locale resource!");
        return false;
    }
    auto resData = LoadResource(NULL, resInfo);
    if (!resData) {
        LOG_FATAL("Could not load locale resource!");
        return false;
    }
    auto resPtr = LockResource(resData);
    if (!resPtr) {
        LOG_FATAL("Could not lock locale resource!");
        FreeResource(resData);
        return false;
    }
    auto resSize = SizeofResource(NULL, resInfo);
    if (!resSize) {
        LOG_FATAL("Could not get size of locale resource!");
        FreeResource(resData);
        return false;
    }
    LOG_INFO("%u", *(uint32_t*)resPtr);

    auto locData = std::unique_ptr<char[]>(new char[*(uint32_t*)resPtr]);
    auto locSize = ZSTD_decompress(locData.get(), *(uint32_t*)resPtr, (char*)resPtr + sizeof(uint32_t), resSize - sizeof(uint32_t));
    if (ZSTD_isError(locSize)) {
        LOG_FATAL("Could not decompress locale data: %s", ZSTD_getErrorName(locSize));
        FreeResource(resData);
        return false;
    }

    char* locPos = locData.get();
    for (int i = 0; i < (int)Locale::Count; ++i) {
        for (int j = 0; j < (int)LocaleString::Count; ++j) {
            if (locPos - locSize == locData.get()) {
                LOG_ERROR("Could not parse locale data at %d %d", i, j);
                FreeResource(resData);
                return true;
            }
            LocaleStrings[i][j].reset(ReadString(&locPos));
        }
    }

    FreeResource(resData);
    return true;
}
