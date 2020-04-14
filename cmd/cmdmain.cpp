#include "../MountedBuild.h"
#include "../checks/winfspcheck.h"

static inline const char* humanSize(uint64_t bytes)
{
    char* suffix[] = { "B", "KB", "MB", "GB", "TB" };
    char length = sizeof(suffix) / sizeof(suffix[0]);

    int i = 0;
    double dblBytes = bytes;

    if (bytes > 1024) {
        for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
            dblBytes = bytes / 1024.0;
    }

    static char output[16];
    sprintf(output, "%.04lf %s", dblBytes, suffix[i]);
    return output;
}

#define argtos(v)                       if (arge > ++argp) v = *argp; else goto usage
#define argtol(v)                       if (arge > ++argp) v = wcstol_deflt(*argp, v); else goto usage

static inline ULONG wcstol_deflt(wchar_t* w, ULONG deflt)
{
    wchar_t* endp;
    ULONG ul = wcstol(w, &endp, 0);
    return L'\0' != w[0] && L'\0' == *endp ? ul : deflt;
}

int wmain(ULONG argc, PWSTR* argv)
{
    {
        auto result = LoadWinFsp();
        if (result != WinFspCheckResult::LOADED) {
            switch (result)
            {
            case WinFspCheckResult::NO_PATH:
                printf("Could not get your Program Files (x86) folder. I honestly have no idea how you'd get this error.\n");
                break;
            case WinFspCheckResult::NO_DLL:
                printf("Could not find WinFsp's DLL in the driver's folder. Try reinstalling WinFsp.\n");
                break;
            case WinFspCheckResult::CANNOT_LOAD:
                printf("Could not load WinFsp's DLL in the driver's folder. Try reinstalling WinFsp.\n");
                break;
            default:
                printf("An unknown error occurred when trying to load WinFsp's DLL: %d\n", result);
                break;
            }
            return 0;
        }
    }

    wchar_t** argp, ** arge;

    PWSTR GamePath = 0, CachePath = 0, MountPath = 0;
    ULONG DownloadThreadCount = 0, CompressionMethod = 0, CompressionLevel = 0;
    bool Verify = false, RemoveUnused = false;
    for (argp = argv + 1, arge = argv + argc; arge > argp; argp++)
    {
        if (L'-' != argp[0][0])
            break;
        switch (argp[0][1])
        {
        case L'?':
            goto usage;
        case L'G':
            argtos(GamePath);
            break;
        case L'P':
            argtol(DownloadThreadCount);
            break;
        case L'C':
            argtos(CachePath);
            break;
        case L'M':
            argtos(MountPath);
            break;
        case L'R':
            RemoveUnused = true;
            break;
        case L'V':
            Verify = true;
            break;
        case 'S':
            argtol(CompressionMethod);
            break;
        case 's':
            argtol(CompressionLevel);
            break;
        default:
            goto usage;
        }
    }
    if (!CachePath || !MountPath) {
        goto usage;
    }
    uint32_t StorageFlags = 0;
    if (Verify) {
        StorageFlags |= StorageVerifyHashes;
    }
    if (CompressionMethod) {
        switch (CompressionMethod)
        {
        case 1:
            StorageFlags |= StorageDecompressed;
            break;
        case 2:
            StorageFlags |= StorageCompressed;
            break;
        case 3:
            StorageFlags |= StorageCompressLZ4;
            break;
        case 4:
            StorageFlags |= StorageCompressZlib;
            break;
        default:
            wprintf(L"Unknown compression method %d\n", CompressionMethod);
            goto usage;
            break;
        }
    }
    else {
        StorageFlags |= StorageCompressLZ4;
    }
    if (CompressionLevel) {
        switch (CompressionLevel)
        {
        case 1:
            StorageFlags |= StorageCompressFastest;
            break;
        case 2:
            StorageFlags |= StorageCompressFast;
            break;
        case 3:
            StorageFlags |= StorageCompressNormal;
            break;
        case 4:
            StorageFlags |= StorageCompressSlow;
            break;
        case 5:
            StorageFlags |= StorageCompressSlowest;
            break;
        default:
            wprintf(L"Unknown compression level %d\n", CompressionLevel);
            goto usage;
            break;
        }
    }
    else {
        StorageFlags |= StorageCompressSlowest;
    }

    MANIFEST* manifest;
    MANIFEST_AUTH* auth;
    STORAGE* storage;
    ManifestAuthGrab(&auth);
    ManifestAuthGetManifest(auth, "",  &manifest);
    printf("Total download size: %s\n", humanSize(ManifestInstallSize(manifest)));
    (void)getchar();
    printf("Making build\n");
    MountedBuild* Build = new MountedBuild(manifest, MountPath, CachePath, [](const char* error) {printf("%s\n", error); });

    printf("Setting up cache dir\n");
    if (!Build->SetupCacheDirectory()) {
        printf("failed to setup cache dir\n");
    }

    printf("starting storage\n");
    if (!Build->StartStorage(StorageFlags)) {
        printf("failed to start storage\n");
    }

    if (RemoveUnused) {
        cancel_flag flag;
        Build->PurgeUnusedChunks([](uint32_t max) {}, []() {}, flag);
    }

    if (DownloadThreadCount) {
        printf("Predownloading\n");
        cancel_flag flag;
        if (!Build->PreloadAllChunks([](uint32_t max) {}, []() {}, flag, DownloadThreadCount)) {
            printf("failed to preload\n");
        }
    }

    printf("Starting\n");
    if (!Build->Mount()) {
        printf("failed to start\n");
    }

    if (GamePath) {
        printf("Setting up game dir\n");
        cancel_flag flag;
        if (!Build->SetupGameDirectory([](uint32_t max) {}, []() {}, flag, DownloadThreadCount ? DownloadThreadCount : 32, GamePath)) {
            printf("failed to setup game dir\n");
        }
    }

    printf("Started, press any key to close\n");
    (void)getchar();
    printf("Closing\n");
    delete Build;
    printf("Closed\n");
    return STATUS_SUCCESS;

usage:
    static char usage[] = ""
        "usage: EGL2.exe OPTIONS\n"
        "\n"
        "options:\n"
        "    -G GameMountDir     [optional: make Fortnite launchable here]\n"
        "    -P ThreadCount      [optional: predownload all chunks]\n"
        "    -C CacheDir         [directory to place downloaded chunk files]\n"
        "    -M MountDir         [drive (Y:) or directory to mount filesystem to]\n"
        "    -R                  [optional: before mounting, remove all chunks that aren't used in the manifest (for when updating)]\n"
        "    -V                  [optional: verify all read chunks to sha hashes and redownload if necessary]\n"
        // 1: uncompressed, 2: as downloaded (zlib), 3: lz4, 4: zlib
        "    -S Compression      [optional: Type of compression used (1-4)]\n"
        // 1: fastest, 2: fast, 3: normal, 4: slow, 5: slowest
        // only used when -S is 3 or 4
        "    -s CompressionLvl   [optional: Amount of compression used (1-5)]\n";

    printf(usage);

    return STATUS_UNSUCCESSFUL;
}