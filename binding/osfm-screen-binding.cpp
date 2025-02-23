#include "binding-util.h"
#include "etc-internal.h"
#include "eventthread.h"
#include "gl-fun.h"
#include "gl-meta.h"
#include "gl-util.h"
#include "graphics.h"
#include "scene.h"
#include "sharedstate.h"
#include <SDL3/SDL.h>

struct MonitorWindow {
  SDL_Window *window;
  ScreenScene scene;

  MonitorWindow(int x, int y, int w, int h, unsigned int flags,
                const char *name, bool is_transparent)
      : scene(w, h) {
    scene.transparent =
        is_transparent; // this is handled differently, it's weird
    EventThread::CreateWindowArgs args = {x, y, w, h, flags, name};
    window = shState->eThread().requestNewWindow(&args);
    shState->monitorWindows.insert(this);
    render();
  }
  ~MonitorWindow() {
    shState->monitorWindows.erase(this);
    shState->eThread().destroySDLWindow(window); // do it on the event thread
  }

  // renders to ping pong buffer
  void render();
  Scene *getScene();
};
// so we can use this from outside this file
Scene *MonitorWindow::getScene() { return &scene; }
void MonitorWindow::render() {
  SDL_GLContext ctx = shState->graphics().context();
  // TODO throw an error if this fails (somehow)
  SDL_GL_MakeCurrent(window, ctx);

  scene.composite();
  auto geo = scene.getGeometry();
  int w = geo.rect.w;
  int h = geo.rect.h;

  GLMeta::blitBeginScreen(Vec2i(w, h), false);
  GLMeta::blitSource(scene.getPP().frontBuffer(), 0);

  GLMeta::blitRectangle(IntRect(0, 0, w, h), IntRect(0, h, w, -h), false);

  GLMeta::blitEnd();
  SDL_GL_SwapWindow(window);
}

static void free_monitor_window(void *window) {
  // delete if not null
  if (window)
    delete (MonitorWindow *)window;
}

DEF_TYPE_CUSTOMNAME_AND_FREE(MonitorWindow, "MonitorWindow",
                             free_monitor_window);

#define GUARD_DISPOSED(w)                                                      \
  if (!w->window)                                                              \
    rb_raise(rb_eRuntimeError, "Window already disposed!");

RB_METHOD(monitorWindowInit) {
  VALUE vx, vy, vw, vh;
  VALUE kwargs;
  rb_scan_args(argc, argv, "4:", &vx, &vy, &vw, &vh, &kwargs);

  int x = NUM2INT(vx);
  int y = NUM2INT(vy);
  int w = NUM2INT(vw);
  int h = NUM2INT(vh);

  if (w < 1 || h < 1)
    rb_raise(rb_eArgError, "Invalid window size");

  const char *name = " ";
  unsigned int flags = SDL_WINDOW_UTILITY | SDL_WINDOW_BORDERLESS;
  bool is_transparent = true;
  if (!NIL_P(kwargs)) {
    ID table[7] = {
        rb_intern("borderless"),    rb_intern("hidden"),
        rb_intern("always_on_top"), rb_intern("fullscreen"),
        rb_intern("skip_taskbar"),  rb_intern("transparent"),
        rb_intern("name"),
    };
    VALUE values[7];

    rb_get_kwargs(kwargs, table, 0, 7, values);

    if (!RTEST(values[0]) && values[0] != Qundef)
      flags &= ~SDL_WINDOW_BORDERLESS; // enabled by default
    if (RTEST(values[1]) && values[1] != Qundef)
      flags |= SDL_WINDOW_HIDDEN; // shown by default
    if (RTEST(values[2]) && values[2] != Qundef)
      flags |= SDL_WINDOW_ALWAYS_ON_TOP; // not always on top by default
    if (RTEST(values[3]) && values[3] != Qundef)
      flags |= SDL_WINDOW_FULLSCREEN; // not fullscreen by default
    if (!RTEST(values[4]) && values[4] != Qundef)
      flags &= ~SDL_WINDOW_UTILITY; // skipped by default
    if (!RTEST(values[5]) && values[5] != Qundef)
      is_transparent = false; // we handle this seperately because all windows
                              // need to be transparent for glx reasons
    if (values[6] != Qundef)
      name = StringValueCStr(values[6]);
  }

  MonitorWindow *window =
      new MonitorWindow(x, y, w, h, flags, name, is_transparent);

  setPrivateData(self, window);

  return self;
}

RB_METHOD(monitorWindowDispose) {
  MonitorWindow *w = getPrivateData<MonitorWindow>(self);
  GUARD_DISPOSED(w);

  setPrivateData(self, nullptr); // setPrivateData(nullptr) calls delete for us

  return Qnil;
}

RB_METHOD(monitorWindowResize) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int w, h;
  rb_get_args(argc, argv, "ii", &w, &h RB_ARG_END);

  if (w < 1 || h < 1)
    rb_raise(rb_eArgError, "Invalid window size");

  SDL_SetWindowSize(window->window, w, h);
  window->scene.setResolution(w, h);

  return Qnil;
}

RB_METHOD(monitorWindowMove) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y RB_ARG_END);

  SDL_SetWindowPosition(window->window, x, y);

  return Qnil;
}

RB_METHOD(monitorWindowPos) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int x, y;
  SDL_GetWindowPosition(window->window, &x, &y);

  return rb_ary_new_from_args(2, INT2NUM(x), INT2NUM(y));
}

RB_METHOD(monitorWindowSize) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int w, h;
  SDL_GetWindowSize(window->window, &w, &h);

  return rb_ary_new_from_args(2, INT2NUM(w), INT2NUM(h));
}

RB_METHOD(monitorWindowShow) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  shState->eThread().requestWindowVisible(window->window, true);

  return Qnil;
}

RB_METHOD(monitorWindowHide) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  shState->eThread().requestWindowVisible(window->window, false);

  return Qnil;
}

RB_METHOD(monitorSetAlwaysOnTop) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  bool top;
  rb_get_args(argc, argv, "b", &top);

  SDL_SetWindowAlwaysOnTop(window->window, top);

  return Qnil;
}

RB_METHOD(monitorFlashWindow) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int state;
  rb_get_args(argc, argv, "i", &state);

  if (state < SDL_FLASH_CANCEL || state > SDL_FLASH_UNTIL_FOCUSED)
    rb_raise(rb_eArgError, "Invalid flash state");

  SDL_FlashWindow(window->window, (SDL_FlashOperation)state);
  return Qnil;
}

RB_METHOD(monitorWindowRaise) {
  MonitorWindow *window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  SDL_RaiseWindow(window->window);

  return Qnil;
}

void osfmBindingInit() {

  VALUE klass = rb_define_class("MonitorWindow", rb_cObject);
  rb_define_alloc_func(klass, classAllocate<&MonitorWindowType>);

  _rb_define_method(klass, "initialize", monitorWindowInit);
  _rb_define_method(klass, "dispose", monitorWindowDispose);
  _rb_define_method(klass, "size", monitorWindowSize);
  _rb_define_method(klass, "resize", monitorWindowResize);
  _rb_define_method(klass, "move_to", monitorWindowMove);
  _rb_define_method(klass, "position", monitorWindowPos);
  _rb_define_method(klass, "show", monitorWindowShow);
  _rb_define_method(klass, "hide", monitorWindowHide);
  _rb_define_method(klass, "set_always_on_top", monitorSetAlwaysOnTop);
  _rb_define_method(klass, "flash", monitorFlashWindow);
  _rb_define_method(klass, "raise", monitorWindowRaise);

  rb_define_const(klass, "UNDEFINED_POS", INT2NUM(SDL_WINDOWPOS_UNDEFINED));
  rb_define_const(klass, "CENTERED_POS", INT2NUM(SDL_WINDOWPOS_CENTERED));
}