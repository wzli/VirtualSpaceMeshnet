#pragma once
#include <chrono>
#include <functional>
#include <map>

#define STRERR(e) #e, e

#define IF_PTR(ptr, func, ...) \
    if (ptr)                   \
    ptr->func(__VA_ARGS__)

namespace vsm {

struct Error {
    const char* msg;
    int type = 0;
    int code = 0;
};

template <class Duration>
Duration getNow() {
    return std::chrono::duration_cast<Duration>(
            std::chrono::steady_clock::now().time_since_epoch());
}

class Logger {
public:
    enum Level {
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        N_LEVELS,
    };

    using LogHandler = std::function<void(int64_t time, Level, Error error, const void*, size_t)>;

    std::function<int64_t(void)>& getClock() { return _clock; }

    void addLogHandler(Level level, LogHandler log_handler) {
        if (log_handler) {
            _log_handlers.insert({level, std::move(log_handler)});
        }
    }

    void log(Level level, Error error, const void* data = nullptr, size_t data_len = 0) const {
        int64_t time =
                _clock ? _clock() : getNow<std::chrono::milliseconds>().count() - _start_time;
        for (auto handler = _log_handlers.begin();
                handler != _log_handlers.end() && handler->first <= level; ++handler) {
            handler->second(time, level, error, data, data_len);
        }
    };

private:
    const int64_t _start_time = getNow<std::chrono::milliseconds>().count();
    std::function<int64_t(void)> _clock;
    std::multimap<Level, LogHandler> _log_handlers;
};

}  // namespace vsm
