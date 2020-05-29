#pragma once

#include <functional>
#include <curl/curl.h>

namespace curlion {

/**
 SocketWatcher is an interface used by ConnectionManager to trigger a notification when a socket 
 is ready to read or write data.
 */
class SocketWatcher {
public:
    /**
     Socket's event type to be watched.
     */
    enum class Event {
        
        /**
         Read event.
         */
        Read,
        
        /**
         Write event.
         */
        Write,
        
        /**
         Both read and write event.
         */
        ReadWrite,
    };
    
    /**
     Callback prototype for the socket event.
     
     @param socket 
        Socket handle triggers the event.
     
     @param can_write
        Indicates the event type. True for write, false for read.
     */
    typedef std::function<void(curl_socket_t socket, bool can_write)> EventCallback;
    
public:
    SocketWatcher() { }
    virtual ~SocketWatcher() { }
    
    /**
     Start watching a socket for specific event.
     
     The watching should be continually, until a corresponding StopWatching is called. callback should
     be called when the event on socket is triggered every time.
     
     Is is guaranteed that Watch would not be call for the second time unless StopWatching is called.
     
     The implementation must ensure that the thread calling callback is the same one calling Watch.
     */
    virtual void Watch(curl_socket_t socket, Event event, const EventCallback& callback) = 0;
    
    /**
     Stop watching a socket's event.
     
     Once StopWatching is called, no callback should be called any more.
     Note that the socket may be closed before passed to StopWatching. Check for this case if it matters.
     */
    virtual void StopWatching(curl_socket_t socket) = 0;
    
private:
    SocketWatcher(const SocketWatcher&) = delete;
    SocketWatcher& operator=(const SocketWatcher&) = delete;
};

}