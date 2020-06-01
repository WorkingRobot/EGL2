#pragma once

#define LSTR(name) Localization::GetString(LocaleString::name)
#define LTITLE(str) wxString::Format("%s - EGL2", str)

#define LOCALESTRINGS \
    LS(APP_ERROR)                      /* "Error", shows up as "Error - EGL2" if there was an issue in setting up               */ \
    LS(APP_ERROR_RUNNING)              /* Error to show if EGL2 is already running                                              */ \
    LS(APP_ERROR_APPDATA)              /* Error to show if the APPDATA folder wasn't found                                      */ \
    LS(APP_ERROR_DATA)                 /* Error to show if EGL2 can't create a folder                                           */ \
    LS(APP_ERROR_LOGS)                 /* Error to show if EGL2 can't create a logs folder                                      */ \
    LS(APP_ERROR_MANIFESTS)            /* Error to show if EGL2 can't create a manifests folder                                 */ \
    LS(APP_ERROR_LOGFILE)              /* Error to show if EGL2 can't create a log file                                         */ \
    LS(APP_ERROR_PROGFILES86)          /* Error to show if the Program Files (x86) folder wasn't found                          */ \
    LS(APP_ERROR_WINFSP_FIND)          /* Error to show if WinFsp wasn't found                                                  */ \
    LS(APP_ERROR_WINFSP_LOAD)          /* Error to show if WinFsp's dll couldn't be loaded                                      */ \
    LS(APP_ERROR_WINFSP_UNKNOWN)       /* Error to show if an unknown error occurred                                            */ \
    LS(MAIN_BTN_SETTINGS)              /* Settings button name                                                                  */ \
    LS(MAIN_BTN_VERIFY)                /* Verify button name                                                                    */ \
    LS(MAIN_BTN_PLAY)                  /* Play button name                                                                      */ \
    LS(MAIN_BTN_UPDATE)                /* Update button name                                                                    */ \
    LS(MAIN_DESC_TITLE)                /* Button description box title                                                          */ \
    LS(MAIN_DESC_DEFAULT)              /* Default button description                                                            */ \
    LS(MAIN_DESC_SETTINGS)             /* Settings button description                                                           */ \
    LS(MAIN_DESC_VERIFY)               /* Verify button description                                                             */ \
    LS(MAIN_DESC_PLAY)                 /* Play button description                                                               */ \
    LS(MAIN_STATUS_STARTING)           /* Status bar for when starting up                                                       */ \
    LS(MAIN_STATUS_PLAYABLE)           /* Status bar for when game is playable                                                  */ \
    LS(MAIN_STATUS_SELLOUT)            /* Next to status bar for my SAC sellout                                                 */ \
    LS(MAIN_STATS_TITLE)               /* Stats box title                                                                       */ \
    LS(MAIN_STATS_CPU)                 /* CPU stat                                                                              */ \
    LS(MAIN_STATS_RAM)                 /* RAM stat                                                                              */ \
    LS(MAIN_STATS_READ)                /* Hard drive read speed                                                                 */ \
    LS(MAIN_STATS_WRITE)               /* Hard drive write speed                                                                */ \
    LS(MAIN_STATS_PROVIDE)             /* Speed at which data is given to programs (different from read speed if compressed)    */ \
    LS(MAIN_STATS_DOWNLOAD)            /* Download speed                                                                        */ \
    LS(MAIN_STATS_LATENCY)             /* Latency between program requesting data and recieving data                            */ \
    LS(MAIN_STATS_THREADS)             /* Number of threads running in EGL2                                                     */ \
    LS(MAIN_PROG_VERIFY)               /* Title of progress window when verifying                                               */ \
    LS(MAIN_PROG_UPDATE)               /* Title of progress window when updating                                                */ \
    LS(MAIN_EXIT_VETOMSG)              /* Message to show if Fortnite is running with EGL2                                      */ \
    LS(MAIN_EXIT_VETOTITLE)            /* Message box's title to show if Fortnite is running with EGL2                          */ \
    LS(MAIN_NOTIF_TITLE)               /* Notification title when an update is available                                        */ \
    LS(MAIN_NOTIF_DESC)                /* Notification description when an update is available                                  */ \
    LS(MAIN_NOTIF_ACTION)              /* Notification action text when an update is available                                  */ \
    LS(PROG_LABEL_ELAPSED)             /* Time elapsed                                                                          */ \
    LS(PROG_LABEL_ETA)                 /* Estimated time till finish                                                            */ \
    LS(PROG_BTN_CANCEL)                /* Cancel button                                                                         */ \
    LS(PROG_BTN_CANCELLING)            /* Cancel button text when already clicked and cancelling                                */ \
    LS(SETUP_TITLE)                    /* Title of setup/settings window                                                        */ \
    LS(SETUP_COMP_METHOD_ZSTD)         /* zstd compression method                                                               */ \
    LS(SETUP_COMP_METHOD_LZ4)          /* lz4 compression method                                                                */ \
    LS(SETUP_COMP_METHOD_DECOMP)       /* Decompression compression method (no compression)                                     */ \
    LS(SETUP_COMP_LEVEL_FASTEST)       /* Fastest compression speed                                                             */ \
    LS(SETUP_COMP_LEVEL_FAST)          /* Fast compression speed                                                                */ \
    LS(SETUP_COMP_LEVEL_NORMAL)        /* Normal compression speed                                                              */ \
    LS(SETUP_COMP_LEVEL_SLOW)          /* Slow compression speed                                                                */ \
    LS(SETUP_COMP_LEVEL_SLOWEST)       /* Slowest compression speed                                                             */ \
    LS(SETUP_UPDATE_LEVEL_SEC1)        /* 1 second                                                                              */ \
    LS(SETUP_UPDATE_LEVEL_SEC5)        /* 5 seconds                                                                             */ \
    LS(SETUP_UPDATE_LEVEL_SEC10)       /* 10 seconds                                                                            */ \
    LS(SETUP_UPDATE_LEVEL_SEC30)       /* 30 seconds                                                                            */ \
    LS(SETUP_UPDATE_LEVEL_MIN1)        /* 1 minute                                                                              */ \
    LS(SETUP_UPDATE_LEVEL_MIN5)        /* 5 minutes                                                                             */ \
    LS(SETUP_UPDATE_LEVEL_MIN10)       /* 10 minutes                                                                            */ \
    LS(SETUP_UPDATE_LEVEL_MIN30)       /* 30 minutes                                                                            */ \
    LS(SETUP_UPDATE_LEVEL_HOUR1)       /* 1 hour                                                                                */ \
    LS(SETUP_GENERAL_LABEL)            /* General settings section title                                                        */ \
    LS(SETUP_GENERAL_INSTFOLDER)       /* Install folder                                                                        */ \
    LS(SETUP_GENERAL_COMPMETHOD)       /* Compression method                                                                    */ \
    LS(SETUP_GENERAL_COMPLEVEL)        /* Compression level                                                                     */ \
    LS(SETUP_GENERAL_UPDATEINT)        /* Update Interval (how often EGL2 checks for updates)                                   */ \
    LS(SETUP_ADVANCED_LABEL)           /* Advanced settings section title                                                       */ \
    LS(SETUP_ADVANCED_BUFCT)           /* Number of chunks/buffers to keep in memory before reading from disk again             */ \
    LS(SETUP_ADVANCED_THDCT)           /* Number of threads to use when verifying or updating                                   */ \
    LS(SETUP_ADVANCED_CMDARGS)         /* Command arguments to launch with Fortnite                                             */ \
    LS(SETUP_BTN_OK)                   /* OK button in setup                                                                    */ \
    LS(SETUP_BTN_CANCEL)               /* Cancel button in setup                                                                */ \

#define LOCALETYPES \
    LS(AR)      /* Arabic               */ \
    LS(DE)      /* German               */ \
    LS(EN)      /* English              */ \
    LS(ES)      /* Spanish              */ \
    LS(FI)      /* Finnish              */ \
    LS(FR)      /* French               */ \
    LS(IT)      /* Italian              */ \
    LS(PL)      /* Polish               */ \
    LS(PT_BR)   /* Brazilian Portuguese */ \
    LS(RU)      /* Russian              */ \
    LS(TR)      /* Turkish              */ \

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

    static bool InitializeLocales();

    static inline void LoadLocale(Locale locale) {
        SelectedLocale = locale;
    }

    static inline const wchar_t* GetString(LocaleString string) {
        return LocaleStrings[(int)SelectedLocale][(int)string].get();
    }

private:
    static inline Locale SelectedLocale = Locale::RU;

    static inline std::unique_ptr<wchar_t[]> LocaleStrings[(int)Locale::Count][(int)LocaleString::Count];
};