#include "mq_wrapper.h"
#include <functional>
#include <thread>
#include <unistd.h>

struct IMqState {
  virtual const IMqState *run(MqWrapper &queue) const = 0;
};

struct MqStateRunning;
struct MqStateConnecting;

struct MqStateConnecting : IMqState {
  const int retry_delay_ms = 500;
  const IMqState *successState;
  explicit MqStateConnecting(const IMqState *successState_)
      : successState(successState_) {}

  const IMqState *run(MqWrapper &queue) const override {
    if (queue.try_open())
      return successState;

    std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
    return this;
  };
};

struct MqStateRunning : IMqState {
  const IMqState *run(MqWrapper &queue) const override;
};

const MqStateRunning mqStateConsuming{};
const MqStateConnecting mqStateConnectingConsumer{&mqStateConsuming};

const MqStateRunning mqStateProducing{};
const MqStateConnecting mqStateConnectingProducer{&mqStateProducing};

const IMqState *MqStateRunning::run(MqWrapper &queue) const {
  return &mqStateConsuming;
}

struct MqConsumerStateMachine {
  const IMqState *state = &mqStateConnectingConsumer;
  MqWrapper queue = MqWrapper("oneshot_mq");

  using Callback = std::function<void(const Message &)>;
  Callback subscriber;

  void Run() {
    do {
      state = state->run(queue);
    } while (true);
  };

  void subscribe(Callback cb) { subscriber = std::move(cb); }

  void notify(const Message &msg) const {
    if (subscriber)
      subscriber(msg);
  }
};

struct MqProducerStateMachine {
  const IMqState *state = &mqStateConnectingProducer;
  MqWrapper queue = MqWrapper("journal_mq");

  void Run() {
    do {
      state = state->run(queue);
    } while (true);
  };
};