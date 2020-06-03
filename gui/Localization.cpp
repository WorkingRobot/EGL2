#include "Localization.h"

#ifndef LOG_SECTION
#define LOG_SECTION "Locale"
#endif

#include "../Logger.h"
#include "../resources.h"

#include <codecvt>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <zstd.h>

inline wchar_t* ReadString(char** bufPos) {
    uint16_t size = *(uint16_t*)(*bufPos);
    auto buf = new wchar_t[size + 1];
    memcpy(buf, *bufPos + sizeof(uint16_t), size * 2);
    buf[size] = L'\0';
    *bufPos += sizeof(uint16_t) + size * 2;
    return buf;
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

inline void SplitLCID(LCID lcid, uint8_t& sortId, uint16_t& languageId) {
    sortId = (lcid >> 16) & 0xF;
    languageId = lcid & 0xFFFF;
}

Locale Localization::GetSystemLocale()
{
    uint8_t sortId;
    // language ids contain 2 bytes
    // the first is the language byte (the actual language: en, fr, es, ar, etc.)
    // the second is the region byte (only unique to the language, one doesn't always correspond to the same region)
    // a region of 0x40 is not specifically US (only in en-US)
    uint16_t langId;
    SplitLCID(GetUserDefaultLCID(), sortId, langId);
    switch (langId & 0xFF)
    {
    case 0x01:
        return Locale::AR;
    case 0x07:
        return Locale::DE;
    case 0x09:
        return Locale::EN;
    case 0x0A:
        return Locale::ES;
    case 0x0B:
        return Locale::FI;
    case 0x0C:
        return Locale::FR;
    case 0x10:
        return Locale::IT;
    case 0x15:
        return Locale::PL;
    case 0x16:
        return Locale::PT_BR; // just treat it as normal portuguese (should be similar enough I hope)
    case 0x19:
        return Locale::RU;
    case 0x1F:
        return Locale::TR;
    default:
        LOG_WARN("Using default locale for %02X\n", langId & 0xFF);
        return Locale::EN;
    }
}
