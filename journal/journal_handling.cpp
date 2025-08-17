#define SDL_MAIN_HANDLED 1 // we are handling this ourselves

#include "mq_state_machine.h"
#include "renderer.h"
#include <SDL3/SDL_main.h>
#include <boost/interprocess/ipc/message_queue.hpp>

using namespace boost::interprocess;

enum UserEvent {
  ChangeImage,
  MoveTo,
};
Uint32 user_event_start = 0;

struct State {
  MqConsumerStateMachine *o2j;
  MqProducerStateMachine *j2o;
  Renderer *renderer;

  SDL_Thread *server_thread;

  State() {
    server_thread = SDL_CreateThread(server_thread_fn, "server_thread", this);
    renderer = new Renderer();
  }

  SDL_AppResult iterate() { return SDL_APP_CONTINUE; };
  SDL_AppResult event(SDL_Event *event) { return SDL_APP_CONTINUE; };

  static int server_thread_fn(void *userdata) {
    // State *state = (State *)userdata;
    return 0;
  }

  ~State() { SDL_WaitThread(server_thread, NULL); }
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
        return state->event(event);
      },
      [](void *appstate, SDL_AppResult result) -> void {
        delete (State *)appstate;
      });
}
