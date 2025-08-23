#include "mq_wrapper.h"
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <thread>

#define IDLE_SLEEP_TIME std::chrono::milliseconds(100)

enum ProducerState { connecting, idle, producing };

MqWrapper producer_mq = MqWrapper("journal_mq");
ProducerState producer_state = ProducerState::connecting;
std::atomic<bool> producer_stop_requested{false};
Message *queued_message = NULL;

bool try_produce() {
  if (!queued_message) {
    std::cout << "[Producer] Attempted producing empty message." << std::endl;
    return true;
  }

  bool success = producer_mq.write(*queued_message);
  if (success)
    free(queued_message);

  return success;
}

int producer_loop(void *userdata) {
  while (!producer_stop_requested.load(std::memory_order_acquire)) {
    switch (producer_state) {
    case connecting:
      producer_state = producer_mq.try_open() ? idle : connecting;
      break;
    case idle:
      std::this_thread::sleep_for(IDLE_SLEEP_TIME);
      producer_state = queued_message ? producing : idle;
      break;
    case producing:
      if (try_produce())
        producer_state = queued_message ? producing : idle;
      else
        producer_state = connecting;
      break;
    default:
      std::cerr << "[Producer] Invalid state: " << producer_state << std::endl;
      producer_state = connecting;
      break;
    }
  }

  return 0;
}

void producer_stop() {
  producer_stop_requested.store(true, std::memory_order_release);
}

void produce_message(Message message) { queued_message = &message; }