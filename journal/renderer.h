#ifndef RENDERER_H
#define RENDERER_H
#include "stb_image.h"
#include <SDL3/SDL.h>

struct Renderer {
  void iterate();
  static SDL_HitTestResult hit_test_fn(SDL_Window *window,
                                       const SDL_Point *point, void *userdata);

  void set_image(char *filename);

  void move_window_to(int x, int y);
};

#endif