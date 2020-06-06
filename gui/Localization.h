#pragma once

#define LSTR(name) Localization::GetString(LocaleString::name)
#define LTITLE(str) wxString::Format("%s - EGL2", str)

#include "localedefs.h"

#include <memory>

enum class Locale : uint8_t {
#define LS(name) name,
    LOCALETYPES
#undef LS

    // If you want to help with translating EGL2 into other languages, dm me @AsrielD#6969
    Count
};

enum class LocaleString : uint16_t {
#define LS(name) name,
    LOCALESTRINGS
#undef LS

    Count
};

class Localization {
public:
    Localization() = delete;
    Localization(const Localization&) = delete;
    Localization& operator=(const Localization&) = delete;

    static Locale GetSystemLocale();

    static bool UseLocale(Locale locale);

    static inline const wchar_t* GetString(LocaleString string) {
        return LocaleStrings[(int)string].get();
    }

private:
    static inline Locale SelectedLocale = (Locale)-1;

    static inline std::unique_ptr<wchar_t[]> LocaleStrings[(int)LocaleString::Count];
};