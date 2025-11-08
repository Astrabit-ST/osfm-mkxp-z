#include "binding-util.h"
#include "eventthread.h"
#include "sharedstate.h"
#include "debugwriter.h"
#include <cstdio>

#define NIKO_X (320 - 16)
#define NIKO_Y ((13 * 16) * 2)

#include <filesystem>
#ifdef __WIN32__
#include <process.h>

static std::wstring utf8ToWide(const char *str)
{
	std::wstring ret;
	if (str && str[0] != '\0') {
		int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
		if (size > 0) {
			wchar_t *wStr = new wchar_t[size];
			if (MultiByteToWideChar(CP_UTF8, 0, str, -1, wStr, size) == size)
				ret = wStr;
			delete [] wStr;
		}
	}
	return ret;
}

#else
#include <unistd.h>
#endif

RB_METHOD(nikoPrepare) {
  RB_UNUSED_PARAM;

  // Blank

  return Qnil;
}

int niko_process_fun() { return 0; }

RB_METHOD(nikoStart) {
  RB_UNUSED_PARAM;

  // Calculate where to stick the window
  // Top-left area of client (hopefully)
  int x, y;
  SDL_GetWindowPosition(shState->rtData().window, &x, &y);
  x += NIKO_X;
  y += NIKO_Y;

  // there was a bunch of pipe junk that is not used at all, so that is all
  // removed

  std::string pwd = std::filesystem::current_path().string();
  std::string dir = pwd;

#ifdef __WIN32__
  dir += "\\_______.exe";
#endif
#ifdef __linux__
  dir += "/_______";
#endif

  std::string window_x = std::to_string(x);
  std::string window_y = std::to_string(y);
  char* const args[] = {
      const_cast<char*>(dir.c_str()), const_cast<char*>(window_x.c_str()),
      const_cast<char*>(window_y.c_str())};

#ifdef __WIN32__
	std::wstring wPath = utf8ToWide(dir.c_str());
	std::wstring wCwd = utf8ToWide(pwd.c_str());
  
	wchar_t wArgs[512] = {'\0'};
	swprintf(wArgs, sizeof(wArgs), L"\"%ls\" %ld %ld", wPath.c_str(), x, y);

	// Start process
	STARTUPINFOW si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);

	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));

	BOOL result = CreateProcessW(wPath.c_str(), wArgs, NULL, NULL, FALSE, 0, NULL, wCwd.c_str(), &si, &pi);
	if (!result){
		Debug() << "Failed to start Journal! Error:" << GetLastError();
  }
#else
  pid_t pid = fork();
  if (pid == 0) {
    execv(dir.c_str(), args);
  }
#endif

  return Qnil;
}

void oneshotNikoBindingInit() {
  VALUE module = rb_define_module("Niko");

  // Niko:: module functions
  _rb_define_module_function(module, "get_ready", nikoPrepare);
  _rb_define_module_function(module, "do_your_thing", nikoStart);
}
