/*
** eventthread.h
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef EVENTTHREAD_H
#define EVENTTHREAD_H

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_scancode.h>

#include <string>

#include <stdint.h>

#include "config.h"
#include "etc-internal.h"
#include "keybindings.h"
#include "sdl-util.h"
#include <SDL3/SDL_video.h>

struct RGSSThreadData;
#ifndef MKXPZ_NO_OPENAL
typedef struct MKXPZ_ALCDEVICE ALCdevice;
#endif
struct SDL_Window;
union SDL_Event;

#define MAX_FINGERS 4

class EventThread {
public:
  struct ControllerState {
    int axes[SDL_GAMEPAD_AXIS_COUNT];
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
  };

  struct MouseState {
    int x, y;
    bool inWindow;
    bool buttons[32];
  };

  struct FingerState {
    bool down;
    int x, y;
  };

  struct TouchState {
    FingerState fingers[MAX_FINGERS];
  };

  static uint8_t keyStates[SDL_SCANCODE_COUNT];
  static ControllerState controllerState;
  static MouseState mouseState;
  static TouchState touchState;
  static SDL_AtomicInt verticalScrollDistance;

  std::string textInputBuffer;
  void lockText(bool lock);

  static bool allocUserEvents();

  EventThread();
  ~EventThread();

  void process(RGSSThreadData &rtData);
  void cleanup();

  /* Called from RGSS thread */
  void requestFullscreenMode(bool mode);
  void requestWindowResize(int width, int height);
  void requestWindowReposition(int x, int y);
  void requestWindowCenter();
  void requestWindowRename(const char *title);
  void requestShowCursor(bool mode);

  struct CreateWindowArgs {
    int x;
    int y;
    int w;
    int h;
    unsigned int flags;
    const char *name;
  };
  // get random freezes without doing this on the event thread
  SDL_Window *requestNewWindow(const CreateWindowArgs *args);
  void destroySDLWindow(SDL_Window *window);
  void requestWindowVisible(SDL_Window *window, bool visible);

  void requestTextInputMode(bool mode);

  void requestSettingsMenu();

  void requestTerminate();

  bool getFullscreen() const;
  bool getShowCursor() const;
  bool getControllerConnected() const;

  SDL_Gamepad *controller() const;

  void showMessageBox(const char *body, int flags = 0);

  /* RGSS thread calls this once per frame */
  void notifyFrame();

  /* Called on game screen (size / offset) changes */
  void notifyGameScreenChange(const SDL_Rect &screen);

private:
  static bool eventFilter(void *, SDL_Event *);

  void resetInputStates();
  void setFullscreen(SDL_Window *, bool mode);
  void updateCursorState(bool inWindow, const SDL_Rect &screen);
  void cursorTimer();

  bool fullscreen;
  bool showCursor;

  SDL_Gamepad *ctrl;

  AtomicFlag msgBoxDone;

  SDL_Mutex *textInputLock;

  bool acceptingTextInput = false;

  struct {
    AtomicFlag sendUpdates;
  } fps;
};

/* Used to asynchronously inform the RGSS thread
 * about certain value changes */
template <typename T> struct UnidirMessage {
  UnidirMessage() : mutex(SDL_CreateMutex()), current(T()) {}

  ~UnidirMessage() { SDL_DestroyMutex(mutex); }

  /* Done from the sending side */
  void post(const T &value) {
    SDL_LockMutex(mutex);

    changed.set();
    current = value;

    SDL_UnlockMutex(mutex);
  }

  /* Done from the receiving side */
  bool poll(T &out) const {
    if (!changed)
      return false;

    SDL_LockMutex(mutex);

    out = current;
    changed.clear();

    SDL_UnlockMutex(mutex);

    return true;
  }

  /* Done from either */
  void get(T &out) const {
    SDL_LockMutex(mutex);
    out = current;
    SDL_UnlockMutex(mutex);
  }

private:
  SDL_Mutex *mutex;
  mutable AtomicFlag changed;
  T current;
};

struct SyncPoint {
  /* Used by eventFilter to control sleep/wakeup */
  void haltThreads();
  void resumeThreads();

  /* Used by RGSS thread */
  bool mainSyncLocked();
  void waitMainSync();

  /* Used by secondary (audio) threads */
  void passSecondarySync();

private:
  struct Util {
    Util();
    ~Util();

    void lock();
    void unlock(bool multi);
    void waitForUnlock();

    AtomicFlag locked;
    SDL_Mutex *mut;
    SDL_Condition *cond;
  };

  Util mainSync;
  Util reply;
  Util secondSync;
};

struct RGSSThreadData {
  /* Main thread sets this to request RGSS thread to terminate */
  AtomicFlag rqTerm;
  /* In response, RGSS thread sets this to confirm
   * that it received the request and isn't stuck */
  AtomicFlag rqTermAck;

  /* Set when F12 is pressed */
  AtomicFlag rqReset;

  /* Set when F12 is released */
  AtomicFlag rqResetFinish;

  // Set when window is being adjusted (resize, reposition)
  AtomicFlag rqWindowAdjust;

  /* True if we're currently exiting */
  AtomicFlag exiting;

  /* True if exiting is allowed */
  AtomicFlag allowExit;

  /* Set when attempting to exit and allowExit is false */
  AtomicFlag triedExit;

  EventThread *ethread;
  UnidirMessage<Vec2i> windowSizeMsg;
  UnidirMessage<Vec2i> drawableSizeMsg;
  UnidirMessage<BDescVec> bindingUpdateMsg;
  SyncPoint syncPoint;

  const char *argv0;

  SDL_Window *window;
#ifndef MKXPZ_NO_OPENAL
  ALCdevice *alcDev;
#endif

  SDL_GLContext glContext;

  Vec2 sizeResoRatio;
  Vec2i screenOffset;
  int scale;
  const int refreshRate;

  Config config;

  std::string rgssErrorMsg;

  RGSSThreadData(EventThread *ethread, const char *argv0, SDL_Window *window,
#ifndef MKXPZ_NO_OPENAL
                 ALCdevice *alcDev,
#endif
                 int refreshRate, int scalingFactor, const Config &newconf,
                 SDL_GLContext ctx)
      : ethread(ethread), argv0(argv0), window(window),
#ifndef MKXPZ_NO_OPENAL
        alcDev(alcDev),
#endif
        sizeResoRatio(1, 1), refreshRate(refreshRate), scale(scalingFactor),
        config(newconf), glContext(ctx) {
    rqResetFinish.set();
  }
};

#endif // EVENTTHREAD_H
