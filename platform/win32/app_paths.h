#pragma once

#include <string>

namespace ep::win32 {

// Resolves where editor++ keeps its portable state. Prefers the directory
// next to the .exe; if that's not writable (e.g. installed under
// Program Files), falls back to %APPDATA%\editor++\ instead -- but never
// touches the registry. The result is cached after the first call.
const std::string& AppDataDir();

std::string ConfigIniPath();   // <AppDataDir>\editor++.ini
std::string SessionIniPath();  // <AppDataDir>\backup\session.ini
std::string BackupDir();       // <AppDataDir>\backup\  (created if missing)

}  // namespace ep::win32
