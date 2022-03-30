#pragma once
#include <chrono>
#include <functional>

namespace vsm {

class TimeSync {
public:
    TimeSync(std::function<int64_t(void)> local_time)
            : _offset_estimate(-local_time())
            , _local_time(std::move(local_time)) {}

    void syncTime(int64_t time, float weight = 1) {
        int64_t offset = time - _local_time();
        int64_t offset_correction = (offset - _offset_estimate) * weight;
        _offset_estimate += offset_correction;
    };

    int64_t getTime() const { return _local_time() + _offset_estimate; }
    int64_t getLocalTime() const { return _local_time(); }
    int64_t getOffset() const { return _offset_estimate; }

    int64_t fromLocalTime(int64_t time) const { return time + _offset_estimate; }
    int64_t toLocalTime(int64_t time) const { return time - _offset_estimate; }

private:
    int64_t _offset_estimate;
    std::function<int64_t(void)> _local_time;
};

}  // namespace vsm
