#ifndef RENDERER_H
#define RENDERER_H
#include "stb_image.h"
#include <SDL3/SDL.h>

struct Renderer {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Surface *surface;
  stbi_uc *pixels;
  bool initialized = false;

  SDL_AppResult init();
  void iterate();
  static SDL_HitTestResult hit_test_fn(SDL_Window *window,
                                       const SDL_Point *point, void *userdata);

  void set_image(char *filename);
  void move_window_to(int x, int y);

  ~Renderer();
};

#endif