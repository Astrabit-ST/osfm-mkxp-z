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
      *pathend = L'\0';
    }
    if (_wchdir(oneshotDir)) {
      MessageBoxW(NULL,
                  L"Changing working directory failed. This should never "
                  L"happen.\nFind Melody and beat her with a stick.",
                  L"ModShot Shim", MB_ICONERROR);
      printf("chdir errno: %d", errno);
    }
  }

  if (argc != 0) {
    argv[0] = ARGV0;
  }

  _wexecv(GAME_LAUNCH_NAME, (const wchar_t *const *)argv);

  MessageBoxW(NULL,
              L"Cannot start ModShot for some reason.\nPlease check your "
              L"ModShot installation.",
              L"ModShot Shim", MB_ICONERROR);

  return 1;
}