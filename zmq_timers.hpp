#pragma once

#include <zmq.h>
#include <functional>

namespace zmq {

class timers_t {
public:
    timers_t()
            : _timers(zmq_timers_new()) {}

    ~timers_t() { zmq_timers_destroy(&_timers); }

    int add(size_t interval, const std::function<void(int)>& handler) {
        return zmq_timers_add(_timers, interval, callback_wrapper, (void*) &handler);
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
        reinterpret_cast<std::function<void(int)>*>(arg)->operator()(timer_id);
    }
    void* _timers;
};

}  // namespace zmq
