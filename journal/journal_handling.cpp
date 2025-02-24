#include "journal_common.h"
#include "stb_image.h"

#include <boost/interprocess/ipc/message_queue.hpp>

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED 1 // we are handling this ourselves
#include <SDL3/SDL_main.h>

#include "clover.png.xxd"

#ifdef __linux__
#include "xdg-user-dir-lookup.h"
#endif
#ifdef __WIN32
#include <windows.h>
#endif

#include <chrono>
#include <iostream>
#include <string>

using namespace boost::interprocess;

std::string fake_save_path() {
#ifdef __linux__
  std::string path = xdg_user_dir_lookup("DOCUMENTS");
#endif
#ifdef __WIN32
  std::string path = getenv("USERPROFILE");
  path += "/Documents/My Games";
#endif
  path += "/Oneshot/save_progress.oneshot";
  return path;
}

enum UserEvent {
  ChangeImage,
  MoveTo,
};
Uint32 user_event_start = 0;

struct State {
  SDL_Window *window;

  SDL_Renderer *renderer;

  SDL_Texture *texture;
  SDL_Surface *surface;
  stbi_uc *pixels;

  message_queue oneshot_mq; // messages oneshot sends us
  message_queue journal_mq; // messages we send to oneshot

  SDL_Thread *server_thread;
  bool is_running;

  State(const stbi_uc *image_buf, size_t image_buf_len)
      : oneshot_mq(open_or_create, "oneshot_mq", 100, sizeof(Message)),
        journal_mq(open_or_create, "journal_mq", 100, sizeof(Message)),
        is_running(true) {
    Message message;
    size_t recvd_size;
    unsigned int priority;
    // clear the queue by reading all the messages
    while (oneshot_mq.try_receive(&message, sizeof(message), recvd_size,
                                  priority)) {
      std::cout << "WARN:" << "Leftover message " << message.tag << std::endl;
    }

    int w, h, comp;
    pixels = stbi_load_from_memory(image_buf, image_buf_len, &w, &h, &comp, 4);
    surface =
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * 4);

    window =
        SDL_CreateWindow(" ", w, h, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_HIDDEN);
    renderer = SDL_CreateRenderer(window, NULL);
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_SetWindowHitTest(window, State::hit_test_fn, this);

    // send a hello to signal that we've started
    message = {.tag = Message::Hello};
    journal_mq.send(&message, sizeof(message), 255);

    server_thread = SDL_CreateThread(server_thread_fn, "server_thread", this);
  }

  ~State() {
    is_running = false;
    SDL_WaitThread(server_thread, NULL);

    SDL_DestroyTexture(texture);
    SDL_DestroySurface(surface);
    stbi_image_free(pixels);

    SDL_DestroyWindow(window);

    // send a goodbye to signal that we've closed
    Message message = {.tag = Message::Goodbye};
    journal_mq.send(&message, sizeof(message), 255);
  }

  static SDL_HitTestResult hit_test_fn(SDL_Window *window,
                                       const SDL_Point *point, void *userdata) {
    State *state = (State *)userdata;

    unsigned char a = 0;
    SDL_ReadSurfacePixel(state->surface, point->x, point->y, NULL, NULL, NULL,
                         &a);

    if (a > 10)
      return SDL_HITTEST_DRAGGABLE;
    else
      return SDL_HITTEST_NORMAL;
  }

  static int server_thread_fn(void *userdata) {
    State *state = (State *)userdata;

    Message message;
    unsigned int priority;
    size_t recvd_size;

    // for whatever reason a plain recieve() misses some messages. this doesn't
    // tho
    while (state->is_running) {
      bool did_recv = false;
      while (!did_recv) {
        auto now = std::chrono::steady_clock::now();
        auto abs_time = now + std::chrono::milliseconds(8);
        did_recv = state->oneshot_mq.timed_receive(
            &message, sizeof(message), recvd_size, priority, abs_time);
        if (!state->is_running)
          return 0;
      }
      switch (message.tag) {
      case Message::Hello: {
        message.tag = Message::Hello;
        state->journal_mq.send(&message, sizeof(message), 255);
      }
      case Message::ImagePath: {
        std::string string;
        // recieve all chunks of the path
        do {
          string.append(message.val.text.chars, message.val.text.len);
          state->oneshot_mq.receive(&message, sizeof(message), recvd_size,
                                    priority);
          // break loop if we are done
        } while (message.tag == Message::ImagePath);

        // i would send an std::string if i new how.
        // i'd much rather send a c string than allocate an std::string*
        // also i tried using new[] and delete but idfk why but valgrind HATED
        // that so i'm just gonna use malloc
        char *filename = (char *)malloc(string.size() + 1);
        memcpy(filename, string.c_str(), string.size() + 1);
        filename[string.size()] = '\0';

        SDL_Event event;
        SDL_zero(event);
        event.type = UserEvent::ChangeImage + user_event_start;
        event.user.data1 = filename;
        SDL_PushEvent(&event);
        break;
      }
      case Message::Close: {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&event);
        return 0;
      }
      default:
        std::cerr << "Unhandled message tag" << std::endl;
        break;
      }
    }

    return 0;
  }

  void set_image(char *filename) {
    SDL_DestroySurface(surface);
    SDL_DestroyTexture(texture);
    stbi_image_free(pixels);

    // FIXME error handling
    FILE *file = fopen(filename, "rb");
    if (!file) {
      std::cerr << "failed to open file" << filename << std::endl;
      return;
    }

    int w, h, comp;
    pixels = stbi_load_from_file(file, &w, &h, &comp, 4);
    surface =
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * 4);
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_SetWindowSize(window, w, h);
  }

  void move_window_to(int x, int y) { SDL_SetWindowPosition(window, x, y); }
};

SDL_AppResult app_init(void **appstate, int argc, char **argv) {
  SDL_Init(SDL_INIT_VIDEO);
  user_event_start = SDL_RegisterEvents(2);

  const stbi_uc *initial_image_buf = ___journal_clover_png;
  size_t initial_image_buf_len = ___journal_clover_png_len;

  std::string save_path = fake_save_path();
  FILE *file = fopen(save_path.c_str(), "rb");
  if (file) {
    // 4 bytes + null terminator
    char pathlen_buf[5] = {0};
    fread(&pathlen_buf, 1, 4, file);
    int pathlen = atoi(pathlen_buf);
    std::string save_image_path(pathlen, '\0');
    fread(save_image_path.data(), 1, pathlen, file);
    fclose(file);

    file = fopen(save_image_path.c_str(), "rb");

    if (!file) {
      std::cerr << "loading save image failed: " << save_image_path
                << std::endl;
      return SDL_APP_FAILURE;
    }

    fseek(file, 0, SEEK_END);
    initial_image_buf_len = ftell(file);
    fseek(file, 0, SEEK_SET);

    initial_image_buf = (stbi_uc *)malloc(initial_image_buf_len);
    fread((void *)initial_image_buf, 1, initial_image_buf_len, file);
    fclose(file);
  }

  *appstate = new State(initial_image_buf, initial_image_buf_len);

  if (initial_image_buf != ___journal_clover_png)
    free((void *)initial_image_buf);

  return SDL_APP_CONTINUE;
}

SDL_AppResult app_iterate(void *appstate) {
  State *state = (State *)appstate;

  SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 0);
  SDL_RenderClear(state->renderer);
  SDL_RenderTexture(state->renderer, state->texture, NULL, NULL);
  SDL_RenderPresent(state->renderer);

  SDL_ShowWindow(state->window);

  return SDL_APP_CONTINUE;
}

SDL_AppResult app_event(void *appstate, SDL_Event *event) {
  State *state = (State *)appstate;

  switch (event->type) {
  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
  case SDL_EVENT_QUIT:
    return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_MOVED: {
    Message message{
        .tag = Message::WindowPosition,
        .val = {{event->window.data1, event->window.data2}},
    };
    state->journal_mq.send(&message, sizeof(message), 0);
  }
  }

  // can't handle these in a switch.
  // also tbh, i'm not sure if this is how you're supposed to do this, because
  // sdl3 seems to have two different mechanisms for identifying user events?
  // you can register multiple, but there's also a user defined event code. Who
  // knows
  if (event->type == UserEvent::ChangeImage + user_event_start) {
    char *filename = (char *)event->user.data1;
    state->set_image(filename);
    free(filename);
  }
  if (event->type == UserEvent::MoveTo + user_event_start) {
    int x = (size_t)event->user.data1;
    int y = (size_t)event->user.data2;
    state->move_window_to(x, y);
  }

  return SDL_APP_CONTINUE;
}

void app_quit(void *appstate, SDL_AppResult result) {
  delete (State *)appstate;
}

void journal_handling(int argc, char **argv) {
  SDL_EnterAppMainCallbacks(argc, argv, app_init, app_iterate, app_event,
                            app_quit);
}
