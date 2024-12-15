#ifndef ONESHOT_H
#define ONESHOT_H

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "etc-internal.h"
#include <string>

struct OneshotPrivate;
struct RGSSThreadData;

class Oneshot {
public:
  Oneshot(RGSSThreadData &threadData);
  ~Oneshot();

  // msgbox type codes
  enum {
    MSG_INFO,
    MSG_YESNO,
    MSG_WARN,
    MSG_ERR,
  };

  // Wallpaper style
  enum {
    STYLE_NONE,
    STYLE_TILE,
    STYLE_CENTER,
    STYLE_STRETCH,
    STYLE_FIT,
    STYLE_FILL,
    STYLE_SPAN,
  };

  // Wallpaper gradient
  enum {
    GRADIENT_NONE,
    GRADIENT_HORIZONTAL,
    GRADIENT_VERTICAL,
  };

  // this isn't JS, we have enums!!!!!!!
  enum Desktop {
    DE_GNOME,
    DE_CINNAMON,
    DE_MATE,
    DE_LXDE,
    DE_XFCE,
    DE_KDE,
    DE_DEEPIN,
    DE_FALLBACK,
  };

  void update();

  // Accessors
  const std::string &os() const;
  const std::string &lang() const;
  const std::string &userName() const;
  const std::string &savePath() const;
  const std::string &docsPath() const;
  const std::string &gamePath() const;
  const std::string &journal() const;
  const std::vector<uint8_t> &obscuredMap() const;
  bool obscuredCleared() const;
  bool allowExit() const;
  bool exiting() const;

  // Mutators
  void setYesNo(const char *yes, const char *no);
  void setWindowPos(int x, int y);
  void setExiting(bool exiting);
  void setAllowExit(bool allowExit);
  void resetObscured();

  // Functions
  bool msgbox(int type, const char *body, const char *title);
  // std::string textinput(const char* prompt, int char_limit, const char*
  // fontName);

  // Dirty flag for obscured texture
  bool obscuredDirty;

#ifdef __linux__
  Desktop desktop;
#endif

private:
  OneshotPrivate *p;
  RGSSThreadData &threadData;
};

#endif // ONESHOT_H