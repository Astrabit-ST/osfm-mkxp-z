#include "niko1.png.xxd"
#include "niko2.png.xxd"
#include "niko3.png.xxd"
#include "stb_image.h"
#include <SDL3/SDL.h>

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