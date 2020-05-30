#pragma once
#include <chrono>
#include <functional>

namespace vsm {

template <class Duration>
class TimeSync {
public:
    TimeSync(std::function<Duration(void)> local_time)
            : _offset_estimate(-local_time())
            , _local_time(std::move(local_time)) {}

    void syncTime(Duration time, float weight = 1) {
        Duration offset = time - _local_time();
        typename Duration::rep offset_correction = (offset - _offset_estimate).count() * weight;
        _offset_estimate += Duration(offset_correction);
    };

    Duration getTime() const { return _local_time() + _offset_estimate; }
    Duration getLocalTime() const { return _local_time(); }
    Duration getOffset() const { return _offset_estimate; }

    Duration fromLocalTime(Duration time) const { return time + _offset_estimate; }
    Duration toLocalTime(Duration time) const { return time - _offset_estimate; }

private:
    Duration _offset_estimate;
    std::function<Duration(void)> _local_time;
};

}  // namespace vsm
