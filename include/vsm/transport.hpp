#pragma once
#include <functional>

namespace vsm {

class Transport {
public:
    using ReceiverCallback = std::function<void(const void* buffer, size_t len)>;
    using TimerCallback = std::function<void(int timer_id)>;

    virtual const char* getAddress() const = 0;

    // these 3 functions are expected to be thread-safe
    virtual int connect(const char* dst_addr) = 0;
    virtual int disconnect(const char* dst_addr) = 0;
    virtual int transmit(const void* buffer, size_t len, const char* group = "") = 0;

    virtual int addReceiver(ReceiverCallback receiver_callback, const char* group = "") = 0;
    virtual int addTimer(size_t interval_ms, TimerCallback timer_callback) = 0;

    virtual int poll(size_t timeout_ms) = 0;  // -1 = inf, 0 = non-blocking
};

}  // namespace vsm
