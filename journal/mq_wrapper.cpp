#include "journal_common.h"
#include <boost/interprocess/ipc/message_queue.hpp>
#include <chrono>
#include <iostream>
#include <optional>

#ifdef __linux__
#include "xdg-user-dir-lookup.h"
#endif
#ifdef __WIN32
#include <windows.h>
#endif

using namespace boost::interprocess;

class MqWrapper {
  static constexpr auto receive_timeout = std::chrono::milliseconds(8);
  static constexpr unsigned char open_retries = 10;

  std::string name;
  std::unique_ptr<message_queue> queue;
  bool is_open = false;

public:
  explicit MqWrapper(const std::string &queue_name) : name(queue_name) {}

  MqWrapper(MqWrapper &&other) noexcept
      : name(std::move(other.name)), queue(std::move(other.queue)),
        is_open(other.is_open) {
    other.is_open = false;
  }

  MqWrapper &operator=(MqWrapper &&other) noexcept {
    if (this != &other) {
      name = std::move(other.name);
      queue = std::move(other.queue);
      is_open = other.is_open;
      other.is_open = false;
    }
    return *this;
  }

  MqWrapper(const MqWrapper &) = delete;
  MqWrapper &operator=(const MqWrapper &) = delete;

  bool try_open() {
    queue.reset();
    is_open = false;

    for (unsigned char attempt = 0; attempt < open_retries; ++attempt) {
      try {
        queue = std::make_unique<message_queue>(open_only, name.c_str());
        is_open = true;
        break;
      } catch (interprocess_exception &ex) {
        std::cerr << "Failed to open MQ: " << ex.what() << std::endl;
      }
    }

    return is_open;
  };

  std::optional<Message> read() {
    if (!is_open || !queue)
      return std::nullopt;

    Message message{};
    size_t recvd_size;
    unsigned int priority = 0;
    auto deadline = std::chrono::steady_clock::now() + receive_timeout;

    if (queue->timed_receive(&message, sizeof(message), recvd_size, priority,
                             deadline)) {
      return message;
    };

    return std::nullopt;
  }

  bool write(const Message &message) {
    if (!is_open || !queue)
      return false;

    unsigned int priority = 0;
    auto deadline = std::chrono::steady_clock::now() + receive_timeout;
    return queue->timed_send(&message, sizeof(message), priority, deadline);
  }

  void clear() {
    if (!is_open || !queue)
      return;

    Message message;
    size_t recvd_size;
    unsigned int priority;

    while (queue->try_receive(&message, sizeof(message), recvd_size, priority))
      std::cout << "WARN: Leftover message " << message.tag << std::endl;
  };
};