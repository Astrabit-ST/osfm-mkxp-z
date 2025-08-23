#include "journal_common.h"
#include <cstddef>
#define SDL_MAIN_HANDLED 1 // we are handling this ourselves

#include "SDL_custom_events.h"
#include "renderer.h"
#include <SDL3/SDL_main.h>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <iostream>

using namespace boost::interprocess;

int consumer_loop(void *userdata);
void consumer_stop();

int producer_loop(void *userdata);
void producer_stop();
void produce_message(Message message);

enum UserEvent {
  ChangeImage,
  MoveTo,
};

struct State {
  Renderer *renderer;

  SDL_Thread *consumer_thread;
  SDL_Thread *producer_thread;

  State() {
    renderer = new Renderer();
    if (renderer->init() != SDL_APP_CONTINUE) {
      std::cout << "Renderer init error: " << SDL_GetError() << std::endl;
      exit(-1);
    }

    consumer_thread = SDL_CreateThread(consumer_loop, "consumer_thread", NULL);
    producer_thread = SDL_CreateThread(producer_loop, "producer_thread", NULL);
  }

  SDL_AppResult iterate() {
    renderer->iterate();
    return SDL_APP_CONTINUE;
  };

  SDL_AppResult sdl_event(SDL_Event *event) {
    if (event->type == JOURNAL_CLOSE)
      return stop();

    if (event->type == ONESHOT_LAUNCHED) {
      return SDL_APP_CONTINUE;
    }

    if (event->type == JOURNAL_SET_IMAGE) {
      char *filename = (char *)event->user.data1;
      renderer->set_image(filename);
      free(filename);
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
    consumer_stop();
    producer_stop();
    return SDL_APP_SUCCESS;
  }

  ~State() { SDL_WaitThread(consumer_thread, NULL); }
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
