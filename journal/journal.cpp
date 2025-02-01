#include <cstdint>
#include <zmq.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <SDL3/SDL.h>

#include "clover.png.xxd"
#include "journal_common.h"
#include "niko1.png.xxd"
#include "niko2.png.xxd"
#include "niko3.png.xxd"

#ifdef __linux__
#include "xdg-user-dir-lookup.h"
#endif
#ifdef __WIN32
#include <windows.h>
#endif

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

SDL_HitTestResult hit_test_fun(SDL_Window *window, const SDL_Point *point,
                               void *userdata) {
  SDL_Surface *surf = (SDL_Surface *)userdata;

  unsigned char a = 0;
  SDL_ReadSurfacePixel(surf, point->x, point->y, NULL, NULL, NULL, &a);

  if (a > 10)
    return SDL_HITTEST_DRAGGABLE;
  else
    return SDL_HITTEST_NORMAL;
}

void journal_handling(const stbi_uc *initial_image_buf,
                      size_t initial_image_buf_len);
void niko_handling(int x, int y);

int main(int argc, char **argv) {
  if (argc == 3) {
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    niko_handling(x, y);
  } else {
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

        return 1;
      }

      fseek(file, 0, SEEK_END);
      initial_image_buf_len = ftell(file);
      fseek(file, 0, SEEK_SET);

      initial_image_buf = (stbi_uc *)malloc(initial_image_buf_len);
      fread((void *)initial_image_buf, 1, initial_image_buf_len, file);
      fclose(file);
    }

    journal_handling(initial_image_buf, initial_image_buf_len);

    if (initial_image_buf != ___journal_clover_png)
      free((void *)initial_image_buf);
  }
}

struct Ctx {
  zmq::context_t zmq_ctx;
  zmq::socket_t pub_socket;
  zmq::socket_t sub_socket;

  SDL_Mutex *mutex;

  SDL_Renderer *renderer;
  SDL_Window *window;
  SDL_Texture *texture;
  SDL_Surface *surface;
  stbi_uc *pixels;

  bool running;

  ~Ctx() {
    SDL_DestroyMutex(mutex);

    SDL_DestroyTexture(texture);
    SDL_DestroyWindow(window);

    SDL_DestroySurface(surface);
    stbi_image_free(pixels);
  }
};

int server_thread(void *data) {
  Ctx *ctx = (Ctx *)data;

  // send a hello over the queue to let oneshot (if it is open) know that a
  // journal has opened
  Message message;
  message.tag = Message::Hello;

  zmq::message_t zmq_message(&message, sizeof(Message));
  ctx->pub_socket.send(zmq_message, zmq::send_flags::none);

  while (ctx->running) {
    try {
      ctx->sub_socket.recv(zmq_message, zmq::recv_flags::none);
    } catch (zmq::error_t &exception) {
      std::cerr << "zmq socket recv error: " << exception.what() << std::endl;
      return 1;
    }

    message = *zmq_message.data<Message>();
    // lock the mutex while we are modifying ctx
    SDL_LockMutex(ctx->mutex);
    switch (message.tag) {
      // if we got a hello, respond with hello
      // (hello is sent when oneshot starts)
      // we dont need to worry about infinite loops here because we get a
      // hello from the message queue, not the status queue
    case Message::Hello: {
      message.tag = Message::Hello;
      zmq_message.rebuild(&message, sizeof(Message));
      ctx->pub_socket.send(zmq_message, zmq::send_flags::none);
    } break;
    // close if we are asked to
    case Message::Close:
      ctx->running = false;
      break;
    // move to a position
    case Message::SetWindowPosition: {
      SDL_SetWindowPosition(ctx->window, message.val.pos.x, message.val.pos.y);
    } break;
    // weve started to recieve an image path
    case Message::ImagePath: {
      std::string string;
      // recieve all chunks of the path
      do {
        string.append(message.val.text.chars, message.val.text.len);

        ctx->sub_socket.recv(zmq_message, zmq::recv_flags::none);
        message = *zmq_message.data<Message>();
        // break loop if we are done
      } while (message.tag == Message::ImagePath);

      // FIXME error handling
      FILE *file = fopen(string.c_str(), "rb");
      if (!file) {
        std::cerr << "failed to open file" << string;
        break;
      }

      SDL_DestroySurface(ctx->surface);
      SDL_DestroyTexture(ctx->texture);
      stbi_image_free(ctx->pixels);

      int w, h, comp;
      ctx->pixels = stbi_load_from_file(file, &w, &h, &comp, 4);

      ctx->surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888,
                                           ctx->pixels, w * 4);
      ctx->texture = SDL_CreateTextureFromSurface(ctx->renderer, ctx->surface);

      SDL_SetWindowSize(ctx->window, w, h);
      SDL_SetWindowHitTest(ctx->window, hit_test_fun, ctx->surface);
    } break;
    default:
      std::cerr << "Unhandled message tag";
      break;
    }
    // unlock the mutex
    SDL_UnlockMutex(ctx->mutex);
  }

  return 0;
}

int render_thread(void *data) {
  Ctx *ctx = (Ctx *)data;
  ctx->renderer = SDL_CreateRenderer(ctx->window, NULL);

  ctx->texture = SDL_CreateTextureFromSurface(ctx->renderer, ctx->surface);

  // I would move this into one line but apparently that leaves things
  // uninitialized??? C++ what the fuck
  int old_window_x = INT32_MAX;
  int old_window_y = INT32_MAX;
  int window_x = 0;
  int window_y = 0;

  while (ctx->running) {
    SDL_LockMutex(ctx->mutex);

    SDL_GetWindowPosition(ctx->window, &window_x, &window_y);
    if (old_window_x != window_x || old_window_y != window_y) {
      old_window_x = window_x;
      old_window_y = window_y;

      Message message;
      message.tag = Message::WindowPosition;
      message.val.pos = {window_x, window_y};
      zmq::message_t zmq_message(&message, sizeof(Message));
      ctx->pub_socket.send(zmq_message, zmq::send_flags::dontwait);
    }

    SDL_RenderClear(ctx->renderer);
    SDL_RenderTexture(ctx->renderer, ctx->texture, NULL, NULL);
    SDL_RenderPresent(ctx->renderer);

    SDL_ShowWindow(ctx->window);

    SDL_UnlockMutex(ctx->mutex);

    // calculates to 60 fps
    SDL_Delay(1000 / 60);
  }

  return 0;
}

void journal_handling(const stbi_uc *initial_image_buf,
                      size_t initial_image_buf_len) {
  // FIXME error handling
  SDL_Init(SDL_INIT_VIDEO);

  Ctx ctx;
  ctx.zmq_ctx = zmq::context_t(1);
  ctx.pub_socket = zmq::socket_t(ctx.zmq_ctx, zmq::socket_type::push);
  ctx.sub_socket = zmq::socket_t(ctx.zmq_ctx, zmq::socket_type::pull);

  ctx.pub_socket.bind("tcp://localhost:7969");
  ctx.sub_socket.connect("tcp://localhost:9697");

  // send a hello over the queue to let oneshot (if it is open) know that a
  // journal has opened
  Message message;
  message.tag = Message::Hello;

  zmq::message_t zmq_message(&message, sizeof(Message));
  ctx.pub_socket.send(zmq_message, zmq::send_flags::dontwait);

  int w, h, comp;
  ctx.pixels = stbi_load_from_memory(initial_image_buf, initial_image_buf_len,
                                     &w, &h, &comp, 4);
  ctx.surface =
      SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, ctx.pixels, w * 4);

  ctx.window =
      SDL_CreateWindow(" ", w, h, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_HIDDEN);

  SDL_SetWindowHitTest(ctx.window, hit_test_fun, ctx.surface);

  ctx.running = true;

  ctx.mutex = SDL_CreateMutex();
  SDL_CreateThread(server_thread, "server thread", &ctx);

  SDL_Event event;
  while (ctx.running) {
    while (SDL_WaitEventTimeout(&event, 16)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
        ctx.running = false;
        break;
      }
    }
  }

  // tell oneshot we have closed
  message.tag = Message::Goodbye;
  zmq_message.rebuild(&message, sizeof(Message));
  ctx.pub_socket.send(zmq_message, zmq::send_flags::dontwait);
}

void niko_handling(int x, int y) {
  // FIXME error handling
  SDL_Init(SDL_INIT_VIDEO);

  int w, h, comp;
  auto niko1_pixels = stbi_load_from_memory(
      ___journal_niko1_png, ___journal_niko1_png_len, &w, &h, &comp, 4);
  auto niko2_pixels = stbi_load_from_memory(
      ___journal_niko2_png, ___journal_niko2_png_len, &w, &h, &comp, 4);
  auto niko3_pixels = stbi_load_from_memory(
      ___journal_niko3_png, ___journal_niko3_png_len, &w, &h, &comp, 4);

  SDL_Window *window = SDL_CreateWindow(
      " ", w, h,
      SDL_WINDOW_TRANSPARENT | SDL_WINDOW_HIDDEN | SDL_WINDOW_ALWAYS_ON_TOP |
          SDL_WINDOW_UTILITY | SDL_WINDOW_BORDERLESS);
  SDL_SetWindowPosition(window, x, y);

  SDL_Rect screen_rect;
  SDL_DisplayID di = SDL_GetDisplayForWindow(window);
  SDL_GetDisplayUsableBounds(di, &screen_rect);

  SDL_Surface *niko1_surf = SDL_CreateSurfaceFrom(
      w, h, SDL_PIXELFORMAT_ABGR8888, niko1_pixels, w * 4);
  SDL_Surface *niko2_surf = SDL_CreateSurfaceFrom(
      w, h, SDL_PIXELFORMAT_ABGR8888, niko2_pixels, w * 4);
  SDL_Surface *niko3_surf = SDL_CreateSurfaceFrom(
      w, h, SDL_PIXELFORMAT_ABGR8888, niko3_pixels, w * 4);

  SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);

  SDL_Texture *niko1_tex = SDL_CreateTextureFromSurface(renderer, niko1_surf);
  SDL_Texture *niko2_tex = SDL_CreateTextureFromSurface(renderer, niko2_surf);
  SDL_Texture *niko3_tex = SDL_CreateTextureFromSurface(renderer, niko3_surf);

  for (int niko_offset = 0; niko_offset + y < screen_rect.h + screen_rect.y;
       niko_offset += 2) {
    // discard all os events so the os thinks we are not stuck
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }

    SDL_RenderClear(renderer);

    if (niko_offset % 32 >= 16)
      SDL_RenderTexture(renderer, niko2_tex, NULL, NULL);
    else if (niko_offset / 32 % 2)
      SDL_RenderTexture(renderer, niko1_tex, NULL, NULL);
    else
      SDL_RenderTexture(renderer, niko3_tex, NULL, NULL);

    SDL_RenderPresent(renderer);

    SDL_SetWindowPosition(window, x, niko_offset + y);
    SDL_ShowWindow(window);

    // calculates to 60 fps
    SDL_Delay(1000 / 60);
  }

  SDL_DestroyTexture(niko1_tex);
  SDL_DestroyTexture(niko2_tex);
  SDL_DestroyTexture(niko3_tex);

  SDL_DestroyWindow(window);

  SDL_DestroySurface(niko1_surf);
  SDL_DestroySurface(niko2_surf);
  SDL_DestroySurface(niko3_surf);

  stbi_image_free(niko1_pixels);
  stbi_image_free(niko2_pixels);
  stbi_image_free(niko3_pixels);
}