#pragma once

#include <memory>
#include <curl/curl.h>

namespace curlion {

/**
 SocketFactory is an interface used by Connection to open and close sockets.
 */
class SocketFactory {
public:
    SocketFactory() { }
    virtual ~SocketFactory() { }
    
    /**
     Open a socket for specific type and address.
     
     Return a valid socket handle if succeeded; return CURL_SOCKET_BAD otherwise.
     */
    virtual curl_socket_t Open(curlsocktype socket_type, const curl_sockaddr* address) = 0;
    
    /**
     Close a socket.
     
     Return a bool indicates that whether successful.
     */
    virtual bool Close(curl_socket_t socket) = 0;
    
private:
    SocketFactory(const SocketFactory&) = delete;
    SocketFactory& operator=(const SocketFactory&) = delete;
};

}