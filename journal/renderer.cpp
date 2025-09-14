#include "renderer.h"
#include "clover.png.xxd"
#include "stb_image.h"
#include <SDL3/SDL.h>
#include <iostream>
#include <stdexcept>

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

// TODO: Apply shape, be borderless, and always on top only when image loaded
// TODO: Escape key to close window

SDL_AppResult Renderer::init() {
  if (initialized)
    throw std::runtime_error("Renderer already initialized.");

  initialized = true;
  SDL_Init(SDL_INIT_VIDEO);

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

  int w, h, comp;
  pixels = stbi_load_from_memory(initial_image_buf, initial_image_buf_len, &w,
                                 &h, &comp, 4);
  surface =
      SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * 4);

  // Set only when display image: SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS
  // Unset when image cleared
  window =
      SDL_CreateWindow(" ", w, h, SDL_WINDOW_TRANSPARENT | SDL_WINDOW_HIDDEN);
  renderer = SDL_CreateRenderer(window, NULL);
  texture = SDL_CreateTextureFromSurface(renderer, surface);

  SDL_SetWindowHitTest(window, Renderer::hit_test_fn, this);

  if (initial_image_buf != ___journal_clover_png)
    free((void *)initial_image_buf);

  return SDL_APP_CONTINUE;
}

void Renderer::iterate() {
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  SDL_RenderClear(renderer);
  SDL_RenderTexture(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
  SDL_ShowWindow(window);
}

SDL_HitTestResult Renderer::hit_test_fn(SDL_Window *window,
                                        const SDL_Point *point,
                                        void *userdata) {
  Renderer *state = (Renderer *)userdata;

  unsigned char a = 0;
  SDL_ReadSurfacePixel(state->surface, point->x, point->y, NULL, NULL, NULL,
                       &a);

  return a > 10 ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

void Renderer::set_image(char *filename) {
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
  SDL_SetWindowShape(window, surface);
}

void Renderer::move_window_to(int x, int y) {
  SDL_SetWindowPosition(window, x, y);
}

Renderer::~Renderer() {
  SDL_DestroyTexture(texture);
  SDL_DestroySurface(surface);
  stbi_image_free(pixels);
  SDL_DestroyWindow(window);
}