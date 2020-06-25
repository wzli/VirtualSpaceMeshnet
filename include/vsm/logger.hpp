#pragma once
#include <chrono>
#include <exception>
#include <functional>
#include <map>

#define STRERR(e) #e, e

#define IF_PTR(ptr, func, ...) \
    if (ptr)                   \
    ptr->func(__VA_ARGS__)

#define IF_FUNC(func, ...) \
    if (func)              \
    func(__VA_ARGS__)

namespace vsm {

using msecs = std::chrono::milliseconds;

struct Error : public std::exception {
    Error(const char* err_msg, int err_type = 0, int err_code = 0)
            : msg(err_msg)
            , type(err_type)
            , code(err_code) {}

    virtual const char* what() const noexcept { return msg; }

    const char* msg;
    int type;
    int code;
};

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

    using LogHandler = std::function<void(msecs time, Level, Error error, const void*, size_t)>;

    std::function<msecs(void)>& getClock() { return _clock; }

    void addLogHandler(Level level, LogHandler log_handler) {
        if (log_handler) {
            _log_handlers.insert({level, std::move(log_handler)});
        }
    }

    void log(Level level, Error error, const void* data = nullptr, size_t data_len = 0) const {
        static const auto start_time = std::chrono::steady_clock::now();
        auto time = _clock ? _clock()
                           : std::chrono::duration_cast<msecs>(
                                     std::chrono::steady_clock::now() - start_time);
        for (auto handler = _log_handlers.begin();
                handler != _log_handlers.end() && handler->first <= level; ++handler) {
            handler->second(time, level, error, data, data_len);
        }
    };

private:
    std::function<msecs(void)> _clock;
    std::multimap<Level, LogHandler> _log_handlers;
};

}  // namespace vsm
