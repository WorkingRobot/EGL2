#pragma once

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <curl/curl.h>

namespace curlion {

/**
 Connection used to send request to remote peer and receive its response.
 It supports variety of network protocols, such as SMTP, IMAP and HTTP etc.
 
 Before start a connection, some setter methods must be call to set required 
 options, sush as SetUrl to set the URL connect to, and SetRequestBody to set
 the body sent to remote peer.
 
 There are two ways to start a connection. The simplest one is to call 
 Connection::Start, this method would not return until the connection finishes.
 
 The more complicated way is to call ConnectionManager::StartConnection to start
 a non-blocking connection. You should call SetFinishedCallback to set a callback
 in order to receive a notification when the connection finishes.

 No mater which way you choose, when a connection is finish, you can call getter 
 methods to get the result, such as GetResult to get the result code, and 
 GetResponseBody to get the body received from remote peer.
 
 For HTTP, there is a derived class HttpConnection provides setter and getter
 methods specific to HTTP.
 
 This is a encapsulation against libcurl's easy handle.
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    /**
     Original position for seeking request body.
     */
    enum class SeekOrigin {
        /**
         Begin of request body.
         */
        Begin,
        
        /**
         Current position of request body.
         */
        Current,
        
        /**
         End of request body.
         */
        End,
    };
    
    /**
     Callback prototype for seeking request body.
     
     @param origin 
        Original position for seeking.
     
     @param offset 
        Offset related to original position.
     
     @return 
        Whether the seeking is succeeded.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             SeekOrigin origin,
             curl_off_t offset)
    > SeekBodyCallback;

    /**
     Calback prototype for reading request body.
     
     @param connection 
        The Connection instance.
     
     @param body 
        Body's buffer. Data need to be copied to here.
     
     @param expected_length 
        How many bytes are expected. Is is also the length of buffer.
     
     @param actual_length 
        Return how many bytes are copied to buffer. It could be less than expected_length.
        Return 0 means whole request body is read.
     
     @return Whether the reading is succeeded. Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             char* body,
             std::size_t expected_length,
             std::size_t& actual_length)
    > ReadBodyCallback;
    
    /**
     Callback prototype for writing response header.
     
     @param connection 
        The Connection instance.
     
     @param header 
        Buffer contains header data.
     
     @param length 
        Buffer's length.
     
     @return 
        Whether the writing is succeeded. Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             const char* header,
             std::size_t length)
    > WriteHeaderCallback;
    
    /**
     Callback prototype for writing response body.
     
     @param connection 
        The Connection instance.
     
     @param body
        Buffer contains body data.
     
     @param length
        Buffer's length
     
     @return 
        Whether the writing is succeeded. Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             const char* body,
             std::size_t length)
    > WriteBodyCallback;
    
    /**
     Callback prototype for progress meter.
     
     @param connection
        The Connection instance.
     
     @param total_download
        The total number of bytes expected to be downloaded.
     
     @param current_download
        The number of bytes downloaded so far.
     
     @param total_upload
        The total number of bytes expected to be uploaded.
     
     @param current_upload
        The number of bytes uploaded so far.
     
     @return
        Return false would abort the connection.
     */
    typedef std::function<
        bool(const std::shared_ptr<Connection>& connection,
             curl_off_t total_download,
             curl_off_t current_download,
             curl_off_t total_upload,
             curl_off_t current_upload)
    > ProgressCallback;
    
    /**
     Debug data type in debug callback.
     */
    enum class DebugDataType {
        
        /**
         The data is informational text.
         */
        Information,
        
        /**
         The data is header received from the peer.
         */
        ReceivedHeader,
        
        /**
         The data is header sent to the peer.
         */
        SentHeader,
        
        /**
         The data is body received from the peer.
         */
        ReceivedBody,
        
        /**
         The data is body sent to the peer.
         */
        SentBody,
        
        /**
         The data is SSL data received from the peer.
         */
        ReceivedSslData,
        
        /**
         The data is SSL data sent to the peer.
         */
        SentSslData,
    };
    
    /**
     Callback prototype for receiving debug information.
     
     @param connection
         The connection instance.
     
     @param data_type
         The type of data.
     
     @param data
         The debug information data.
     
     @param size
         The size of data.
     */
    typedef std::function<
        void(const std::shared_ptr<Connection>& connection,
             DebugDataType data_type,
             const char* data,
             std::size_t size)
    > DebugCallback;
    
    /**
     Callback prototype for connection finished.
     
     @param connection
        The Connection instance.
     */
    typedef std::function<void(const std::shared_ptr<Connection>& connection)> FinishedCallback;
    
public:
    /**
     Construct the Connection instance.
     */
    Connection();
    
    /**
     Destruct the Connection instance.
     
     Destructing a running connection will abort it immediately.
     */
    virtual ~Connection();
    
    /**
     Start the connection in blocking manner.
     
     This method does not return a value, you should call GetResult method to get the result.
     If the connection is already started, this method takes no effects.
     Use ConnectionManager to start connections if you wish a non-blocking manner.
     */
    void Start(int timeoutMs);
    
    /**
     Reset all options to default.
     
     Calling this method while the connection is running takes no effect.
     */
    void ResetOptions();
    
    /**
     Set whether to print detail information about connection to stderr.
     
     The default is false.
     */
    void SetVerbose(bool verbose);
    
    /**
     Set the URL used in connection.
     */
    void SetUrl(const std::string& url);

    /**
     Set the user agent used in connection.
     */
    void SetUserAgent(const std::string& agent);
    
    /**
     Set the proxy used in connection.
     */
    void SetProxy(const std::string& proxy);
    
    /**
     Set authenticated account for proxy.
     */
    void SetProxyAccount(const std::string& username, const std::string& password);
    
    /**
     Set whether to connect to server only, don't tranfer any data.
     
     The default is false.
     */
    void SetConnectOnly(bool connect_only);
    
    /**
     Set custom host name to IP address resolve items to DNS cache.
     
     The first element of each item is a host and port pair, which must be in HOST:PORT format.
     The second element of each item is an IP address, can be either IPv4 or IPv6 format. If IP 
     address is empty, this item is considered to be removed from DNS cache.
     
     Note that resolve items are applied to DNS cache only after the connection starts. That is,
     later set resolve items will replace earlier set items. And once an item is added to DNS cache,
     the only way to remove it is setting another item with the same host and port, along with an 
     empty IP address.
     
     This option is equal to set CURLOPT_RESOLVE option to libcurl.
     */
    void SetDnsResolveItems(const std::multimap<std::string, std::string>& resolve_items);
    
    /**
     Set whether to verify the peer's SSL certificate.
     
     The default is true.
     */
    void SetVerifyCertificate(bool verify);
    
    /**
     Set whether to verify the certificate's name against host.
     
     The default is true.
     */
    void SetVerifyHost(bool verify);
    
    /**
     Set the path of file holding one or more certificates to verify the peer with.
     */
    void SetCertificateFilePath(const std::string& file_path);
    
    /**
     Set the body for request.
     
     Note that the body would be ignored once a callable read body callback is set.
     */
    void SetRequestBody(const std::string& body) {
        request_body_ = body;
    }
    
    /**
     Set whether to receive response body.
     
     This option must be set to false for those responses without body,
     otherwise the connection would be blocked.
     
     The default is true.
     */
    void SetReceiveBody(bool receive_body);
    
    /**
     Set whether to enable the progress meter.
     
     When progress meter is disalbed, the progress callback would not be called.
     
     The default is false.
     */
    void SetEnableProgress(bool enable);
    
    /**
     Set timeout for the connect phase.
     
     Set to 0 to switch to the default timeout 300 seconds.
     */
    void SetConnectTimeoutInMilliseconds(long milliseconds);
    
    /**
     Set timeout for how long the connection can be idle.
     
     This option is a shortcut to SetLowSpeedTimeout, which low speed is set to 1.
     */
    void SetIdleTimeoutInSeconds(long seconds) {
        SetLowSpeedTimeout(1, seconds);
    }
    
    /**
     Set timeout for low speed transfer.
     
     If the average transfer speed is below specified speed for specified duration, the connection is 
     considered timeout.
     
     The default is no timeout. Set 0 to any one of the parameters to turn off the timeout.
     
     This option is equal to set both CURLOPT_LOW_SPEED_LIMIT and CURLOPT_LOW_SPEED_TIME options 
     to libcurl. Note that the timeout may be broken in some cases due to internal implementation 
     of libcurl.
     */
    void SetLowSpeedTimeout(long low_speed_in_bytes_per_seond, long timeout_in_seconds);
    
    /**
     Set timeout for the whole connection.
     
     The default is no timeout. Set 0 to turn off the timeout.
     */
    void SetTimeoutInMilliseconds(long milliseconds);
    
    /**
     Set callback for reading request body.
     
     If callback is callable, the request body set by SetRequestBody would be ignored.
     */
    void SetReadBodyCallback(const ReadBodyCallback& callback) {
        read_body_callback_ = callback;
    }
    
    /**
     Set callback for seeking request body.
     
     This callback is used to re-position the reading pointer while re-sending is needed.
     It should be provided along with read body callback.
     */
    void SetSeekBodyCallback(const SeekBodyCallback& callback) {
        seek_body_callback_ = callback;
    }
    
    /**
     Set callback for writing response header.
     
     If callback is callable, GetResponseHeader would return empty string.
     */
    void SetWriteHeaderCallback(const WriteHeaderCallback& callback) {
        write_header_callback_ = callback;
    }
    
    /**
     Set callback for writing response body.
     
     If callback is callable, GetResponseBody would return empty string.
     */
    void SetWriteBodyCallback(const WriteBodyCallback& callback) {
        write_body_callback_ = callback;
    }
    
    /**
     Set callback for progress meter.
     */
    void SetProgressCallback(const ProgressCallback& callback) {
        progress_callback_ = callback;
    }
    
    /**
     Set callback for receiving debug information.
     
     SetVerbose method must be called with true to enable the debug callback. If a non-null callback
     is set as the debug callback, all verbose output would be sent to debug callback instead of stderr.
     */
    void SetDebugCallback(const DebugCallback& callback);
    
    /**
     Set callback for connection finished.
     
     Use this callback to get informed when the connection finished.
     */
    void SetFinishedCallback(const FinishedCallback& callback) {
        finished_callback_ = callback;
    }
    
    /**
     Get the result code.
     
     An undefined value would be returned if the connection is not yet finished.
     */
    CURLcode GetResult() const {
        return result_;
    }

    /**
     Clone internal connection.

     Options all stay the same, but all connections and handles are different. This doesn't cleanup the old handle!
     https://curl.haxx.se/libcurl/c/curl_easy_duphandle.html
     */
    void Clone();
    
    /**
     Get the last response code.
     
     For HTTP, response code is the HTTP status code.
     
     The return value is undefined if the connection is not yet finished.
     */
    long GetResponseCode() const;
    
    /**
     Get response header.
     
     Note that empty string would be returned if a callable write header callback is set.
     Undefined string content would be returned if the connection is not yet finished.
     */
    const std::string& GetResponseHeader() const {
        return response_header_;
    }
    
    /**
     Get response body.
     
     Note that empty string would be returned if a callable write body callback is set.
     Undefined string content would be returned if the connection is not yet finished.
     */
    const std::string& GetResponseBody() const {
        return response_body_;
    }
    
    /**
     Get the underlying easy handle.
     */
    CURL* GetHandle() const {
        return handle_;
    }
    
//Methods be called from ConnectionManager.
private:
    void WillStart();
    void DidFinish(CURLcode result);
    
protected:
    /**
     Reset response states to default.
     
     This method is called when the connection needs to reset all response states, such as reseting 
     the content of response body to empty. This usually happens when the connection restarts.
     
     Derived classes can override this method the reset their response states, and they must call 
     the same method of base class.
     */
    virtual void ResetResponseStates();
    
    /**
     Reset resources associated with curl options to default.
     
     This method is called when the connection needs to reset all option resources, such as reseting 
     the content of request body to empty.
     
     Derived classes can override this method to reset their option resources, and they must
     call the same method of base class.
     */
    virtual void ResetOptionResources();
    
private:
    static size_t CurlReadBodyCallback(char* buffer, size_t size, size_t nitems, void* instream);
    static int CurlSeekBodyCallback(void* userp, curl_off_t offset, int origin);
    static size_t CurlWriteHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    static size_t CurlWriteBodyCallback(char* ptr, size_t size, size_t nmemb, void* v);
    static int CurlProgressCallback(void *clientp,
                                    curl_off_t dltotal,
                                    curl_off_t dlnow,
                                    curl_off_t ultotal,
                                    curl_off_t ulnow);
    static int CurlDebugCallback(CURL* handle,
                                 curl_infotype type,
                                 char* data,
                                 size_t size,
                                 void* userptr);
  
    void SetInitialOptions();
    void ReleaseDnsResolveItems();
    
    bool ReadBody(char* body, std::size_t expected_length, std::size_t& actual_length);
    bool SeekBody(SeekOrigin origin, curl_off_t offset);
    bool WriteHeader(const char* header, std::size_t length);
    bool WriteBody(const char* body, std::size_t length);
    bool Progress(curl_off_t total_download,
                  curl_off_t current_download,
                  curl_off_t total_upload,
                  curl_off_t current_upload);
    void Debug(DebugDataType data_type, const char* data, std::size_t size);
    
private:
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

private:
    CURL* handle_;
    bool is_running_;
    
    curl_slist* dns_resolve_items_;
    std::string request_body_;
    std::size_t request_body_read_length_;
    ReadBodyCallback read_body_callback_;
    SeekBodyCallback seek_body_callback_;
    WriteHeaderCallback write_header_callback_;
    WriteBodyCallback write_body_callback_;
    ProgressCallback progress_callback_;
    DebugCallback debug_callback_;
    FinishedCallback finished_callback_;
    CURLcode result_;
    std::string response_header_;
    std::string response_body_;
    
    friend class ConnectionManager;
};

}
