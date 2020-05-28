#include <catch2/catch.hpp>
#include <vsm/logger.hpp>

#include <cstring>
#include <iostream>

using namespace vsm;

TEST_CASE("Logger") {
    Logger logger;
    Error test_error("test_msg", 5, 6);
    const void* test_data = test_error.msg;
    size_t test_data_len = 3;

    int log_count[Logger::N_LEVELS] = {0};
    auto add_log_counter = [&](Logger::Level level) {
        logger.addLogHandler(
                level, [&](msecs, Logger::Level err_lv, Error error, const void* data, size_t len) {
                    ++log_count[err_lv];
                    REQUIRE(!strcmp(error.msg, test_error.msg));
                    REQUIRE(error.type == test_error.type);
                    REQUIRE(error.code == test_error.code);
                    REQUIRE(data == test_data);
                    REQUIRE(len == test_data_len);
                });
    };
    for (int i = 0; i < Logger::N_LEVELS; ++i) {
        add_log_counter(static_cast<Logger::Level>(i));
    }
    for (int i = 0; i < Logger::N_LEVELS; ++i) {
        logger.log(static_cast<Logger::Level>(i), test_error, test_data, test_data_len);
    }
    for (int i = 0; i < Logger::N_LEVELS; ++i) {
        REQUIRE(log_count[i] == i + 1);
    }
}
