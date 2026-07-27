# editor++

Portable Windows text editor, native C++17 on Win32 + Scintilla. Replaces
Notepad++ for personal use: same basic functionality, zero bloat, instant
startup. See `CLAUDE.md` for the full spec and roadmap.

## Build

Developed on Linux, targeting Windows via MinGW-w64 cross-compilation.

```
make test   # native build + run of core/ unit tests (<10s)
make win    # cross-compile build/editor++.exe
make smoke  # make win, then launch under Wine/Xvfb to catch startup crashes
make clean
make all    # test + win
```

Requires `g++`, `x86_64-w64-mingw32-g++`, `wine`, `xvfb-run`.
