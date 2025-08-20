#include "mq_state_machine.h"
#include "SDL3/SDL_timer.h"
#include "journal_common.h"
#include "mq_wrapper.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <unistd.h>

#define RETRY_DELAY_MS 500

struct MqStateRunning;
struct MqStateConnecting;

struct MqStateClosed : IMqState {
  const IMqState *successState;
  const IMqState *run(MqWrapper &queue) const override { return this; };
};

struct MqStateConnecting : IMqState {
  const IMqState *successState;
  explicit MqStateConnecting(const IMqState *successState_)
      : successState(successState_) {}

  const IMqState *run(MqWrapper &queue) const override {
    std::cout << "Attempting to reconnect... ";
    if (queue.try_open()) {
      std::cout << "Succes!" << std::endl;
      return successState;
    }

    std::cout << "Failed." << std::endl;
    SDL_Delay(RETRY_DELAY_MS);
    return this;
  };
};

struct MqStateConsumerRunning : IMqState {
  const IMqState *run(MqWrapper &queue) const override;
};

struct MqStateProducerRunning : IMqState {
  const IMqState *run(MqWrapper &queue) const override;
};

const MqStateClosed mqStateClosing{};

const MqStateConsumerRunning mqStateConsuming{};
const MqStateConnecting mqStateConnectingConsumer{&mqStateConsuming};

const MqStateProducerRunning mqStateProducing{};
const MqStateConnecting mqStateConnectingProducer{&mqStateProducing};

const IMqState *MqStateConsumerRunning::run(MqWrapper &queue) const {
  bool success = false;
  Message message;

  for (char i = 0; i < 5; i++) {
    success = queue.read(message);
    if (success)
      break;
  }

  if (!success)
    return &mqStateConnectingConsumer;

  switch (message.tag) {
  case Message::Hello: {
    // Temporary
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
    return &mqStateClosing;
  }
  case Message::Goodbye: {
    break;
  }
  case Message::Close: {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
    return &mqStateClosing;
  }
  case Message::WindowPosition: {
    break;
  }
  case Message::SetWindowPosition: {
    break;
  }
  case Message::ImagePath: {
    break;
  }
  case Message::FinishImagePath: {
    break;
  }
  default:
    std::cerr << "Unhandled message tag: " << message.tag << std::endl;
    break;
  }

  return &mqStateConsuming;
}

const IMqState *MqStateProducerRunning::run(MqWrapper &queue) const {
  bool success = false;
  Message message;

  for (char i = 0; i < 5; i++) {
    success = queue.read(message);
    if (success)
      break;
  }

  if (!success)
    return &mqStateConnectingProducer;

  return &mqStateProducing;
}

MqConsumerStateMachine::MqConsumerStateMachine()
    : state(&mqStateConnectingConsumer), queue("oneshot_mq") {}

void MqConsumerStateMachine::run() {
  while (state != &mqStateClosing &&
         !stop_requested.load(std::memory_order_acquire)) {
    state = state->run(queue);
  }
};

void MqConsumerStateMachine::stop() {
  stop_requested.store(true, std::memory_order_release);
};

MqProducerStateMachine::MqProducerStateMachine()
    : state(&mqStateProducing), queue("journal_mq") {}

void MqProducerStateMachine::run() {
  while (state != &mqStateClosing &&
         !stop_requested.load(std::memory_order_acquire)) {
    state = state->run(queue);
  }
};

void MqProducerStateMachine::stop() {
  stop_requested.store(true, std::memory_order_release);
};