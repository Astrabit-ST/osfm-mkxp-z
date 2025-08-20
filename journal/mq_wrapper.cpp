#include "mq_wrapper.h"
#include "journal_common.h"
#include <boost/interprocess/ipc/message_queue.hpp>
#include <chrono>
#include <iostream>

#ifdef __linux__
#include "xdg-user-dir-lookup.h"
#endif
#ifdef __WIN32
#include <windows.h>
#endif

using namespace boost::interprocess;

#define RECEIVE_TIMEOUT std::chrono::milliseconds(100)
#define OPEN_RETRIES 10

MqWrapper::MqWrapper(const std::string &queue_name) : name(queue_name) {}

bool MqWrapper::try_open() {
  queue.reset();
  is_open = false;

  for (unsigned char attempt = 0; attempt < OPEN_RETRIES; ++attempt) {
    try {
      queue = std::make_unique<message_queue>(open_only, name.c_str());
      is_open = true;
      break;
    } catch (interprocess_exception &ex) {
      std::cerr << "Failed to open MQ: " << ex.what() << std::endl;
    }
  }

  return is_open;
}

bool MqWrapper::read(Message &message) {
  if (!is_open || !queue)
    return false;

  size_t recvd_size;
  unsigned int priority = 0;
  auto deadline = std::chrono::steady_clock::now() + RECEIVE_TIMEOUT;

  return queue->timed_receive(&message, sizeof(message), recvd_size, priority,
                              deadline);
}

bool MqWrapper::write(const Message &message) {
  if (!is_open || !queue)
    return false;

  unsigned int priority = 0;
  auto deadline = std::chrono::steady_clock::now() + RECEIVE_TIMEOUT;
  return queue->timed_send(&message, sizeof(message), priority, deadline);
}

void MqWrapper::clear() {
  if (!is_open || !queue)
    return;

  Message message;
  size_t recvd_size;
  unsigned int priority;

  while (queue->try_receive(&message, sizeof(message), recvd_size, priority))
    std::cout << "WARN: Leftover message " << message.tag << std::endl;
};