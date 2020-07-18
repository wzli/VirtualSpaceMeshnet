#pragma once

#include <zmq.h>
#include <vector>
#include <functional>

namespace zmq {

class ttimers_t {
public:
    ttimers_t()
            : _timers(zmq_timers_new()) {}

    ~ttimers_t() { zmq_timers_destroy(&_timers); }

    int add(size_t interval, std::function<void(int)> handler) {
        _callbacks.emplace_back(std::move(handler));
        return zmq_timers_add(_timers, interval, callback_wrapper, this);
    }

    int cancel(int timer_id) { return zmq_timers_cancel(_timers, timer_id); }

    int set_interval(int timer_id, size_t interval) {
        return zmq_timers_set_interval(_timers, timer_id, interval);
    }

    int reset(int timer_id) { return zmq_timers_reset(_timers, timer_id); }

    long timeout() { return zmq_timers_timeout(_timers); }

    int execute() { return zmq_timers_execute(_timers); }

private:
    static void callback_wrapper(int timer_id, void* arg) {
        reinterpret_cast<ttimers_t*>(arg)->_callbacks[timer_id - 1](timer_id);
    }
    std::vector<std::function<void(int)>> _callbacks;
    void* _timers;
};

}  // namespace zmq
