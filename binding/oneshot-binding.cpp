#include "binding-types.h"
#include "binding-util.h"
#include "etc.h"
#include "eventthread.h"
#include "oneshot.h"
#include "ruby/internal/special_consts.h"
#include "sharedstate.h"
#include <SDL3/SDL_video.h>

#include <SDL3/SDL.h>

RB_METHOD(oneshotSetYesNo) {
  RB_UNUSED_PARAM;
  const char *yes;
  const char *no;
  rb_get_args(argc, argv, "zz", &yes, &no RB_ARG_END);
  shState->oneshot().setYesNo(yes, no);
  return Qnil;
}

RB_METHOD(oneshotMsgBox) {
  RB_UNUSED_PARAM;
  int type;
  const char *body;
  const char *title = "";
  rb_get_args(argc, argv, "iz|z", &type, &body, &title RB_ARG_END);
  return rb_bool_new(shState->oneshot().msgbox(type, body, title));
}

// RB_METHOD(oneshotTextInput) {
//   RB_UNUSED_PARAM;
//   VALUE prompt;
//   int char_limit = 100;
//   VALUE font = Qnil;
//   rb_get_args(argc, argv, "S|iS", &prompt, &char_limit, &font RB_ARG_END);
//   std::string promptStr = std::string(RSTRING_PTR(prompt),
//   RSTRING_LEN(prompt)); std::string fontStr =
//       (font == Qnil) ? "" : std::string(RSTRING_PTR(font),
//       RSTRING_LEN(font));
//   return rb_str_new2(
//       shState->oneshot()
//           .textinput(promptStr.c_str(), char_limit, fontStr.c_str())
//           .c_str());
// }

RB_METHOD(oneshotResetObscured) {
  RB_UNUSED_PARAM;
  shState->oneshot().resetObscured();
  return Qnil;
}

RB_METHOD(oneshotObscuredCleared) {
  RB_UNUSED_PARAM;
  return shState->oneshot().obscuredCleared() ? Qtrue : Qfalse;
}

RB_METHOD(oneshotAllowExit) {
  RB_UNUSED_PARAM;
  bool allowExit;
  rb_get_args(argc, argv, "b", &allowExit RB_ARG_END);
  shState->oneshot().setAllowExit(allowExit);
  return Qnil;
}

RB_METHOD(oneshotExiting) {
  RB_UNUSED_PARAM;
  bool exiting;
  rb_get_args(argc, argv, "b", &exiting RB_ARG_END);
  shState->oneshot().setExiting(exiting);
  return Qnil;
}

RB_METHOD(oneshotShake) {
  RB_UNUSED_PARAM;
  int absx, absy;
  SDL_GetWindowPosition(shState->rtData().window, &absx, &absy);
  int state;
  srand(time(NULL));
  for (int i = 0; i < 60; ++i) {
    int max = 60 - i;
    int x = rand() % (max * 2) - max;
    int y = rand() % (max * 2) - max;
    SDL_SetWindowPosition(shState->rtData().window, absx + x, absy + y);
    rb_eval_string_protect("sleep 0.02", &state);
  }
  return Qnil;
}

// https://stackoverflow.com/questions/21001659/crc32-algorithm-implementation-in-c-without-a-look-up-table-and-with-a-public-li
uint32_t crc32b(char *data, size_t len) {
  uint32_t byte, crc, mask;

  crc = 0xFFFFFFFF;
  for (size_t i = 0; i < len; i++) {
    byte = data[i]; // Get next byte.
    crc = crc ^ byte;
    for (int j = 7; j >= 0; j--) { // Do eight times.
      mask = -(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320 & mask);
    }
  }
  return ~crc;
}

RB_METHOD(oneshotCRC32) {
  RB_UNUSED_PARAM;
  VALUE string;
  rb_get_args(argc, argv, "S", &string RB_ARG_END);

  uint32_t crc = crc32b(RSTRING_PTR(string), RSTRING_LEN(string));
  return UINT2NUM(crc);
}

void oneshotBindingInit() {
  VALUE module = rb_define_module("Oneshot");
  VALUE msg = rb_define_module_under(module, "Msg");

  // Constants
  rb_const_set(module, rb_intern("OS"),
               rb_str_new2(shState->oneshot().os().c_str()));
#ifdef __linux__
  const char *text;
  switch (shState->oneshot().desktop) {
  case Oneshot::DE_GNOME:
    text = "gnome";
    break;
  case Oneshot::DE_CINNAMON:
    text = "cinnamon";
    break;
  case Oneshot::DE_MATE:
    text = "mate";
    break;
  case Oneshot::DE_LXDE:
    text = "lxde";
    break;
  case Oneshot::DE_XFCE:
    text = "xfce";
    break;
  case Oneshot::DE_KDE:
    text = "kde";
    break;
  case Oneshot::DE_DEEPIN:
    text = "deepin";
    break;
  case Oneshot::DE_FALLBACK:
    text = "fallback";
    break;
  }
  rb_const_set(module, rb_intern("DE"), rb_str_new2(text));
#endif
  rb_const_set(module, rb_intern("USER_NAME"),
               rb_str_new2(shState->oneshot().userName().c_str()));
  rb_const_set(module, rb_intern("SAVE_PATH"),
               rb_str_new2(shState->oneshot().savePath().c_str()));
  rb_const_set(module, rb_intern("DOCS_PATH"),
               rb_str_new2(shState->oneshot().docsPath().c_str()));
  rb_const_set(module, rb_intern("GAME_PATH"),
               rb_str_new2(shState->oneshot().gamePath().c_str()));
  rb_const_set(module, rb_intern("JOURNAL"),
               rb_str_new2(shState->oneshot().journal().c_str()));
  rb_const_set(module, rb_intern("LANG"),
               rb_str_new2(shState->oneshot().lang().c_str()));
  rb_const_set(msg, rb_intern("INFO"), INT2FIX(Oneshot::MSG_INFO));
  rb_const_set(msg, rb_intern("YESNO"), INT2FIX(Oneshot::MSG_YESNO));
  rb_const_set(msg, rb_intern("WARN"), INT2FIX(Oneshot::MSG_WARN));
  rb_const_set(msg, rb_intern("ERR"), INT2FIX(Oneshot::MSG_ERR));

  // Functions
  _rb_define_module_function(module, "set_yes_no", oneshotSetYesNo);
  _rb_define_module_function(module, "msgbox", oneshotMsgBox);
  // _rb_define_module_function(module, "textinput", oneshotTextInput);
  _rb_define_module_function(module, "reset_obscured", oneshotResetObscured);
  _rb_define_module_function(module, "obscured_cleared?",
                             oneshotObscuredCleared);
  _rb_define_module_function(module, "allow_exit", oneshotAllowExit);
  _rb_define_module_function(module, "exiting", oneshotExiting);
  _rb_define_module_function(module, "shake", oneshotShake);
  _rb_define_module_function(module, "crc32", oneshotCRC32);
}