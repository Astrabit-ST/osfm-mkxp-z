#ifndef MQSTATEMACHINE_H
#define MQSTATEMACHINE_H

#include "journal_common.h"
#include "mq_wrapper.h"
#include <atomic>
#include <unistd.h>

struct IMqState {
  virtual const IMqState *run(MqWrapper &queue) const = 0;
};

typedef void (*Callback)(const Message &);

struct MqConsumerStateMachine {
  const IMqState *state;
  MqWrapper queue;
  std::atomic<bool> stop_requested{false};

public:
  MqConsumerStateMachine();
  void run();
  void stop();
};

struct MqProducerStateMachine {
  const IMqState *state;
  MqWrapper queue;
  std::atomic<bool> stop_requested{false};

public:
  MqProducerStateMachine();
  void run();
  void stop();
};

#endif