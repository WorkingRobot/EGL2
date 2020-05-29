#pragma once

#include <functional>

namespace curlion {

/**
 Timer is an interface used by ConnectionManager to trigger a notification after a period of time.
 */
class Timer {
public:
    Timer() { }
    virtual ~Timer() { }
    
    /**
     Start the timer.
     
     callback should be called after timeout_ms milliseconds. In case of timeout_ms is 0,
     call callback as soon as possible.
     
     Is is guaranteed that Start would not be call for the second time unless Stop is called.
     
     The implementation must ensure that the thread calling callback is the same one calling Start.
     */
    virtual void Start(long timeout_ms, const std::function<void()>& callback) = 0;
    
    /**
     Stop the timer.
     
     Once Stop is called, no callback should be called any more.
     */
    virtual void Stop() = 0;
    
private:
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
};

}