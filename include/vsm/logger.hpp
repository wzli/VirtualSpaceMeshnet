#pragma once
#include <map>
#include <functional>

namespace vsm {

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

    using LogHandler = std::function<void(Level, const char*, int, const void*, size_t)>;

    void addLogHandler(Level level, LogHandler log_handler) {
        _log_handlers.insert({level, std::move(log_handler)});
    };

    void log(Level level, const char* message, int type = 0, const void* data = nullptr,
            size_t data_len = 0) {
        for (const auto log_handler : _log_handlers) {
            if (log_handler.first > level) {
                break;
            }
            log_handler.second(level, message, type, data, data_len);
        }
    };

private:
    std::multimap<Level, LogHandler> _log_handlers;
};

}  // namespace vsm
