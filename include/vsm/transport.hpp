#pragma once
#include <functional>

namespace vsm {

class Transport {
public:
    using ReceiverCallback =
            std::function<void(const std::string& src_addr, const void* buffer, size_t len)>;
    using TimerCallback = std::function<void(int timer_id)>;

    virtual int connect(const std::string& dst_addr) = 0;
    virtual int disconnect(const std::string& dst_addr) = 0;

    virtual int transmit(const void* buffer, size_t len, const std::string& group) = 0;

    virtual int addReceiver(ReceiverCallback receiver_callback, const std::string& group) = 0;
    virtual int addTimer(size_t interval_ms, TimerCallback timer_callback) = 0;
};

}  // namespace vsm
