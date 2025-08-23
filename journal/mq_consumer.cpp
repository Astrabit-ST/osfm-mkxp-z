#include "SDL_custom_events.h"
#include "mq_wrapper.h"
#include <atomic>
#include <iostream>

#define TRY_CONSUME_RETRIES 5

enum ConsumerState { connecting, consuming };

MqWrapper consumer_mq = MqWrapper("oneshot_mq");
ConsumerState consumer_state = ConsumerState::connecting;
std::atomic<bool> consumer_stop_requested{false};

bool consumer_try_consume() {
  bool success = false;
  Message message;

  for (char i = 0; i < TRY_CONSUME_RETRIES; i++) {
    success = consumer_mq.read(message);
    if (success)
      break;
  }

  if (!success)
    return false;

  switch (message.tag) {
  case Message::Close: {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
    return true;
  }
  case Message::ImagePath: {
    std::string string;
    do {
      string.append(message.val.text.chars, message.val.text.len);
    } while (consumer_mq.read(message) && message.tag == Message::ImagePath);

    if (message.tag != Message::FinishImagePath) {
      // Last message received not FinishImagePath -> Error?
      // Failed reading message chain up to FinishImagePath -> Error.
    }

    // i would send an std::string if i new how.
    // i'd much rather send a c string than allocate an std::string*
    // also i tried using new[] and delete but idfk why but valgrind HATED
    // that so i'm just gonna use malloc
    char *filename = (char *)malloc(string.size() + 1);
    memcpy(filename, string.c_str(), string.size() + 1);
    filename[string.size()] = '\0';

    SDL_Event event;
    SDL_zero(event);
    event.type = JOURNAL_SET_IMAGE;
    event.user.data1 = filename;
    SDL_PushEvent(&event);
    break;
  }
  case Message::FinishImagePath: {
    // Error?
    break;
  }
  case Message::Hello:
  case Message::Goodbye:
  case Message::WindowPosition:
  case Message::SetWindowPosition:
    break;
  default:
    std::cerr << "Unhandled message tag: " << message.tag << std::endl;
    break;
  }

  return true;
}

int consumer_loop(void *userdata) {
  while (!consumer_stop_requested.load(std::memory_order_acquire)) {
    switch (consumer_state) {
    case connecting:
      consumer_state = consumer_mq.try_open() ? consuming : connecting;
      break;
    case consuming:
      consumer_state = consumer_try_consume() ? consuming : connecting;
      break;
    default:
      std::cerr << "[Consumer] Invalid state: " << consumer_state << std::endl;
      consumer_state = connecting;
      break;
    }
  }

  return 0;
}

void consumer_stop() {
  consumer_stop_requested.store(true, std::memory_order_release);
}