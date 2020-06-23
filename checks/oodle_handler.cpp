#include "oodle_handler.h"

#define WARFRAME_CDN_HOST "https://origin.warframe.com" // content.warframe.com is also viable
#define WARFRAME_INDEX_PATH "/origin/E926E926/index.txt.lzma" // id is any random 8 char hex
#define WARFRAME_INDEX_URL WARFRAME_CDN_HOST WARFRAME_INDEX_PATH
#define OODLE_DLL_NAME "oo2core_8_win64.dll"

#include "../web/http.h"

#include <fstream>
#include <lzma.h>
#include <memory>

int LoadDll(const fs::path& dllPath) {
    if (fs::status(dllPath).type() != fs::file_type::regular) {
        return 1;
    }
    std::error_code ec;
    if (fs::file_size(dllPath, ec) < 512 * 1024) { // size too small to be oodle
        return 2;
    }
    if (LoadLibraryA(dllPath.string().c_str()) == NULL) {
        return 2;
    }
    return 0;
}

OodleHandlerResult ParseData(const char* url, std::stringstream& outStrm) {
    std::string indexData;
    int indexDataOffset = 0;
    {
        auto indexConn = Client::CreateConnection();
        indexConn->SetUrl(url
        );
        if (!Client::Execute(indexConn, cancel_flag())) {
            return OodleHandlerResult::NET_ERROR;
        }
        indexData = indexConn->GetResponseBody();
    }

    lzma_stream strm = LZMA_STREAM_INIT;

    if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK) {
        return OodleHandlerResult::LZMA_ERROR;
    }

    auto outBuf = std::make_unique<char[]>(1024 * 1024);
    strm.next_out = (uint8_t*)outBuf.get();

    while (indexDataOffset < indexData.size()) {
        strm.next_in = (uint8_t*)indexData.data() + indexDataOffset;
        strm.avail_in = std::min(1024 * 1024, (int)indexData.size() - indexDataOffset);
        strm.avail_out = 1024 * 1024;
        auto ret = lzma_code(&strm, LZMA_RUN);
        if (ret == LZMA_OK) {
            outStrm.write(outBuf.get(), 1024 * 1024 - strm.avail_out);
            indexDataOffset += 1024 * 1024 - strm.avail_in;
        }
        else if (ret == LZMA_STREAM_END) {
            outStrm.write(outBuf.get(), 1024 * 1024 - strm.avail_out);
            break;
        }
        else {
            lzma_end(&strm);
            return OodleHandlerResult::LZMA_ERROR;
        }
    }

    lzma_end(&strm);
    return OodleHandlerResult::LOADED;
}

OodleHandlerResult LoadOodle(const fs::path& cachePath) {
    static bool alreadyLoaded = false;
    if (alreadyLoaded) {
        return OodleHandlerResult::LOADED;
    }

    auto loadRet = LoadDll(cachePath / OODLE_DLL_NAME);
    if (!loadRet) {
        alreadyLoaded = true;
        return OodleHandlerResult::LOADED;
    }
    {
        std::error_code ec;
        fs::remove(cachePath / OODLE_DLL_NAME, ec);
    }

    std::stringstream outStrm;
    auto ret = ParseData(WARFRAME_INDEX_URL, outStrm);
    if (ret != OodleHandlerResult::LOADED) {
        return ret;
    }

    std::string line, dllUrl;
    while (std::getline(outStrm, line)) {
        if (line.find(OODLE_DLL_NAME) != std::string::npos) {
            dllUrl = WARFRAME_CDN_HOST + line.substr(0, line.find(','));
            break;
        }
    }
    if (dllUrl.empty()) {
        return OodleHandlerResult::INDEX_ERROR;
    }

    outStrm = std::stringstream();
    ret = ParseData(dllUrl.c_str(), outStrm);
    if (ret != OodleHandlerResult::LOADED) {
        return ret;
    }

    std::ofstream dllFp((cachePath / OODLE_DLL_NAME).string().c_str(), std::ios::binary | std::ios::trunc);
    if (dllFp.bad()) {
        return OodleHandlerResult::CANNOT_WRITE;
    }
    dllFp << outStrm.rdbuf();
    dllFp.close();

    loadRet = LoadDll(cachePath / OODLE_DLL_NAME);
    if (!loadRet) {
        alreadyLoaded = true;
        return OodleHandlerResult::LOADED;
    }
    return OodleHandlerResult::CANNOT_LOAD;
}
