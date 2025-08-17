#ifndef MQSTATEMACHINE_H
#define MQSTATEMACHINE_H

#include "journal_common.h"
#include <functional>
#include <unistd.h>

struct MqConsumerStateMachine {
  using Callback = std::function<void(const Message &)>;
  void Run();
  void subscribe(Callback cb);
};

struct MqProducerStateMachine {
  void Run();
};

#endif