#include "file_io.h"

#include <windows.h>

#include "platform/win32/text_convert.h"

namespace ep::win32 {

namespace {

struct HandleGuard {
  HANDLE h;
  ~HandleGuard() {
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
  }
};

}  // namespace

ep::Result<std::string> ReadFileBytes(const std::string& utf8_path) {
  std::wstring wpath = Utf8ToWide(utf8_path);
  HandleGuard guard{CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                 nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (guard.h == INVALID_HANDLE_VALUE) {
    return ep::Fail<std::string>("cannot open file for reading");
  }

  LARGE_INTEGER size;
  if (!GetFileSizeEx(guard.h, &size)) {
    return ep::Fail<std::string>("cannot query file size");
  }

  std::string data(static_cast<size_t>(size.QuadPart), '\0');
  size_t total_read = 0;
  while (total_read < data.size()) {
    DWORD chunk = 0;
    DWORD to_read = static_cast<DWORD>(
        (data.size() - total_read) > (1u << 30) ? (1u << 30)
                                                  : (data.size() - total_read));
    if (!ReadFile(guard.h, data.data() + total_read, to_read, &chunk,
                  nullptr)) {
      return ep::Fail<std::string>("read error");
    }
    if (chunk == 0) break;
    total_read += chunk;
  }
  data.resize(total_read);
  return ep::Ok(std::move(data));
}

ep::Result<void> WriteFileAtomic(const std::string& utf8_path,
                                  const std::string& bytes) {
  std::wstring wpath = Utf8ToWide(utf8_path);
  std::wstring wtmp = wpath + L".tmp";

  {
    HandleGuard guard{CreateFileW(wtmp.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                   nullptr)};
    if (guard.h == INVALID_HANDLE_VALUE) {
      return ep::Fail("cannot create temp file");
    }

    size_t total_written = 0;
    while (total_written < bytes.size()) {
      DWORD chunk = 0;
      DWORD to_write = static_cast<DWORD>((bytes.size() - total_written) >
                                                   (1u << 30)
                                               ? (1u << 30)
                                               : (bytes.size() - total_written));
      if (!WriteFile(guard.h, bytes.data() + total_written, to_write, &chunk,
                     nullptr)) {
        return ep::Fail("write error");
      }
      total_written += chunk;
    }
    if (!FlushFileBuffers(guard.h)) {
      return ep::Fail("flush error");
    }
  }

  if (!MoveFileExW(wtmp.c_str(), wpath.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(wtmp.c_str());
    return ep::Fail("cannot replace target file");
  }
  return ep::Ok();
}

ep::Result<LoadedTextFile> ReadTextFileForEditing(const std::string& utf8_path) {
  auto bytes = ReadFileBytes(utf8_path);
  if (!bytes) return ep::Fail<LoadedTextFile>(bytes.Err().message);

  ep::EncodingDetection detection = ep::DetectEncoding(bytes.Value());
  std::string payload = bytes.Value().substr(detection.bom_length);

  LoadedTextFile result;
  result.encoding = detection.encoding;
  switch (detection.encoding) {
    case ep::Encoding::Ansi:
      result.text = AnsiToUtf8(payload);
      break;
    case ep::Encoding::Utf16LE:
      result.text = Utf16LEBytesToUtf8(payload);
      break;
    case ep::Encoding::Utf16BE:
      result.text = Utf16BEBytesToUtf8(payload);
      break;
    case ep::Encoding::Utf8:
    case ep::Encoding::Utf8Bom:
      result.text = std::move(payload);
      break;
  }
  return ep::Ok(std::move(result));
}

std::string EncodeForWriting(const std::string& utf8_text, ep::Encoding encoding) {
  switch (encoding) {
    case ep::Encoding::Utf8Bom:
      return "\xEF\xBB\xBF" + utf8_text;
    case ep::Encoding::Ansi:
      return Utf8ToAnsi(utf8_text);
    case ep::Encoding::Utf16LE:
      return Utf8ToUtf16LEBytes(utf8_text);
    case ep::Encoding::Utf16BE:
      return Utf8ToUtf16BEBytes(utf8_text);
    case ep::Encoding::Utf8:
    default:
      return utf8_text;
  }
}

}  // namespace ep::win32
