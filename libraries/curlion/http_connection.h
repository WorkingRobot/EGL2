#pragma once

#include <map>
#include <string>
#include "connection.h"

namespace curlion {

class HttpForm;
    
/**
 HttpConnection used to send HTTP request and received HTTP response.
 
 This class dervies from Connection, adds some setter and getter methods speicfic to HTTP.
 */
class HttpConnection : public Connection {
public:
    /**
     Construct the HttpConnection instance.
     */
    HttpConnection();
    
    /**
     Destruct the HttpConnection instance.
     */
    ~HttpConnection();
    
    /**
     Set whether to use HTTP POST method.
     
     The default is false, means using GET method.
     */
    void SetUsePost(bool use_post);
    
    /**
     Set HTTP request headers.
     
     The new headers would replace all headers previously set.
     */
    void SetRequestHeaders(const std::multimap<std::string, std::string>& headers);
    
    /**
     Add a single HTTP request header.
     */
    void AddRequestHeader(const std::string& field, const std::string& value);
    
    /**
     Set a request form for HTTP POST.
     
     The default is nullptr.
     */
    void SetRequestForm(const std::shared_ptr<HttpForm>& form);
    
    /**
     Set whether to auto-redirect when received HTTP 3xx response.
     
     Use SetMaxAutoRedirectCount to limit the redirction count.
     
     The default is false.
     */
    void SetAutoRedirect(bool auto_redirect);
    
    /**
     Set maximum number of auto-redirects allowed.
     
     Set to 0 to forbidden any redirect.
     
     The default is -1, meas no redirect limits.
     */
    void SetMaxAutoRedirectCount(long count);
    
    /**
     Get HTTP response headers.
     
     This is a wrapper method for GetResponseHeader, parses the raw header string to key value pairs.
     Note that when auto-redirection is enabled, all headers in multiple responses would be contained.
     */
    const std::multimap<std::string, std::string>& GetResponseHeaders() const;
    
protected:
    void ResetResponseStates() override;
    void ResetOptionResources() override;
    
private:
    void ParseResponseHeaders() const;
    void ReleaseRequestHeaders();
    
private:
    curl_slist* request_headers_;
    std::shared_ptr<HttpForm> form_;
    mutable bool has_parsed_response_headers_;
    mutable std::multimap<std::string, std::string> response_headers_;
};

}