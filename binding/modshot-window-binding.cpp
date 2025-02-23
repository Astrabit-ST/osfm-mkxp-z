#include "SDL3/SDL_video.h"
#include "binding-util.h"
#include "eventthread.h"
#include "graphics.h"
#include "oneshot.h"
#include "sharedstate.h"

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#ifdef _WIN32
#include <windows.h>
#else
// #include
#endif

RB_METHOD(GetWindowPosition) {
  int x, y;
  SDL_GetWindowPosition(shState->rtData().window, &x, &y);
  return rb_ary_new3(2, LONG2FIX(x), LONG2FIX(y));
}

RB_METHOD(SetWindowPosition) {
  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y);
  SDL_SetWindowPosition(shState->rtData().window, x, y);
  return Qnil;
}

RB_METHOD(SetTitle) {
  char *wintitle; // thx rkevin
  rb_get_args(argc, argv, "z", &wintitle);
  SDL_SetWindowTitle(shState->rtData().window, wintitle);
  return Qnil;
}

/*
RB_METHOD(SetTransparentColor) {

        RB_UNUSED_PARAM;
        Value colorObj;
        rb_get_args(argc, argv, "o", &colorObj RB_ARG_END);
        Color *c = getPrivateDataCheck<Color>(colorObj, ColorType);
        #ifdef _WIN32
        SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);  // Initialize wmInfo
    SDL_GetWindowWMInfo(shState->rtData().window, &wmInfo);
    HWND hWnd = wmInfo.info.win.window;
        SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) |
WS_EX_LAYERED);
    // Set transparency color
    SetLayeredWindowAttributes(hWnd, RGB(c.r, c.g, c.b), 0, LWA_COLORKEY);
        #else
        #endif
        return Qnil;
}
*/

RB_METHOD(SetIcon) {
  char *path;
  rb_get_args(argc, argv, "z", &path);
  SDL_Surface *icon = IMG_Load(path);
  if (!icon) {
    rb_raise(rb_eRuntimeError, "Loading icon from path failed");
  }
  SDL_SetWindowIcon(shState->rtData().window, icon);
  return Qnil;
}

RB_METHOD(SetWindowOpacity) {

  float opacity;
  rb_get_args(argc, argv, "f", &opacity);
  SDL_SetWindowOpacity(shState->rtData().window, opacity);
  return Qnil;
}

RB_METHOD(SetAlwaysOnTop) {
  bool top;
  rb_get_args(argc, argv, "b", &top);
  SDL_SetWindowAlwaysOnTop(shState->rtData().window, top);
  return Qnil;
}

RB_METHOD(FlashWindow) {
  int state;
  rb_get_args(argc, argv, "i", &state);

  if (state < SDL_FLASH_CANCEL || state > SDL_FLASH_UNTIL_FOCUSED)
    rb_raise(rb_eArgError, "Invalid flash state");

  SDL_FlashWindow(shState->rtData().window, (SDL_FlashOperation)state);
  return Qnil;
}

RB_METHOD(WindowRaise) {
  RB_UNUSED_PARAM;
  SDL_RaiseWindow(shState->rtData().window);
  return Qnil;
}

RB_METHOD(SetTransparent) {
  bool state;
  rb_get_args(argc, argv, "b", &state);
  shState->graphics().setMainWinTransparent(state);
  return Qnil;
}

RB_METHOD(GetTransparent) {
  RB_UNUSED_PARAM;
  bool val = shState->graphics().getMainWinTransparent();
  return rb_bool_new(val);
}

void modshotwindowBindingInit() {
  VALUE module = rb_define_module("ModWindow");
  _rb_define_module_function(module, "GetWindowPosition", GetWindowPosition);
  _rb_define_module_function(module, "SetWindowPosition", SetWindowPosition);
  _rb_define_module_function(module, "SetTitle", SetTitle);
  _rb_define_module_function(module, "SetIcon", SetIcon);
  _rb_define_module_function(module, "setWindowOpacity", SetWindowOpacity);
  _rb_define_module_function(module, "setAlwaysOnTop", SetAlwaysOnTop);
  _rb_define_module_function(module, "flashWindow", FlashWindow);
  _rb_define_module_function(module, "raiseWindow", WindowRaise);
  _rb_define_module_function(module, "transparent", GetTransparent);
  _rb_define_module_function(module, "transparent=", SetTransparent);
  //_rb_define_module_function(module, "setWindowChromaKey",
  // SetTransparentColor);
}