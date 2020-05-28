#pragma once
#include <functional>

namespace vsm {

template <class ValueType>
struct AssignmentIterator {
    using AssignmentFunction = std::function<void(const ValueType&)>;
    struct AssignmentValue {
        void operator=(const ValueType& value) { assignment_function(value); }
        AssignmentFunction assignment_function;
    };
    using iterator_category = std::output_iterator_tag;
    using value_type = AssignmentValue;
    using difference_type = size_t;
    using pointer = value_type*;
    using reference = value_type&;

    AssignmentIterator(AssignmentFunction assignment_function)
            : assignment_value{assignment_function} {}

    AssignmentIterator& operator++() { return *this; }
    value_type& operator*() { return assignment_value; }

    value_type assignment_value;
};

class Transport {
public:
    using ReceiverCallback = std::function<void(const void* buffer, size_t len)>;
    using TimerCallback = std::function<void(int timer_id)>;

    virtual const std::string& getAddress() const = 0;

    virtual int connect(const std::string& dst_addr) = 0;
    virtual int disconnect(const std::string& dst_addr) = 0;

    virtual int transmit(const void* buffer, size_t len, const std::string& group) = 0;

    virtual int addReceiver(ReceiverCallback receiver_callback, const std::string& group) = 0;
    virtual int addTimer(size_t interval, TimerCallback timer_callback) = 0;  // miliseconds

    virtual int poll(int timeout) = 0;  // miliseconds, -1 = inf, 0 = non-blocking

    auto connectIterator() {
        return AssignmentIterator<std::string>(
                [this](const std::string& address) { connect(address); });
    };

    auto disconnectIterator() {
        return AssignmentIterator<std::string>(
                [this](const std::string& address) { disconnect(address); });
    };
};

}  // namespace vsm
