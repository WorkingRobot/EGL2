#include "connection.h"
#include "socket_factory.h"
#include "log.h"

#include <condition_variable>
#include <thread>

#ifdef WIN32
#undef min
#endif

namespace curlion {

static inline LoggerProxy WriteConnectionLog(void* connection_identifier) {
    return Log() << "Connection(" << connection_identifier << "): ";
}
    
    
Connection::Connection() :
    is_running_(false),
    dns_resolve_items_(nullptr),
    request_body_read_length_(0),
    result_(CURL_LAST) {
    
    handle_ = curl_easy_init();
    SetInitialOptions();
}


Connection::~Connection() {
    
    ReleaseDnsResolveItems();
    curl_easy_cleanup(handle_);
}

    
void Connection::SetInitialOptions() {
    
    curl_easy_setopt(handle_, CURLOPT_READFUNCTION, CurlReadBodyCallback);
    curl_easy_setopt(handle_, CURLOPT_READDATA, this);
    curl_easy_setopt(handle_, CURLOPT_SEEKFUNCTION, CurlSeekBodyCallback);
    curl_easy_setopt(handle_, CURLOPT_SEEKDATA, this);
    curl_easy_setopt(handle_, CURLOPT_HEADERFUNCTION, CurlWriteHeaderCallback);
    curl_easy_setopt(handle_, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(handle_, CURLOPT_WRITEFUNCTION, CurlWriteBodyCallback);
    curl_easy_setopt(handle_, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(handle_, CURLOPT_XFERINFOFUNCTION, CurlProgressCallback);
    curl_easy_setopt(handle_, CURLOPT_XFERINFODATA, this);
}

    
void Connection::Start(int timeoutMs) {
    
    if (!is_running_) {
        WillStart();
        CURLcode result = CURL_LAST;

        std::mutex mx;
        std::condition_variable cv;
        auto performThread = std::thread([&](CURL* handle, std::chrono::steady_clock::time_point expireTime) {
            auto res = curl_easy_perform(handle);
            if (std::chrono::steady_clock::now() < expireTime) {
                result = res;
                cv.notify_all();
            }
            else {
                curl_easy_cleanup(handle);
            }
        }, handle_, std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs));
        performThread.detach();

        std::unique_lock<std::mutex> lk(mx);
        if (cv.wait_for(lk, std::chrono::milliseconds(timeoutMs), [&] { return result != CURL_LAST; })) {
            DidFinish(result);
        }
        else {
            DidFinish(CURLE_OPERATION_TIMEDOUT);
            TerminateThread(performThread.native_handle(), 1);
        }
    }
}
    
void Connection::ResetOptions() {
    
    if (!is_running_) {
        curl_easy_reset(handle_);
        SetInitialOptions();
        ResetOptionResources();
    }
}
    
    
void Connection::ResetOptionResources() {
    
    ReleaseDnsResolveItems();
    
    request_body_.clear();
    request_body_read_length_ = 0;
    
    read_body_callback_ = nullptr;
    seek_body_callback_ = nullptr;
    write_header_callback_ = nullptr;
    write_body_callback_ = nullptr;
    progress_callback_ = nullptr;
    debug_callback_ = nullptr;
    finished_callback_ = nullptr;
}

    
void Connection::ReleaseDnsResolveItems() {
    
    if (dns_resolve_items_ != nullptr) {
        curl_slist_free_all(dns_resolve_items_);
        dns_resolve_items_ = nullptr;
    }
}
    

void Connection::SetVerbose(bool verbose) {
    curl_easy_setopt(handle_, CURLOPT_VERBOSE, verbose);
}


void Connection::SetUrl(const std::string& url) {
    curl_easy_setopt(handle_, CURLOPT_URL, url.c_str());
}


void Connection::SetProxy(const std::string& proxy) {
    curl_easy_setopt(handle_, CURLOPT_PROXY, proxy.c_str());
}


void Connection::SetProxyAccount(const std::string& username, const std::string& password) {
    curl_easy_setopt(handle_, CURLOPT_PROXYUSERNAME, username.c_str());
    curl_easy_setopt(handle_, CURLOPT_PROXYPASSWORD, password.c_str());
}


void Connection::SetConnectOnly(bool connect_only) {
    curl_easy_setopt(handle_, CURLOPT_CONNECT_ONLY, connect_only);
}
    
    
void Connection::SetDnsResolveItems(const std::multimap<std::string, std::string>& resolve_items) {
    
    ReleaseDnsResolveItems();
    
    for (const auto& each_pair : resolve_items) {
      
        std::string item_string;
        if (each_pair.second.empty()) {
            item_string.append(1, '-');
            item_string.append(each_pair.first);
        }
        else {
            item_string.append(each_pair.first);
            item_string.append(1, ':');
            item_string.append(each_pair.second);
        }
        dns_resolve_items_ = curl_slist_append(dns_resolve_items_, item_string.c_str());
    }
    
    curl_easy_setopt(handle_, CURLOPT_RESOLVE, dns_resolve_items_);
}

    
void Connection::SetVerifyCertificate(bool verify) {
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYPEER, verify);
}

void Connection::SetVerifyHost(bool verify) {
    curl_easy_setopt(handle_, CURLOPT_SSL_VERIFYHOST, verify ? 2 : 0);
}

void Connection::SetCertificateFilePath(const std::string& file_path) {
    const char* path = file_path.empty() ? nullptr : file_path.c_str();
    curl_easy_setopt(handle_, CURLOPT_CAINFO, path);
}

void Connection::SetReceiveBody(bool receive_body) {
    curl_easy_setopt(handle_, CURLOPT_NOBODY, ! receive_body);
}

void Connection::SetEnableProgress(bool enable) {
    curl_easy_setopt(handle_, CURLOPT_NOPROGRESS, ! enable);
}

void Connection::SetConnectTimeoutInMilliseconds(long milliseconds) {
    curl_easy_setopt(handle_, CURLOPT_CONNECTTIMEOUT_MS, milliseconds);
}

void Connection::SetLowSpeedTimeout(long low_speed_in_bytes_per_seond, long timeout_in_seconds) {
    curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_LIMIT, low_speed_in_bytes_per_seond);
    curl_easy_setopt(handle_, CURLOPT_LOW_SPEED_TIME, timeout_in_seconds);
}

void Connection::SetTimeoutInMilliseconds(long milliseconds) {
    curl_easy_setopt(handle_, CURLOPT_TIMEOUT_MS, milliseconds);
}

void Connection::SetDebugCallback(const DebugCallback& callback) {
    
    debug_callback_ = callback;
    
    if (debug_callback_ != nullptr) {
        curl_easy_setopt(handle_, CURLOPT_DEBUGFUNCTION, CurlDebugCallback);
        curl_easy_setopt(handle_, CURLOPT_DEBUGDATA, this);
    }
    else {
        curl_easy_setopt(handle_, CURLOPT_DEBUGFUNCTION, nullptr);
        curl_easy_setopt(handle_, CURLOPT_DEBUGDATA, nullptr);
    }
}

void Connection::WillStart() {
    
    is_running_ = true;
    ResetResponseStates();
}


void Connection::ResetResponseStates() {
    
    request_body_read_length_ = 0;
    result_ = CURL_LAST;
    response_header_.clear();
    response_body_.clear();
}


void Connection::DidFinish(CURLcode result) {
    
    is_running_ = false;
    result_ = result;
    
    if (finished_callback_) {
        finished_callback_(this->shared_from_this());
    }
}

    
void Connection::Clone()
{
    handle_ = curl_easy_duphandle(handle_);
}

long Connection::GetResponseCode() const {
    
    long response_code = 0;
    curl_easy_getinfo(handle_, CURLINFO_RESPONSE_CODE, &response_code);
    return response_code;
}
    

bool Connection::ReadBody(char* body, std::size_t expected_length, std::size_t& actual_length) {
    
    WriteConnectionLog(this) << "Read body to buffer with size " << expected_length << '.';
    
    bool is_succeeded = false;
    
    if (read_body_callback_) {
        is_succeeded = read_body_callback_(this->shared_from_this(), body, expected_length, actual_length);
    }
    else {
    
        std::size_t remain_length = request_body_.length() - request_body_read_length_;
        actual_length = std::min(remain_length, expected_length);
        
        std::memcpy(body, request_body_.data() + request_body_read_length_, actual_length);
        request_body_read_length_ += actual_length;
        
        is_succeeded = true;
    }
    
    if (is_succeeded) {
        WriteConnectionLog(this) << "Read body done with size " << actual_length << '.';
    }
    else {
        WriteConnectionLog(this) << "Read body failed.";
    }
    
    return is_succeeded;
}


bool Connection::SeekBody(SeekOrigin origin, curl_off_t offset) {
 
    WriteConnectionLog(this)
        << "Seek body from "
        << (origin == SeekOrigin::End ? "end" :
           (origin == SeekOrigin::Current ? "current" : "begin"))
        << " to offset " << offset << '.';

    bool is_succeeded = false;
    
    if (read_body_callback_) {
        
        if (seek_body_callback_) {
            is_succeeded = seek_body_callback_(this->shared_from_this(), origin, offset);
        }
    }
    else {

        std::size_t original_position = 0;
        switch (origin) {
            case SeekOrigin::Begin:
                break;
            case SeekOrigin::Current:
                original_position = request_body_read_length_;
                break;
            case SeekOrigin::End:
                original_position = request_body_.length();
                break;
            default:
                //Shouldn't reach here.
                break;
        }
        
        std::size_t new_read_length = original_position + offset;
        if (new_read_length <= request_body_.length()) {
            request_body_read_length_ = new_read_length;
            is_succeeded = true;
        }
    }
    
    WriteConnectionLog(this) << "Seek body " << (is_succeeded ? "done" : "failed") << '.';
    
    return is_succeeded;
}


bool Connection::WriteHeader(const char* header, std::size_t length) {
    
    WriteConnectionLog(this) << "Write header with size " << length << '.';
    
    bool is_succeeded = false;
    
    if (write_header_callback_) {
        is_succeeded = write_header_callback_(this->shared_from_this(), header, length);
    }
    else {
        response_header_.append(header, length);
        is_succeeded = true;
    }
    
    WriteConnectionLog(this) << "Write header " << (is_succeeded ? "done" : "failed") << '.';
    
    return is_succeeded;
}


bool Connection::WriteBody(const char* body, std::size_t length) {

    WriteConnectionLog(this) << "Write body with size " << length << '.';

    bool is_succeeded = false;

    if (write_body_callback_) {
        is_succeeded = write_body_callback_(this->shared_from_this(), body, length);
    }
    else {
        response_body_.append(body, length);
        is_succeeded = true;
    }
    
    WriteConnectionLog(this) << "Write body " << (is_succeeded ? "done" : "failed") << '.';

    return is_succeeded;
}
    
    
bool Connection::Progress(curl_off_t total_download,
                          curl_off_t current_download,
                          curl_off_t total_upload,
                          curl_off_t current_upload) {
    
    WriteConnectionLog(this) << "Progress meter. "
        << current_download << " downloaded, " << total_download << " expected; "
        << current_upload << " uploaded, " << total_upload << " expected.";
    
    bool is_succeeded = true;
    
    if (progress_callback_) {
        
        is_succeeded = progress_callback_(this->shared_from_this(),
                                          total_download,
                                          current_download,
                                          total_upload,
                                          current_upload);
    }
    
    if (! is_succeeded) {
        WriteConnectionLog(this) << "Aborted by progress meter.";
    }
    
    return is_succeeded;
}


void Connection::Debug(curlion::Connection::DebugDataType data_type, const char *data, std::size_t size) {
    
    if (debug_callback_ != nullptr) {
        debug_callback_(shared_from_this(), data_type, data, size);
    }
}

    
size_t Connection::CurlReadBodyCallback(char* buffer, size_t size, size_t nitems, void* instream) {
    Connection* connection = static_cast<Connection*>(instream);
    std::size_t actual_read_length = 0;
    bool is_succeeded = connection->ReadBody(buffer, size * nitems, actual_read_length);
    return is_succeeded ? actual_read_length : CURL_READFUNC_ABORT;
}

int Connection::CurlSeekBodyCallback(void* userp, curl_off_t offset, int origin) {
    SeekOrigin seek_origin = SeekOrigin::Begin;
    switch (origin) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            seek_origin = SeekOrigin::Current;
            break;
        case SEEK_END:
            seek_origin = SeekOrigin::End;
        default:
            return 1;
    }
    Connection* connection = static_cast<Connection*>(userp);
    bool is_succeeded = connection->SeekBody(seek_origin, offset);
    return is_succeeded ? 0 : 1;
}

size_t Connection::CurlWriteHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::size_t length = size * nitems;
    Connection* connection = static_cast<Connection*>(userdata);
    bool is_succeeded = connection->WriteHeader(buffer, length);
    return is_succeeded ? length : 0;
}

size_t Connection::CurlWriteBodyCallback(char* ptr, size_t size, size_t nmemb, void* v) {
    std::size_t length = size * nmemb;
    Connection* connection = static_cast<Connection*>(v);
    bool is_succeeded = connection->WriteBody(ptr, length);
    return is_succeeded ? length : 0;
}
    
int Connection::CurlProgressCallback(void *clientp,
                                     curl_off_t dltotal,
                                     curl_off_t dlnow,
                                     curl_off_t ultotal,
                                     curl_off_t ulnow) {
    
    Connection* connection = static_cast<Connection*>(clientp);
    bool is_succeeded = connection->Progress(dltotal, dlnow, ultotal, ulnow);
    return is_succeeded ? 0 : -1;
}
    
int Connection::CurlDebugCallback(CURL* handle,
                                  curl_infotype type,
                                  char* data,
                                  size_t size,
                                  void* userptr) {
    
    DebugDataType data_type = DebugDataType::Information;
    switch (type) {
        case CURLINFO_TEXT:
            break;
        case CURLINFO_HEADER_IN:
            data_type = DebugDataType::ReceivedHeader;
            break;
        case CURLINFO_HEADER_OUT:
            data_type = DebugDataType::SentHeader;
        case CURLINFO_DATA_IN:
            data_type = DebugDataType::ReceivedBody;
            break;
        case CURLINFO_DATA_OUT:
            data_type = DebugDataType::SentBody;
            break;
        case CURLINFO_SSL_DATA_IN:
            data_type = DebugDataType::ReceivedSslData;
            break;
        case CURLINFO_SSL_DATA_OUT:
            data_type = DebugDataType::SentSslData;
            break;
        default:
            break;
    }
    
    Connection* connection = static_cast<Connection*>(userptr);
    connection->Debug(data_type, data, size);
    return 0;
}

}
