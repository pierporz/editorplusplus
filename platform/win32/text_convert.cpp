#include "text_convert.h"

#include <windows.h>

#include <utility>

namespace ep::win32 {

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) return std::wstring();
  int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                    static_cast<int>(utf8.size()), nullptr, 0);
  if (needed <= 0) return std::wstring();
  std::wstring wide(static_cast<size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                       wide.data(), needed);
  return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
  if (wide.empty()) return std::string();
  int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(),
                                    static_cast<int>(wide.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (needed <= 0) return std::string();
  std::string utf8(static_cast<size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                       utf8.data(), needed, nullptr, nullptr);
  return utf8;
}

namespace {

std::wstring AnsiToWide(const std::string& ansi) {
  if (ansi.empty()) return std::wstring();
  int needed = MultiByteToWideChar(CP_ACP, 0, ansi.data(),
                                    static_cast<int>(ansi.size()), nullptr, 0);
  if (needed <= 0) return std::wstring();
  std::wstring wide(static_cast<size_t>(needed), L'\0');
  MultiByteToWideChar(CP_ACP, 0, ansi.data(), static_cast<int>(ansi.size()),
                       wide.data(), needed);
  return wide;
}

std::string WideToAnsi(const std::wstring& wide) {
  if (wide.empty()) return std::string();
  int needed = WideCharToMultiByte(CP_ACP, 0, wide.data(),
                                    static_cast<int>(wide.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (needed <= 0) return std::string();
  std::string ansi(static_cast<size_t>(needed), '\0');
  WideCharToMultiByte(CP_ACP, 0, wide.data(), static_cast<int>(wide.size()),
                       ansi.data(), needed, nullptr, nullptr);
  return ansi;
}

// Windows' native wchar_t storage is little-endian UTF-16, so the LE file
// byte order maps directly onto it with no swapping.
std::wstring Utf16LEBytesToWide(const std::string& bytes) {
  return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data()), bytes.size() / 2);
}

std::string WideToUtf16LEBytes(const std::wstring& wide) {
  return std::string(reinterpret_cast<const char*>(wide.data()), wide.size() * 2);
}

std::string SwapUtf16ByteOrder(const std::string& bytes) {
  std::string swapped = bytes;
  for (size_t i = 0; i + 1 < swapped.size(); i += 2) std::swap(swapped[i], swapped[i + 1]);
  return swapped;
}

}  // namespace

std::string AnsiToUtf8(const std::string& ansi_bytes) { return WideToUtf8(AnsiToWide(ansi_bytes)); }

std::string Utf8ToAnsi(const std::string& utf8) { return WideToAnsi(Utf8ToWide(utf8)); }

std::string Utf16LEBytesToUtf8(const std::string& utf16le_bytes) {
  return WideToUtf8(Utf16LEBytesToWide(utf16le_bytes));
}

std::string Utf8ToUtf16LEBytes(const std::string& utf8) {
  return WideToUtf16LEBytes(Utf8ToWide(utf8));
}

std::string Utf16BEBytesToUtf8(const std::string& utf16be_bytes) {
  return WideToUtf8(Utf16LEBytesToWide(SwapUtf16ByteOrder(utf16be_bytes)));
}

std::string Utf8ToUtf16BEBytes(const std::string& utf8) {
  return SwapUtf16ByteOrder(WideToUtf16LEBytes(Utf8ToWide(utf8)));
}

}  // namespace ep::win32
