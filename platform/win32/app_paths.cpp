#include "platform/win32/app_paths.h"

#include <windows.h>

#include <shlobj.h>

#include "platform/win32/text_convert.h"

namespace ep::win32 {

namespace {

std::string ExeDir() {
  wchar_t buf[MAX_PATH];
  DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  std::wstring path(buf, len);
  size_t slash = path.find_last_of(L"\\/");
  return WideToUtf8(slash == std::wstring::npos ? path : path.substr(0, slash));
}

bool IsWritable(const std::string& dir) {
  std::wstring probe = Utf8ToWide(dir) + L"\\.editorpp_write_test";
  HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_TEMPORARY, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  CloseHandle(h);
  DeleteFileW(probe.c_str());
  return true;
}

std::string RoamingAppDataDir() {
  PWSTR path = nullptr;
  std::string result;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path))) {
    result = WideToUtf8(path) + "\\editor++";
    CoTaskMemFree(path);
    CreateDirectoryW(Utf8ToWide(result).c_str(), nullptr);
  }
  return result;
}

std::string ResolveAppDataDir() {
  std::string exe_dir = ExeDir();
  if (IsWritable(exe_dir)) return exe_dir;
  return RoamingAppDataDir();
}

}  // namespace

const std::string& AppDataDir() {
  static const std::string dir = ResolveAppDataDir();
  return dir;
}

std::string ConfigIniPath() { return AppDataDir() + "\\editor++.ini"; }

std::string BackupDir() {
  std::string dir = AppDataDir() + "\\backup";
  CreateDirectoryW(Utf8ToWide(dir).c_str(), nullptr);
  return dir;
}

std::string SessionIniPath() { return BackupDir() + "\\session.ini"; }

}  // namespace ep::win32
