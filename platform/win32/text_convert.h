#pragma once

#include <string>

namespace ep::win32 {

// The character-set conversions allowed at the Win32 boundary. core/ and the
// Scintilla buffer only ever see UTF-8; these exist purely to translate to
// and from the byte encodings Win32 APIs and on-disk files use.
std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);

// CP_ACP (the system's single-byte "ANSI" codepage, e.g. CP1252).
std::string AnsiToUtf8(const std::string& ansi_bytes);
std::string Utf8ToAnsi(const std::string& utf8);

// Raw UTF-16 file bytes, in the given endianness -- distinct from
// std::wstring, which is native-endian and not necessarily byte-for-byte
// what's on disk.
std::string Utf16LEBytesToUtf8(const std::string& utf16le_bytes);
std::string Utf8ToUtf16LEBytes(const std::string& utf8);
std::string Utf16BEBytesToUtf8(const std::string& utf16be_bytes);
std::string Utf8ToUtf16BEBytes(const std::string& utf8);

}  // namespace ep::win32
