#pragma once
#include <map>
#include <functional>
#include <exception>

namespace vsm {

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
        FATAL,
        ERROR,
        WARN,
        INFO,
        DEBUG,
        TRACE,
        N_LEVELS,
    };

    using LogHandler = std::function<void(Level, Error error, const void*, size_t)>;

    void addLogHandler(Level level, LogHandler log_handler) {
        _log_handlers.insert({level, std::move(log_handler)});
    };

    void log(Level level, Error error, const void* data = nullptr, size_t data_len = 0) {
        for (const auto log_handler : _log_handlers) {
            if (log_handler.first > level) {
                break;
            }
            log_handler.second(level, error, data, data_len);
        }
    };

private:
    std::multimap<Level, LogHandler> _log_handlers;
};

}  // namespace vsm
