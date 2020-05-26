#include <catch2/catch.hpp>
#include <vsm/logger.hpp>

#include <cstring>
#include <iostream>

using namespace vsm;

TEST_CASE("logger") {
    Logger logger;
    const char* test_msg = "test_msg";
    int test_type = 5;
    const void* test_data = test_msg;
    size_t test_data_len = strlen(test_msg);
    int log_count[Logger::N_LEVELS] = {0};
    auto add_log_counter = [&](Logger::Level level) {
        logger.addLogHandler(level,
                [&](Logger::Level err_lv, const char* msg, int type, const void* data, size_t len) {
                    ++log_count[err_lv];
                    REQUIRE(!strcmp(msg, test_msg));
                    REQUIRE(type == test_type);
                    REQUIRE(data == test_data);
                    REQUIRE(len == test_data_len);
                });
    };
    for (int i = 0; i < Logger::N_LEVELS; ++i) {
        add_log_counter(static_cast<Logger::Level>(i));
    }
    for (int i = 0; i < Logger::N_LEVELS; ++i) {
        logger.log(static_cast<Logger::Level>(i), test_msg, test_type, test_data, test_data_len);
    }
    for (int i = 0; i < Logger::N_LEVELS; ++i) {
        REQUIRE(log_count[i] == i + 1);
    }
}
