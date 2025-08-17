#ifndef MQWRAPPER_H
#define MQWRAPPER_H

#include <boost/interprocess/ipc/message_queue.hpp>
#include "journal_common.h"
#include <chrono>
#include <optional>

using namespace boost::interprocess;

class MqWrapper {
    static constexpr auto receive_timeout = std::chrono::milliseconds(8);
    static constexpr unsigned char open_retries = 10;

    std::string name;
    std::unique_ptr<message_queue> queue;
    bool is_open = false;

public:
    explicit MqWrapper(const std::string &queue_name);
    MqWrapper(MqWrapper &&other) noexcept;
    MqWrapper &operator=(MqWrapper &&other) noexcept;

    MqWrapper(const MqWrapper &) = delete;
    MqWrapper &operator=(const MqWrapper &) = delete;

    bool try_open();
    std::optional<Message> read();
    bool write(const Message &message);
    void clear();
};

#endif