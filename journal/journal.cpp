#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <cstdlib>

void journal_handling(int argc, char **argv);
void niko_handling(int x, int y);

int main(int argc, char **argv) {
  if (argc == 3) {
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    niko_handling(x, y);
  } else {
    journal_handling(argc, argv);
  }
}
