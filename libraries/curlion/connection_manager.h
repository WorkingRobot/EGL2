#pragma once

#include <map>
#include <memory>
#include <system_error>
#include <curl/curl.h>

namespace curlion {

class Connection;
class SocketFactory;
class SocketWatcher;
class Timer;

/**
 ConnectionManager manages connections' executions, including starting or 
 aborting connection.
 
 Call SartConnection method to start a connection. Call AbortConnection 
 method to stop a running connection.
 
 This class is not thread safe.
 
 This is a encapsulation against libcurl's multi handle.
 */
class ConnectionManager {
public:
    /**
     Construct the ConnectionManager instance.
     */
    ConnectionManager(const std::shared_ptr<SocketFactory>& socket_factory,
                      const std::shared_ptr<SocketWatcher>& socket_watcher,
                      const std::shared_ptr<Timer>& timer);
    
    /**
     Destruct the ConnectionManager instance.
     
     When this method got called, all running connections would be aborted.
     */
    ~ConnectionManager();
    
    /**
     Start a connection.
     
     @param connection
         The connection to start. Must not be nullptr.
     
     @return
         Return an error on failure.
     
     This method will retain the connection, until it is finished or aborted.
     
     It is OK to call this method with the same Connection instance multiple times.
     Nothing changed if the connection is running; Otherwise it will be restarted.
     */
    std::error_condition StartConnection(const std::shared_ptr<Connection>& connection);
    
    /**
     Abort a running connection.
     
     @param connection
         The connection to abort. Must not be nullptr.
     
     @return
         Return an error on failure.
     
     Note that the connection's finished callback would not be triggered when it is
     aborted.
     
     Is is OK to call this methods while the connection is not running, nothing
     would happend.
     
     If this method fails, the connection is in an unknown condition, it should be 
     abondaned and never be reused again.
     */
    std::error_condition AbortConnection(const std::shared_ptr<Connection>& connection);
    
    /**
     Get the underlying multi handle.
     */
    CURLM* GetHandle() const {
        return multi_handle_;
    }
    
private:
    static curl_socket_t CurlOpenSocketCallback(void* clientp, curlsocktype socket_type, curl_sockaddr* address);
    static int CurlCloseSocketCallback(void* clientp, curl_socket_t socket);
    static int CurlTimerCallback(CURLM* multi_handle, long timeout_ms, void* user_pointer);
    static int CurlSocketCallback(CURL* easy_handle,
                                  curl_socket_t socket,
                                  int action,
                                  void* user_pointer,
                                  void* socket_pointer);
    
    curl_socket_t OpenSocket(curlsocktype socket_type, curl_sockaddr* address);
    bool CloseSocket(curl_socket_t socket);
    
    void SetTimer(long timeout_ms);
    void TimerTriggered();
    
    void WatchSocket(curl_socket_t socket, int action, void* socket_pointer);
    void SocketEventTriggered(curl_socket_t socket, bool can_write);
    
    void CheckFinishedConnections();
    
private:
    ConnectionManager(const ConnectionManager&) = delete;
    ConnectionManager& operator=(const ConnectionManager&) = delete;
    
private:
    std::shared_ptr<SocketFactory> socket_factory_;
    std::shared_ptr<SocketWatcher> socket_watcher_;
    std::shared_ptr<Timer> timer_;
    
    CURLM* multi_handle_;
    std::map<CURL*, std::shared_ptr<Connection>> running_connections_;
};

}
