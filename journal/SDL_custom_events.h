#include <SDL3/SDL_main.h>

Uint32 user_event_start = SDL_RegisterEvents(2);
Uint32 JOURNAL_CHANGE_IMAGE = user_event_start + 1;
Uint32 JOURNAL_CLOSE = user_event_start + 2;