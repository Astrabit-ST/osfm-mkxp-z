#pragma once

#include <SDL3/SDL.h>
inline const Uint32 user_event_start = [] { return SDL_RegisterEvents(3); }();
inline const Uint32 ONESHOT_LAUNCHED = [] { return user_event_start + 1; }();
inline const Uint32 JOURNAL_CLOSE = [] { return user_event_start + 2; }();
inline const Uint32 JOURNAL_SET_IMAGE = [] { return user_event_start + 3; }();