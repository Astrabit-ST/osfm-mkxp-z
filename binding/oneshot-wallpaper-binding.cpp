#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <vector>

#include "binding-types.h"
#include "binding-util.h"
#include "config.h"
#include "debugwriter.h"
#include "etc.h"
#include "oneshot.h"
#include "sharedstate.h"

#ifdef _WIN32
#include <windows.h>
static WCHAR szStyle[8] = {0};
static WCHAR szTile[8] = {0};
static WCHAR szFile[MAX_PATH + 1] = {0};
static DWORD oldcolor = 0;
static DWORD szStyleSize = sizeof(szStyle) - 1;
static DWORD szTileSize = sizeof(szTile) - 1;
static bool setStyle = false;
static bool setTile = false;
static bool isCached = false;
#else
#ifdef __APPLE__
#include "mac-desktop.h"
static bool isCached = false;
#else
#include <dlfcn.h>
#include <gio/gio.h>
#include <glib.h>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

struct XfconfChannel;
typedef gboolean (*xfconf_init_fn)(GError **error);
typedef void (*xfconf_shutdown_fn)(void);

typedef XfconfChannel *(*xfconf_channel_get_fn)(const gchar *channel_name);
typedef gchar *(*xfconf_channel_get_string_fn)(XfconfChannel *channel,
                                               const gchar *property,
                                               const gchar *default_value);
typedef gboolean (*xfconf_channel_set_string_fn)(XfconfChannel *channel,
                                                 const gchar *property,
                                                 const gchar *value);
typedef gint32 (*xfconf_channel_get_int_fn)(XfconfChannel *channel,
                                            const gchar *property,
                                            gint32 default_value);
typedef gboolean (*xfconf_channel_set_int_fn)(XfconfChannel *channel,
                                              const gchar *property,
                                              gint32 value);

typedef gboolean (*xfconf_channel_get_property_fn)(XfconfChannel *channel,
                                                   const gchar *property,
                                                   GValue *value);
typedef gboolean (*xfconf_channel_set_property_fn)(XfconfChannel *channel,
                                                   const gchar *property,
                                                   const GValue *value);

typedef void (*xfconf_channel_reset_property_fn)(XfconfChannel *channel,
                                                 const gchar *property_base,
                                                 gboolean recursive);

struct DesktopEnv {
  DesktopEnv();
  ~DesktopEnv();

  void set_wallpaper(std::string path, std::string gameDirStr, int color);
  void reset_wallpaper();

  Oneshot::Desktop desktop;

  // GNOME settings
  GSettings *bgsetting;
  std::string defPictureURI, defPictureOptions, defPrimaryColor,
      defColorShading;
  // XFCE settings
  XfconfChannel *bgchannel;
  void *xfconf_dl;
  int defPictureStyle;
  int defColorStyle;
  GValue defColor = G_VALUE_INIT;
  bool defColorExists;
  std::string optionImage, optionColor, optionImageStyle, optionColorStyle;
  // KDE settings
  std::map<std::string, std::string> defPlugins, defPictures, defColors,
      defModes;
  std::map<std::string, bool> defBlurs;
  // Fallback settings
  std::string fallbackPath;
};
static DesktopEnv *env;

#endif
#endif

#ifdef __linux__
DesktopEnv::DesktopEnv() {
  desktop = shState->oneshot().desktop;
  switch (desktop) {
  case Oneshot::DE_CINNAMON:
  case Oneshot::DE_GNOME:
  case Oneshot::DE_MATE:
  case Oneshot::DE_DEEPIN: {
    switch (desktop) {
    case Oneshot::DE_CINNAMON:
      bgsetting = g_settings_new("org.cinnamon.desktop.background");
      break;
    case Oneshot::DE_DEEPIN:
      bgsetting = g_settings_new("com.deepin.wrap.gnome.desktop.background");
      break;
    case Oneshot::DE_GNOME:
      bgsetting = g_settings_new("org.gnome.desktop.background");
      break;
    case Oneshot::DE_MATE:
      bgsetting = g_settings_new("org.mate.background");
      defPictureURI = g_settings_get_string(bgsetting, "picture-filename");
      break;
    default:
      break;
    }
    defPictureOptions = g_settings_get_string(bgsetting, "picture-options");
    defPrimaryColor = g_settings_get_string(bgsetting, "primary-color");
    defColorShading = g_settings_get_string(bgsetting, "color-shading-type");
    break;
  }
  case Oneshot::DE_XFCE: {
    GError *xferror = NULL;
    xfconf_dl = dlopen("libxfconf.so", RTLD_NOW);

    if (!xfconf_dl) {
      desktop = Oneshot::DE_FALLBACK;
      break;
    }
    auto xfconf_init = (xfconf_init_fn)dlsym(xfconf_dl, "xfconf_init");
    auto xfconf_channel_get =
        (xfconf_channel_get_fn)dlsym(xfconf_dl, "xfconf_init");
    auto xfconf_channel_get_string = (xfconf_channel_get_string_fn)dlsym(
        xfconf_dl, "xfconf_channel_get_string");
    auto xfconf_channel_get_int =
        (xfconf_channel_get_int_fn)dlsym(xfconf_dl, "xfconf_channel_get_int");
    auto xfconf_channel_get_property = (xfconf_channel_get_property_fn)dlsym(
        xfconf_dl, "xfconf_channel_get_property");

    if (!xfconf_init(&xferror)) {
      // Configuration failed to initialize, we won't set the wallpaper
      desktop = Oneshot::DE_FALLBACK;
      g_error_free(xferror);
      break;
    }
    bgchannel = xfconf_channel_get("xfce4-desktop");
    std::string optionPrefix = "/backdrop/screen0/monitor0/workspace0/";
    optionImage = optionPrefix + "last-image";
    optionColor = optionPrefix + "color1";
    optionImageStyle = optionPrefix + "image-style";
    optionColorStyle = optionPrefix + "color-style";
    defPictureURI =
        xfconf_channel_get_string(bgchannel, optionImage.c_str(), "");
    defPictureStyle =
        xfconf_channel_get_int(bgchannel, optionImageStyle.c_str(), -1);
    defColorExists =
        xfconf_channel_get_property(bgchannel, optionColor.c_str(), &defColor);
    defColorStyle =
        xfconf_channel_get_int(bgchannel, optionColorStyle.c_str(), -1);
    break;
  }
  case Oneshot::DE_KDE: {
    std::ifstream configFile;
    configFile.open(std::string(getenv("HOME")) +
                        "/.config/plasma-org.kde.plasma.desktop-appletsrc",
                    std::ios::in);
    if (!configFile.is_open()) {
      Debug() << "FATAL: Cannot find desktop configuration!";
      desktop = Oneshot::DE_FALLBACK;
      break;
    }

    std::string line;
    std::vector<std::string> sections;
    std::size_t undefined = 999999999;
    bool readPlugin = false, readOther = false;
    std::string containment;
    while (getline(configFile, line)) {
      std::size_t index = undefined, lastIndex = undefined;
      if (line.size() == 0) {
        readPlugin = false;
        readOther = false;
      } else if (readPlugin) {
        index = line.find('=');
        if (line.substr(0, index) == "wallpaperplugin") {
          defPlugins[containment] = line.substr(index + 1);
        }
      } else if (readOther) {
        index = line.find('=');
        std::string key = line.substr(0, index);
        std::string val = line.substr(index + 1);
        if (key == "Image") {
          defPictures[containment] = val;
        } else if (key == "Color") {
          defColors[containment] = val;
        } else if (key == "FillMode") {
          defModes[containment] = val;
        } else if (key == "Blur") {
          defBlurs[containment] = (val == "true");
        }
      } else if (line.at(0) == '[') {
        sections.clear();
        while (true) {
          index = line.find(lastIndex == undefined ? '[' : ']',
                            index == undefined ? 0 : index);
          if (index == std::string::npos) {
            break;
          }
          if (lastIndex == undefined) {
            lastIndex = index;
          } else {
            sections.push_back(
                line.substr(lastIndex + 1, index - lastIndex - 1));
            lastIndex = undefined;
          }
        }
        if (sections.size() == 2 && sections[0] == "Containments") {
          readPlugin = true;
          containment = sections[1];
        } else if (sections.size() == 5 && sections[0] == "Containments" &&
                   sections[2] == "Wallpaper" &&
                   sections[3] == "org.kde.image" && sections[4] == "General") {
          readOther = true;
          containment = sections[1];
        }
      }
    }
    configFile.close();

    break;
  }
  case Oneshot::DE_LXDE:
  case Oneshot::DE_FALLBACK: {
    break;
  }
  }

  // if fallback, do fallback to a path on the desktop
  if (desktop == Oneshot::DE_FALLBACK) {
    fallbackPath = std::string(getenv("HOME")) + "/Desktop/ONESHOT_hint.png";
  }
}

DesktopEnv::~DesktopEnv() {
  if (desktop == Oneshot::DE_XFCE) {
    auto xfconf_shutdown =
        (xfconf_shutdown_fn)dlsym(xfconf_dl, "xfconf_shutdown");
    xfconf_shutdown();
    dlclose(xfconf_dl);
  }
}

void DesktopEnv::set_wallpaper(std::string path, std::string gameDirStr,
                               int color) {
  switch (desktop) {
  case Oneshot::DE_CINNAMON:
  case Oneshot::DE_GNOME:
  case Oneshot::DE_MATE:
  case Oneshot::DE_DEEPIN: {
    std::stringstream hexColor;
    hexColor << "#" << std::hex << color;
    g_settings_set_string(bgsetting, "picture-options", "scaled");
    g_settings_set_string(bgsetting, "primary-color", hexColor.str().c_str());
    g_settings_set_string(bgsetting, "color-shading-type", "solid");
    if (desktop == Oneshot::DE_MATE) {
      g_settings_set_string(bgsetting, "picture-filename",
                            (gameDirStr + path).c_str());
    } else {
      g_settings_set_string(bgsetting, "picture-uri",
                            ("file://" + gameDirStr + path).c_str());
    }
    break;
  }
  case Oneshot::DE_XFCE: {
    auto xfconf_channel_set_string = (xfconf_channel_set_string_fn)dlsym(
        xfconf_dl, "xfconf_channel_set_string");
    auto xfconf_channel_set_int =
        (xfconf_channel_set_int_fn)dlsym(xfconf_dl, "xfconf_channel_set_int");
    auto xfconf_channel_get_property = (xfconf_channel_get_property_fn)dlsym(
        xfconf_dl, "xfconf_channel_get_property");
    auto xfconf_channel_set_property = (xfconf_channel_set_property_fn)dlsym(
        xfconf_dl, "xfconf_channel_set_property");

    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;
    unsigned int ur = r * 256 + r;
    unsigned int ug = g * 256 + g;
    unsigned int ub = b * 256 + b;
    unsigned int alpha = 65535;
    std::string concatPath(gameDirStr + path);
    xfconf_channel_set_string(bgchannel, optionImage.c_str(),
                              concatPath.c_str());
    xfconf_channel_set_int(bgchannel, optionColorStyle.c_str(), 0);
    xfconf_channel_set_int(bgchannel, optionImageStyle.c_str(), 4);
    GValue colorValue = G_VALUE_INIT;
    GPtrArray *colorArr = g_ptr_array_sized_new(4);
    GType colorArrType = g_type_from_name("GPtrArray_GValue_");
    if (!colorArrType) {
      std::stringstream colorCommand;
      colorCommand << "xfconf-query -c xfce4-desktop -n -p " << optionColor
                   << " -t uint -t uint -t uint -t uint -s " << ub << " -s "
                   << ug << " -s " << ub << " -s " << alpha;
      int colorCommandRes = system(colorCommand.str().c_str());
      defColorExists = xfconf_channel_get_property(
          bgchannel, optionColor.c_str(), &defColor);
      colorArrType = g_type_from_name("GPtrArray_GValue_");
      if (!colorArrType) {
        // Let's do some debug output here and skip changing the color
        Debug() << "WALLPAPER ERROR: xfconf-query call returned"
                << colorCommandRes;
        return;
      }
    }
    g_value_init(&colorValue, colorArrType);
    GValue *vr = g_new0(GValue, 1);
    GValue *vg = g_new0(GValue, 1);
    GValue *vb = g_new0(GValue, 1);
    GValue *va = g_new0(GValue, 1);
    g_value_init(vr, G_TYPE_UINT);
    g_value_init(vg, G_TYPE_UINT);
    g_value_init(vb, G_TYPE_UINT);
    g_value_init(va, G_TYPE_UINT);
    g_value_set_uint(vr, ur);
    g_value_set_uint(vg, ug);
    g_value_set_uint(vb, ub);
    g_value_set_uint(va, alpha);
    g_ptr_array_add(colorArr, vr);
    g_ptr_array_add(colorArr, vg);
    g_ptr_array_add(colorArr, vb);
    g_ptr_array_add(colorArr, va);
    g_value_set_boxed(&colorValue, colorArr);
    xfconf_channel_set_property(bgchannel, optionColor.c_str(), &colorValue);
    break;
  }
  case Oneshot::DE_KDE: {
    std::stringstream command;
    std::string concatPath(gameDirStr + path);
    concatPath = std::regex_replace(concatPath, std::regex("\\"), "\\\\");
    concatPath = std::regex_replace(concatPath, std::regex("\""), "\\\"");
    concatPath = std::regex_replace(concatPath, std::regex("'"), "\\x27");

    command << "qdbus org.kde.plasmashell /PlasmaShell "
               "org.kde.PlasmaShell.evaluateScript 'string:"
            << "var allDesktops = desktops();"
            << "for (var i = 0, l = allDesktops.length; i < l; ++i) {"
            << "var d = allDesktops[i];"
            << "d.wallpaperPlugin = \"org.kde.image\";"
            << "d.currentConfigGroup = [\"Wallpaper\", \"org.kde.image\", "
               "\"General\"];"
            << "d.writeConfig(\"Image\", \"file://" << concatPath << "\");"
            << "d.writeConfig(\"FillMode\", \"6\");"
            << "d.writeConfig(\"Blur\", false);"
            << "d.writeConfig(\"Color\", [\""
            << std::to_string((color >> 16) & 0xFF) << "\", \""
            << std::to_string((color >> 8) & 0xFF) << "\", \""
            << std::to_string(color & 0xFF) << "\"]);" << "}" << "'";
    Debug() << "Wallpaper command:" << command.str();
    int result = system(command.str().c_str());
    Debug() << "Result:" << result;
    break;
  }
  case Oneshot::DE_LXDE:
  case Oneshot::DE_FALLBACK:
    std::ifstream srcHint(gameDirStr + path);
    std::ofstream dstHint(fallbackPath);
    dstHint << srcHint.rdbuf();
    srcHint.close();
    dstHint.close();
    break;
  }
}

void DesktopEnv::reset_wallpaper() {
  switch (desktop) {

  case Oneshot::DE_GNOME:
  case Oneshot::DE_CINNAMON:
  case Oneshot::DE_MATE:
  case Oneshot::DE_DEEPIN: {
    if (desktop == Oneshot::DE_MATE) {
      g_settings_set_string(bgsetting, "picture-filename",
                            defPictureURI.c_str());
    } else {
      g_settings_set_string(bgsetting, "picture-uri", defPictureURI.c_str());
    }
    g_settings_set_string(bgsetting, "picture-options",
                          defPictureOptions.c_str());
    g_settings_set_string(bgsetting, "primary-color", defPrimaryColor.c_str());
    g_settings_set_string(bgsetting, "color-shading-type",
                          defColorShading.c_str());
    break;
  }
  case Oneshot::DE_XFCE: {
    auto xfconf_channel_set_string = (xfconf_channel_set_string_fn)dlsym(
        xfconf_dl, "xfconf_channel_set_string");
    auto xfconf_channel_set_int =
        (xfconf_channel_set_int_fn)dlsym(xfconf_dl, "xfconf_channel_set_int");
    auto xfconf_channel_set_property = (xfconf_channel_set_property_fn)dlsym(
        xfconf_dl, "xfconf_channel_set_property");
    auto xfconf_channel_reset_property =
        (xfconf_channel_reset_property_fn)dlsym(
            xfconf_dl, "xfconf_channel_reset_property");

    if (defColorExists) {
      xfconf_channel_set_property(bgchannel, optionColor.c_str(), &defColor);
    } else {
      xfconf_channel_reset_property(bgchannel, optionColor.c_str(), false);
    }
    if (defPictureURI == "") {
      xfconf_channel_reset_property(bgchannel, optionImage.c_str(), false);
    } else {
      xfconf_channel_set_string(bgchannel, optionImage.c_str(),
                                defPictureURI.c_str());
    }
    if (defPictureStyle == -1) {
      xfconf_channel_reset_property(bgchannel, optionImageStyle.c_str(), false);
    } else {
      xfconf_channel_set_int(bgchannel, optionImageStyle.c_str(),
                             defPictureStyle);
    }
    if (defColorStyle == -1) {
      xfconf_channel_reset_property(bgchannel, optionColorStyle.c_str(), false);
    } else {
      xfconf_channel_set_int(bgchannel, optionColorStyle.c_str(),
                             defColorStyle);
    }
    break;
  }
  case Oneshot::DE_KDE: {
    std::stringstream command;
    command << "qdbus org.kde.plasmashell /PlasmaShell "
               "org.kde.PlasmaShell.evaluateScript 'string:"
            << "var allDesktops = desktops();" << "var data = {";
    // Plugin, picture, color, mode, blur
    for (auto const &x : defPlugins) {
      command << "\"" << x.first << "\": {"
              << "plugin: \"" << x.second << "\"";
      if (defPictures.find(x.first) != defPictures.end()) {
        std::string picture = defPictures[x.first];
        picture = std::regex_replace(picture, std::regex("\\"), "\\\\");
        picture = std::regex_replace(picture, std::regex("\""), "\\\"");
        picture = std::regex_replace(picture, std::regex("'"), "\\x27");
        command << ", picture: \"" << picture << "\"";
      }
      if (defColors.find(x.first) != defColors.end()) {
        command << ", color: \"" << defColors[x.first] << "\"";
      }
      if (defModes.find(x.first) != defModes.end()) {
        command << ", mode: \"" << defModes[x.first] << "\"";
      }
      if (defBlurs.find(x.first) != defBlurs.end() && defBlurs[x.first]) {
        command << ", blur: true";
      }
      command << "},";
    }
    command << "\"no\": {}};"
            << "for (var i = 0, l = allDesktops.length; i < l; ++i) {"
            << "var d = allDesktops[i];" << "var dat = data[d.id];"
            << "d.wallpaperPlugin = dat.plugin;"
            << "d.currentConfigGroup = [\"Wallpaper\", \"org.kde.image\", "
               "\"General\"];"
            << "if (dat.picture) {" << "d.writeConfig(\"Image\", dat.picture);"
            << "}" << "if (dat.color) {"
            << "d.writeConfig(\"Color\", dat.color.split(\",\"));" << "}"
            << "if (dat.mode) {" << "d.writeConfig(\"FillMode\", dat.mode);"
            << "}" << "if (dat.blur) {" << "d.writeConfig(\"Blur\", dat.blur);"
            << "}" << "}" << "'";
    Debug() << "Reset wallpaper command:" << command.str();
    int result = system(command.str().c_str());
    Debug() << "Reset result:" << result;
    break;
  }
  case Oneshot::DE_LXDE:
  case Oneshot::DE_FALLBACK: {
    if (remove(fallbackPath.c_str()) != 0) {
      Debug() << "Failed to delete:" << fallbackPath;
    }
    break;
  }
  }
}

#endif

RB_METHOD(wallpaperSet) {
  RB_UNUSED_PARAM;
  const char *name;
  int color;
  rb_get_args(argc, argv, "zi", &name, &color RB_ARG_END);
  std::string path;
  std::string nameStr = name;
#ifdef _WIN32
  path = "Wallpaper\\" + nameStr + ".bmp";
  Debug() << "Setting wallpaper to" << path;
  // Crapify the slashes
  size_t index = 0;
  for (;;) {
    index = path.find("/", index);
    if (index == std::string::npos)
      break;
    path.replace(index, 1, "\\");
    index += 1;
  }
  WCHAR imgnameW[MAX_PATH];
  WCHAR imgnameFull[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, imgnameW, MAX_PATH);
  GetFullPathNameW(imgnameW, MAX_PATH, imgnameFull, NULL);

  int colorId = COLOR_BACKGROUND;
  WCHAR zero[2] = L"0";
  DWORD zeroSize = 4;

  HKEY hKey = NULL;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_READ,
                    &hKey) != ERROR_SUCCESS)
    goto end;

  if (!isCached) {
    // QUERY

    // Style
    setStyle =
        RegQueryValueExW(hKey, L"WallpaperStyle", 0, NULL, (LPBYTE)(szStyle),
                         &szStyleSize) == ERROR_SUCCESS;

    // Tile
    setTile = RegQueryValueExW(hKey, L"TileWallpaper", 0, NULL,
                               (LPBYTE)(szTile), &szTileSize) == ERROR_SUCCESS;

    // File path
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, (PVOID)szFile,
                               0))
      goto end;

    // Color
    oldcolor = GetSysColor(COLOR_BACKGROUND);

    isCached = true;
  }

  RegCloseKey(hKey);
  hKey = NULL;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0, KEY_WRITE,
                    &hKey) != ERROR_SUCCESS)
    goto end;

  // SET

  // Set the style
  if (RegSetValueExW(hKey, L"WallpaperStyle", 0, REG_SZ, (const BYTE *)zero,
                     zeroSize) != ERROR_SUCCESS)
    goto end;

  if (RegSetValueExW(hKey, L"TileWallpaper", 0, REG_SZ, (const BYTE *)zero,
                     zeroSize) != ERROR_SUCCESS)
    goto end;

  // Set the wallpaper
  if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)imgnameFull,
                             SPIF_UPDATEINIFILE))
    goto end;

  // Set the color
  if (!SetSysColors(1, &colorId, (const COLORREF *)&color))
    goto end;
end:
  if (hKey)
    RegCloseKey(hKey);
#else
  std::string nameFix(name);
  std::size_t found = nameFix.find("w32");
  if (found != std::string::npos) {
    nameFix.replace(nameFix.end() - 3, nameFix.end(), "unix");
  }
  path = "/Wallpaper/" + nameFix + ".png";

  Debug() << "Setting wallpaper to " << path;

#ifdef __APPLE__
  if (!isCached) {
    MacDesktop::CacheCurrentBackground();
    isCached = true;
  }
  MacDesktop::ChangeBackground(
      shState->config().gameFolder + path, ((color >> 16) & 0xFF) / 255.0,
      ((color >> 8) & 0xFF) / 255.0, (color & 0xFF) / 255.0);
#else
  char gameDir[PATH_MAX];
  if (getcwd(gameDir, sizeof(gameDir)) == NULL) {
    return Qnil;
  }
  std::string gameDirStr(gameDir);
  env->set_wallpaper(path, gameDirStr, color);

#endif
#endif
  return Qnil;
}

RB_METHOD(wallpaperReset) {
  RB_UNUSED_PARAM;
#ifdef _WIN32
  if (isCached) {
    int colorId = COLOR_BACKGROUND;
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0,
                      KEY_WRITE, &hKey) != ERROR_SUCCESS)
      goto end;

    // Set the style
    if (setStyle)
      RegSetValueExW(hKey, L"WallpaperStyle", 0, REG_SZ, (const BYTE *)szStyle,
                     szStyleSize);

    if (setTile)
      RegSetValueExW(hKey, L"TileWallpaper", 0, REG_SZ, (const BYTE *)szTile,
                     szTileSize);

    // Set the wallpaper
    if (!SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, (PVOID)szFile,
                               SPIF_UPDATEINIFILE))
      goto end;

    // Set the color
    if (!SetSysColors(1, &colorId, (const COLORREF *)&oldcolor))
      goto end;
  end:
    if (hKey)
      RegCloseKey(hKey);
  }
#else
#ifdef __APPLE__
  MacDesktop::ResetBackground();
#else
  env->reset_wallpaper();
#endif
#endif
  return Qnil;
}

void oneshotWallpaperBindingInit() {
#ifdef __linux
  env = new DesktopEnv();
#endif
  VALUE module = rb_define_module("Wallpaper");

  // Functions
  _rb_define_module_function(module, "set", wallpaperSet);
  _rb_define_module_function(module, "reset", wallpaperReset);
}

#ifdef __linux__
void oneshotWallpaperBindingTerminate() { delete env; }
#endif