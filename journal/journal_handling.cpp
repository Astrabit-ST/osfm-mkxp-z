#include "journal_common.h"
#include <cstddef>
#define SDL_MAIN_HANDLED 1 // we are handling this ourselves

#include "SDL_custom_events.h"
#include "renderer.h"
#include <SDL3/SDL_main.h>
#include <atomic>
#include <iostream>

static boost::interprocess::shared_memory_object journal_shm;
static boost::interprocess::mapped_region journal_region;
static Journal *journal = nullptr;

std::atomic<bool> consumer_stop_requested(false);

static int consumer_loop(void *data) {
  JournalGuard guard(*journal, false);

  if (journal->active_count < SIZE_MAX) {
    ++journal->active_count;
  }

  uint64_t image_nonce = journal->image.nonce;
  uint64_t close_nonce = journal->close.nonce;
  uint64_t set_journal_position_nonce = journal->set_journal_position.nonce;

  while (!consumer_stop_requested) {
    if (image_nonce != journal->image.nonce) {
      image_nonce = journal->image.nonce;
      SDL_Event event;
      SDL_zero(event);
      event.type = JOURNAL_SET_IMAGE;
      event.user.data1 = new std::string(journal->image.path);
      SDL_PushEvent(&event);
    }

    if (close_nonce != journal->close.nonce) {
      close_nonce = journal->close.nonce;
      SDL_Event event;
      SDL_zero(event);
      event.type = SDL_EVENT_QUIT;
      SDL_PushEvent(&event);
    }

    if (set_journal_position_nonce != journal->set_journal_position.nonce) {
      set_journal_position_nonce = journal->set_journal_position.nonce;
      SDL_Event event;
      SDL_zero(event);
      event.type = JOURNAL_SET_WINDOW_POSITION;
      event.user.data1 = new std::pair<int, int>(journal->set_journal_position.x, journal->set_journal_position.y);
      SDL_PushEvent(&event);
    }

    journal->cond.wait(guard);
  }

  if (journal->active_count > 0) {
    --journal->active_count;
  }

  return 0;
}

struct State {
  Renderer *renderer;

  SDL_Thread *consumer_thread;

  State() {
    renderer = new Renderer();
    if (renderer->init() != SDL_APP_CONTINUE) {
      std::cout << "Renderer init error: " << SDL_GetError() << std::endl;
      exit(-1);
    }

    init_journal(journal_shm, journal_region, journal);

    consumer_stop_requested = false;
    consumer_thread = SDL_CreateThread(consumer_loop, "consumer_thread", NULL);
  }

  SDL_AppResult iterate() {
    renderer->iterate();
    return SDL_APP_CONTINUE;
  };

  SDL_AppResult sdl_event(SDL_Event *event) {
    if (event->type == ONESHOT_LAUNCHED) {
      return SDL_APP_CONTINUE;
    }

    if (event->type == JOURNAL_SET_IMAGE) {
      std::string *filename = (std::string *)event->user.data1;
      try {
        renderer->set_image(filename->c_str());
      } catch (...) {
        delete filename;
        throw;
      }
      delete filename;
      return SDL_APP_CONTINUE;
    }

    if (event->type == JOURNAL_SET_WINDOW_POSITION) {
      std::pair<int, int> *coords = (std::pair<int, int> *)event->user.data1;
      try {
        renderer->move_window_to(coords->first, coords->second);
      } catch (...) {
        delete coords;
        throw;
      }
      delete coords;
      return SDL_APP_CONTINUE;
    }

    switch (event->type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
    case SDL_EVENT_QUIT:
      return SDL_APP_SUCCESS;
    case SDL_EVENT_WINDOW_MOVED:
      {
        JournalGuard guard(*journal, true);
        journal->get_journal_position.x = event->window.data1;
        journal->get_journal_position.y = event->window.data2;
      }
      return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
  };

  ~State() {
    consumer_stop_requested = true;
    try {
      journal->cond.notify_all();
    } catch (...) {}
    SDL_WaitThread(consumer_thread, nullptr);
    deinit_journal(journal_shm, journal_region, journal);
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
