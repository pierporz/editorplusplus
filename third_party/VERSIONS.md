# Vendored dependencies

| Library   | Version | Source                                              |
|-----------|---------|------------------------------------------------------|
| Scintilla | 5.6.4   | https://www.scintilla.org/scintilla564.zip           |
| Lexilla   | 5.5.1   | https://www.scintilla.org/lexilla551.zip              |

Trimmed to what editor++ builds: `scintilla/{src,include,win32}`,
`lexilla/{lexlib,access,include,src/Lexilla.cxx}` plus only the lexers used
(Bash, CPP, HTML, JSON, Markdown, Python, SQL, Props). Docs, tests, and other
platform backends (gtk/cocoa/qt) were dropped. Do not hand-edit vendored
sources; re-vendor and re-trim on version bumps.
