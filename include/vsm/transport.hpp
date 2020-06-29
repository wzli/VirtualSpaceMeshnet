#pragma once
#include <chrono>
#include <functional>

namespace vsm {

class Transport {
public:
    using ReceiverCallback = std::function<void(const void* buffer, size_t len)>;
    using TimerCallback = std::function<void(int timer_id)>;

    virtual const std::string& getAddress() const = 0;

    // these 3 functions are expected to be thread-safe
    virtual int connect(const std::string& dst_addr) = 0;
    virtual int disconnect(const std::string& dst_addr) = 0;
    virtual int transmit(const void* buffer, size_t len, const std::string& group = "") = 0;

    virtual int addReceiver(ReceiverCallback receiver_callback, const std::string& group = "") = 0;
    virtual int addTimer(std::chrono::milliseconds interval, TimerCallback timer_callback) = 0;

    virtual int poll(std::chrono::milliseconds timeout) = 0;  // -1 = inf, 0 = non-blocking
};

}  // namespace vsm
