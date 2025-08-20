#ifndef MQWRAPPER_H
#define MQWRAPPER_H

#include "journal_common.h"
#include <boost/interprocess/ipc/message_queue.hpp>

using namespace boost::interprocess;

class MqWrapper {
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
  bool read(Message &message);
  bool write(const Message &message);
  void clear();
};

#endif