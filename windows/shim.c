#include <errno.h>
#include <stdio.h>
#include <wchar.h>
#include <windows.h>

wchar_t *ARGV0 = L"lib\\oneshot.exe";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  (void)hInstance;
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nCmdShow;

  int argc;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  HMODULE hModule = GetModuleHandle(NULL);

  if (hModule != NULL) {
    wchar_t oneshotDir[MAX_PATH];
    GetModuleFileNameW(hModule, oneshotDir, sizeof(oneshotDir));
    wchar_t *pathend = wcsrchr(oneshotDir, L'\\');

    if (pathend != NULL) {
      *pathend = '\0';
    }
    if (_wchdir(oneshotDir)) {
      char msg[512];
      snprintf(msg, 512, "Changing working directory failed. This should never happen.\nFind Melody and beat her with a stick.\nError code: %s", strerror(errno));
      MessageBox(NULL, msg, "ModShot Shim", MB_ICONERROR);
    }
  }

  if (argc != 0) {
    argv[0] = ARGV0;
  }

  _wexecv(L"lib\\oneshot.exe", (const wchar_t *const *)argv);

  char msg[512];
  snprintf(msg, 512, "Cannot start ModShot for some reason.\nPlease check your ModShot installation.\nError code: %s", strerror(errno));

  MessageBox(NULL, msg, "ModShot Shim", MB_ICONERROR);

  return 1;
}