#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <string>

#include "platform/win32/main_window.h"
#include "platform/win32/text_convert.h"

extern "C" int Scintilla_RegisterClasses(void* hInstance);
extern "C" int Scintilla_ReleaseResources();

namespace {

// Joins argv[1..argc) with '\n' (illegal in Windows paths, so safe as a
// delimiter) for a WM_COPYDATA payload to an already-running instance.
std::wstring JoinArgs(int argc, LPWSTR* argv) {
  std::wstring joined;
  for (int i = 1; i < argc; i++) {
    if (!joined.empty()) joined.push_back(L'\n');
    joined += argv[i];
  }
  return joined;
}

// If another instance is already running, forwards this process's files to
// it (bringing it to the foreground) and returns true so the caller can
// exit immediately without creating a window.
bool ForwardToExistingInstance(HANDLE mutex, int argc, LPWSTR* argv) {
  if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
  CloseHandle(mutex);

  HWND existing = FindWindowW(ep::win32::MainWindow::ClassName(), nullptr);
  if (!existing) return true;  // stale mutex owner gone; just don't start a duplicate

  DWORD owner_pid = 0;
  GetWindowThreadProcessId(existing, &owner_pid);
  AllowSetForegroundWindow(owner_pid);

  std::wstring joined = JoinArgs(argc, argv);
  if (!joined.empty()) {
    COPYDATASTRUCT cds{};
    cds.dwData = ep::win32::MainWindow::kCopyDataMagic;
    cds.cbData = static_cast<DWORD>((joined.size() + 1) * sizeof(wchar_t));
    cds.lpData = joined.data();
    SendMessageW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds));
  } else {
    SendMessageW(existing, WM_COPYDATA, 0, 0);  // just bring it to the foreground
  }
  return true;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
                     PWSTR /*pCmdLine*/, int nCmdShow) {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"editorpp_single_instance_mutex");
  if (mutex && ForwardToExistingInstance(mutex, argc, argv)) {
    if (argv) LocalFree(argv);
    return 0;
  }

  INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_TAB_CLASSES | ICC_BAR_CLASSES};
  InitCommonControlsEx(&icc);

  Scintilla_RegisterClasses(static_cast<void*>(hInstance));

  ep::win32::MainWindow window;
  if (!window.Create(hInstance, nCmdShow)) {
    return 1;
  }

  if (argv) {
    for (int i = 1; i < argc; i++) {
      window.OpenFileFromPath(ep::win32::WideToUtf8(argv[i]));
    }
    LocalFree(argv);
  }

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    if (window.TranslateFindDialogMessage(msg)) continue;
    if (!TranslateAcceleratorW(window.Handle(), window.Accelerators(), &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  if (mutex) CloseHandle(mutex);
  Scintilla_ReleaseResources();
  return static_cast<int>(msg.wParam);
}
