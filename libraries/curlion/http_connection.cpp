#include "http_connection.h"
#include <vector>
#include "http_form.h"

namespace curlion {

static std::string MakeHttpHeaderLine(const std::string& field, const std::string& value);
static std::vector<std::string> SplitString(const std::string& string, const std::string& delimiter);


HttpConnection::HttpConnection() :
    request_headers_(nullptr),
    has_parsed_response_headers_(false) {
    
}


HttpConnection::~HttpConnection() {
    ReleaseRequestHeaders();
}


void HttpConnection::SetUsePost(bool use_post) {
    curl_easy_setopt(GetHandle(), CURLOPT_POST, use_post);
}


void HttpConnection::SetRequestHeaders(const std::multimap<std::string, std::string>& headers) {
    
    ReleaseRequestHeaders();
    
    for (auto& each_header : headers) {
        
        std::string each_header_line = MakeHttpHeaderLine(each_header.first, each_header.second);
        request_headers_ = curl_slist_append(request_headers_, each_header_line.c_str());
    }
    
    curl_easy_setopt(GetHandle(), CURLOPT_HTTPHEADER, request_headers_);
}


void HttpConnection::AddRequestHeader(const std::string& field, const std::string& value) {

    std::string header_line = MakeHttpHeaderLine(field, value);
    request_headers_ = curl_slist_append(request_headers_, header_line.c_str());
    
    curl_easy_setopt(GetHandle(), CURLOPT_HTTPHEADER, request_headers_);
}

    
void HttpConnection::SetRequestForm(const std::shared_ptr<HttpForm>& form) {
    
    form_ = form;
    
    curl_httppost* handle = nullptr;
    if (form_ != nullptr) {
        handle = form_->GetHandle();
    }

    curl_easy_setopt(GetHandle(), CURLOPT_HTTPPOST, handle);
}

    
void HttpConnection::SetAutoRedirect(bool auto_redirect) {
    curl_easy_setopt(GetHandle(), CURLOPT_FOLLOWLOCATION, auto_redirect);
}


void HttpConnection::SetMaxAutoRedirectCount(long count) {
    curl_easy_setopt(GetHandle(), CURLOPT_MAXREDIRS, count);
}


const std::multimap<std::string, std::string>& HttpConnection::GetResponseHeaders() const {
    
    if (! has_parsed_response_headers_) {
        ParseResponseHeaders();
        has_parsed_response_headers_ = true;
    }
    
    return response_headers_;
}


void HttpConnection::ParseResponseHeaders() const {
    
    std::vector<std::string> lines = SplitString(GetResponseHeader(), "\r\n");
    
    std::vector<std::string> key_value_pair;
    for (auto& each_line : lines) {
        
        key_value_pair = SplitString(each_line, ": ");
        if (key_value_pair.size() < 2) {
            continue;
        }
        
        response_headers_.insert(std::make_pair(key_value_pair.at(0), key_value_pair.at(1)));
    }
}


void HttpConnection::ResetResponseStates() {
    
    Connection::ResetResponseStates();
    
    has_parsed_response_headers_ = false;
    response_headers_.clear();
}
    
    
void HttpConnection::ResetOptionResources() {
    
    Connection::ResetOptionResources();
    
    ReleaseRequestHeaders();
    form_.reset();
}
    
    
void HttpConnection::ReleaseRequestHeaders() {
    
    if (request_headers_ != nullptr) {
        curl_slist_free_all(request_headers_);
        request_headers_ = nullptr;
    }
}


static std::string MakeHttpHeaderLine(const std::string& field, const std::string& value) {
    
    std::string header_line = field;
    header_line.append(": ");
    header_line.append(value);
    
    return header_line;
}


static std::vector<std::string> SplitString(const std::string& string, const std::string& delimiter) {
    
    std::vector<std::string> splitted_strings;
    
    std::size_t begin_index = 0;
    std::size_t end_index = 0;
    
    while (begin_index < string.length()) {
    
        end_index = string.find(delimiter, begin_index);
        
        if (end_index == std::string::npos) {
            end_index = string.length();
        }
        
        splitted_strings.push_back(string.substr(begin_index, end_index - begin_index));
    
        begin_index = end_index + delimiter.length();
    }
    
    return splitted_strings;
}

}