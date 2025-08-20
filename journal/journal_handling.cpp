#define SDL_MAIN_HANDLED 1 // we are handling this ourselves

#include "SDL_custom_events.h"
#include "mq_state_machine.h"
#include "renderer.h"
#include <SDL3/SDL_main.h>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <iostream>

using namespace boost::interprocess;

enum UserEvent {
  ChangeImage,
  MoveTo,
};

struct State {
  Renderer *renderer;

  MqConsumerStateMachine *o2j;
  MqProducerStateMachine *j2o;

  SDL_Thread *consumer_thread;
  SDL_Thread *producer_thread;

  State() {
    renderer = new Renderer();
    if (renderer->init() != SDL_APP_CONTINUE) {
      std::cout << "Renderer init error: " << SDL_GetError() << std::endl;
      exit(-1);
    }

    o2j = new MqConsumerStateMachine();
    j2o = new MqProducerStateMachine();

    consumer_thread = SDL_CreateThread(consumer_fn, "consumer_thread", o2j);
    producer_thread = SDL_CreateThread(producer_fn, "producer_thread", j2o);
  }

  SDL_AppResult iterate() {
    renderer->iterate();
    return SDL_APP_CONTINUE;
  };

  SDL_AppResult sdl_event(SDL_Event *event) {
    if (event->type == JOURNAL_CLOSE)
      return stop();

    if (event->type == JOURNAL_CHANGE_IMAGE) {
      return SDL_APP_CONTINUE;
    }

    switch (event->type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_QUIT:
      return stop();
    case SDL_EVENT_WINDOW_MOVED:
      return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
  };

  SDL_AppResult stop() {
    o2j->stop();
    j2o->stop();
    return SDL_APP_SUCCESS;
  }

  static int consumer_fn(void *userdata) {
    MqConsumerStateMachine *mq = (MqConsumerStateMachine *)userdata;
    mq->run();
    return 0;
  }

  static int producer_fn(void *userdata) {
    MqProducerStateMachine *mq = (MqProducerStateMachine *)userdata;
    mq->run();
    return 0;
  }

  ~State() {
    SDL_WaitThread(consumer_thread, NULL);
    SDL_WaitThread(producer_thread, NULL);
  }
};

void journal_handling(int argc, char **argv) {
  SDL_EnterAppMainCallbacks(
      argc, argv,
      [](void **appstate, int argc, char **argv) -> SDL_AppResult {
        *appstate = new State();
        return SDL_APP_CONTINUE;
      },
      [](void *appstate) -> SDL_AppResult {
        State *state = (State *)appstate;
        return state->iterate();
      },
      [](void *appstate, SDL_Event *event) -> SDL_AppResult {
        State *state = (State *)appstate;
        return state->sdl_event(event);
      },
      [](void *appstate, SDL_AppResult result) -> void {
        delete (State *)appstate;
      });
}
