# CLAUDE.md — editor++

> Spec operativa per Claude Code. Leggi **tutto** questo file prima di scrivere codice.
> Se una richiesta dell'utente contraddice questo file, segui l'utente ma segnalalo.

---

## 1. Cos'è

editor++ è un editor di testo **portable per Windows**, scritto in C++17 nativo su Win32 API + Scintilla.
Sostituisce Notepad++ per uso personale: stesse funzioni base, zero bloat, avvio istantaneo.

**Non negoziabile:**
- Singolo `.exe` statico, nessuna DLL esterna, nessun installer, nessuna scrittura nel registro.
- Avvio a freddo **< 100 ms**, `.exe` **< 4 MB**, RAM a riposo **< 20 MB**.
- Zero dipendenze runtime oltre a Scintilla/Lexilla (vendorizzate nel repo).
- Tutta la configurazione in `editor++.ini` **accanto all'eseguibile**.

**Vietato senza approvazione esplicita:** Qt, wxWidgets, MFC, .NET, Electron, Boost, qualunque
package manager (vcpkg/conan), qualunque libreria che aggiunga > 200 KB al binario.

---

## 2. Ambiente di sviluppo — LEGGI ATTENTAMENTE

Lo sviluppo avviene su una **macchina Linux**. Il target è **Windows**. Conseguenze operative:

| Cosa | Come |
|---|---|
| Build Windows | Cross-compile MinGW-w64 (`x86_64-w64-mingw32-g++`) |
| Test logica (`core/`) | Compilazione **nativa Linux** + esecuzione unit test |
| Test GUI (`platform/`) | Solo compilazione. **Non puoi vederla.** |
| Smoke test | `xvfb-run -a wine build/editor++.exe` — becca crash all'avvio |
| Test visivo finale | Lo fa l'utente su Windows |

### Regola architetturale che ne deriva (la più importante del file)

**`core/` non deve contenere una singola riga di Win32.** Niente `<windows.h>`, niente `HWND`,
niente `TCHAR`, niente `WCHAR` API-specifico. Solo C++17 standard.
Se puoi testarlo su Linux, deve stare in `core/`.

Quando implementi una feature, chiediti sempre: *"la parte logica può stare in core/?"*
La risposta è quasi sempre sì. Base64, pretty print, parsing INI, serializzazione sessione,
rilevamento encoding/EOL, conteggio statistiche: **tutto core/, tutto testato**.

`platform/win32/` contiene solo: creazione finestre, message loop, menu, toolbar, tab control,
dialog nativi, clipboard, I/O file. Deve essere sottile e stupido.

---

## 3. Struttura del repo

```
editor++/
├── CLAUDE.md
├── Makefile
├── README.md
├── core/                      # C++17 puro, ZERO Win32, testabile su Linux
│   ├── base64.{h,cpp}
│   ├── json_pretty.{h,cpp}    # parser+formatter JSON scritto a mano
│   ├── xml_pretty.{h,cpp}     # formatter XML scritto a mano
│   ├── sql_pretty.{h,cpp}     # formatter SQL scritto a mano
│   ├── encoding.{h,cpp}       # BOM detect, UTF-8 validate, EOL detect/convert
│   ├── text_stats.{h,cpp}     # conteggi caratteri/righe/selezione
│   ├── session.{h,cpp}        # serializzazione stato tab -> stringa
│   ├── ini.{h,cpp}            # parser INI minimale
│   └── result.h               # Result<T> / Error, nessuna eccezione
├── platform/win32/
│   ├── main.cpp               # WinMain, message loop, single-instance
│   ├── main_window.{h,cpp}    # frame, menu, layout, resize
│   ├── tab_bar.{h,cpp}        # SysTabControl32 + drag reorder + close button
│   ├── editor.{h,cpp}         # wrapper Scintilla (1 istanza per documento)
│   ├── document.{h,cpp}       # buffer: path, dirty, encoding, EOL, undo state
│   ├── status_bar.{h,cpp}
│   ├── toolbar.{h,cpp}
│   ├── find_replace.{h,cpp}   # dialog non modale
│   ├── file_io.{h,cpp}        # lettura/scrittura atomica, path lunghi
│   └── resource.{h,rc}        # menu, accel, icone, manifest
├── third_party/
│   ├── scintilla/             # vendorizzato, versione pinnata
│   └── lexilla/
├── tests/
│   ├── test_main.cpp          # runner minimale header-only, no gtest
│   ├── test_base64.cpp
│   ├── test_json_pretty.cpp
│   ├── test_xml_pretty.cpp
│   ├── test_sql_pretty.cpp
│   ├── test_encoding.cpp
│   └── test_session.cpp
└── build/                     # gitignored
```

---

## 4. Build

Il `Makefile` deve esporre esattamente questi target:

```make
make test      # compila core/ + tests/ con g++ NATIVO e li esegue. Deve girare in < 10s.
make win       # cross-compila editor++.exe con MinGW-w64
make smoke     # make win && xvfb-run -a wine build/editor++.exe (avvio + chiusura)
make clean
make all       # test + win
```

Flag obbligatori:

```
CXXFLAGS_COMMON = -std=c++17 -O2 -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti
WINFLAGS        = -municode -mwindows -static -static-libgcc -static-libstdc++ \
                  -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX
LDFLAGS_WIN     = -lcomctl32 -lcomdlg32 -lshlwapi -lole32 -luuid -lgdi32 -luser32
```

Nota: Scintilla richiede eccezioni/RTTI in alcuni punti — compila `third_party/` con i suoi
flag separati, non con `-fno-exceptions`. Il nostro codice resta senza eccezioni.

**Prima di dichiarare completo qualunque task, esegui `make test` e `make win`.
Se uno dei due fallisce, il task non è finito.**

---

## 5. Feature richieste

### 5.1 File
- Nuovo, Apri (multi-selezione), Salva, Salva con nome, Chiudi, Chiudi tutto, Chiudi gli altri.
- Drag & drop di file sulla finestra.
- Apertura da riga di comando: `editor++.exe file1.txt file2.json`.
- Single instance: se già aperto, i file vanno in tab nuove dell'istanza esistente (`WM_COPYDATA`).
- Scrittura **atomica**: scrivi su `file.tmp`, poi `MoveFileEx` con `REPLACE_EXISTING`.

### 5.2 Tab + sessione persistente
- Tab riordinabili con drag, chiusura con tasto centrale e con la X.
- `Ctrl+Tab` / `Ctrl+Shift+Tab`, `Ctrl+W`.
- **Alla riapertura ripristina esattamente lo stato precedente:** file aperti, ordine,
  tab attiva, posizione cursore, scroll, selezione.
- **I buffer non salvati sopravvivono alla chiusura**, come Notepad++: il contenuto va in
  `backup/` accanto all'exe, indicizzato in `session.ini`. Nessun prompt "vuoi salvare?"
  alla chiusura per i file mai salvati — li si ritrova e basta.
- Autosave del backup ogni 3 s dopo l'ultima modifica (timer debounced, mai su ogni tasto).

### 5.3 Trova e sostituisci
Dialog **non modale**, con:
- Trova successivo/precedente, Sostituisci, Sostituisci tutto, Conta.
- Ambito: documento corrente / selezione / tutte le tab aperte.
- Opzioni: case sensitive, parola intera, **regex**, wrap around.
- Usa la ricerca nativa di Scintilla (`SCI_SEARCHINTARGET`, flag `SCFIND_REGEXP`), non
  reimplementare il matching.
- Cronologia delle ultime 20 ricerche in combobox, persistita nell'INI.
- `F3` / `Shift+F3` funzionano anche a dialog chiuso.
- Evidenzia tutte le occorrenze con un indicator Scintilla.

### 5.4 Strumenti (menu "Tools")
- **Pretty print JSON** — indenta il documento o la selezione. Parser scritto a mano in
  `core/json_pretty.cpp`. Deve gestire: unicode escape, numeri esponenziali, nesting profondo
  (iterativo, non ricorsivo — niente stack overflow su input ostili), stringhe con caratteri
  di controllo. In caso di JSON invalido: **non modificare nulla**, mostra riga/colonna
  dell'errore nella status bar.
- **Minify JSON** (gratis, stesso parser).
- **Pretty print XML** — indentazione strutturale. Deve gestire: dichiarazione XML, commenti,
  CDATA, tag self-closing, attributi con virgolette miste, namespace. Non validante.
  Preserva il contenuto di CDATA e `<pre>`-like verbatim.
- **Pretty print SQL** — formattatore strutturale non validante in `core/sql_pretty.cpp`
  (tokenizer iterativo, no ricorsione). Va a capo sulle clausole principali (SELECT, FROM,
  WHERE, JOIN e varianti, GROUP BY, ORDER BY, HAVING, UNION [ALL], INSERT INTO, VALUES,
  UPDATE, SET, DELETE FROM, WITH), indenta le subquery tra parentesi, spezza la lista
  colonne di SELECT una per riga, va a capo su AND/OR. Solo le parole chiave riconosciute
  vengono maiuscolizzate; identificatori/nomi funzione restano come scritti. Commenti
  (`--`, `/* */`) e stringhe preservati verbatim.
- **Base64 encode** / **Base64 decode** — su selezione se presente, altrimenti tutto il
  documento. Encode opera sui byte UTF-8. Decode: se il risultato non è UTF-8 valido,
  avvisa nella status bar e non sostituire.

Tutte queste operazioni devono essere **un singolo undo** (`SCI_BEGINUNDOACTION` /
`SCI_ENDUNDOACTION`) e preservare la posizione di scroll.

### 5.5 Visualizzazione
- Pulsante toolbar + voce di menu **"¶"** che alterna la visibilità di:
  spazi, tab, fine riga (CR/LF/CRLF), a tre stati o come toggle unico (toggle unico va bene).
  Scintilla: `SCI_SETVIEWWS`, `SCI_SETVIEWEOL`.
- Toggle: a capo automatico, numeri di riga, indent guides, current line highlight.
- Zoom `Ctrl+rotella`, `Ctrl+0` reset.
- Syntax highlighting via Lexilla: JSON, XML, HTML, SQL, Python, C/C++, JS, INI, Markdown,
  Bash. Rilevamento da estensione, override manuale dal menu Linguaggio.

### 5.6 Status bar
Pannelli fissi, aggiornati su `SCN_UPDATEUI` (mai su ogni carattere):

```
[ Ln 42, Col 17, Pos 1203 ] [ Lunghezza: 8.412  Righe: 231 ] [ Sel: 145 car, 3 righe ] [ UTF-8 ] [ CRLF ] [ INS ]
```

- Encoding e EOL sono **cliccabili** → menu per convertire (UTF-8, UTF-8 BOM, UTF-16LE,
  ANSI/CP1252 · CRLF, LF, CR).
- I conteggi vengono da `core/text_stats.cpp` e sono unit-testati (attenzione: caratteri ≠ byte
  con UTF-8; mostra i caratteri).

---

## 6. Convenzioni di codice

- **C++17**, niente eccezioni e niente RTTI nel nostro codice. Errori via `Result<T>` (`core/result.h`).
- `std::string` = UTF-8 ovunque in `core/`. La conversione a `std::wstring` avviene **solo**
  al confine Win32, in `platform/win32/`, con due helper centralizzati (`Utf8ToWide`, `WideToUtf8`).
  Mai conversioni sparse.
- Naming: `PascalCase` per tipi e funzioni, `snake_case` per variabili locali,
  `m_` prefisso per membri, `k` per costanti (`kMaxTabs`).
- Un file `.cpp` = una responsabilità. Se un file supera **500 righe**, spezzalo.
- Niente singleton, niente variabili globali oltre a `g_hInstance`. Passa le dipendenze.
- RAII per tutte le risorse Win32 (wrapper per `HGDIOBJ`, `HANDLE`, `HFONT`).
- Commenti solo dove il *perché* non è ovvio. Niente commenti che ripetono il codice.
- Ogni funzione in `core/` che ha una logica non banale ha un test corrispondente.

### Performance
- Nessuna allocazione nel percorso di digitazione.
- Le statistiche della status bar si ricalcolano su idle/notifica, non su ogni keystroke.
- File > 10 MB: apri senza lexer e senza word wrap, avvisa nella status bar.
- Non copiare mai l'intero buffer del documento se serve solo un range: usa `SCI_GETTEXTRANGE`.

---

## 7. Configurazione (`editor++.ini`)

Portable: se la directory dell'exe non è scrivibile, fai fallback su `%APPDATA%\editor++\`
e segnalalo, ma **non** usare mai il registro.

```ini
[window]
x=100
y=100
width=1200
height=800
maximized=0

[view]
word_wrap=0
line_numbers=1
show_whitespace=0
show_eol=0
font=Consolas
font_size=11
theme=light

[editor]
tab_width=4
use_spaces=0
auto_indent=1

[find]
history=...
match_case=0
regex=0

[session]
restore_tabs=1
backup_interval_ms=3000
```

`session.ini` è **separato** da `editor++.ini` (stato volatile vs preferenze).

---

## 8. Roadmap a fasi

Lavora **una fase alla volta**. Non iniziare la fase N+1 finché la N non compila,
passa i test e non è stata confermata dall'utente.

**Fase 1 — Fondamenta**
Makefile con i 4 target funzionanti, `core/result.h`, test runner, finestra Win32 vuota con
Scintilla singola istanza, apri/salva/salva-con-nome, menu base. `make smoke` deve passare.

**Fase 2 — Core logic (tutto testato su Linux, zero GUI)**
`base64`, `json_pretty`, `xml_pretty`, `encoding`, `text_stats`, `ini`, `session`.
Ogni modulo con la sua suite di test, inclusi input malformati e casi limite.
*Questa fase è interamente sviluppabile e verificabile su Linux: sfruttala al massimo.*

**Fase 3 — Multi-tab + sessione**
Tab bar, gestione documenti multipli, dirty state, ripristino sessione, backup dei
buffer non salvati.

**Fase 4 — UI completa**
Find/Replace dialog, status bar interattiva, toolbar, toggle whitespace/EOL, cablaggio
dei tool di Fase 2 al menu, syntax highlighting Lexilla, zoom, word wrap.

**Fase 5 — Rifinitura**
Drag&drop, single instance, riga di comando, file recenti, tema scuro, accelerator completi,
gestione file grandi, icona e version info.

**Fase 6 (opzionale, futura) — Porting Linux/GTK**
Possibile *solo* se `core/` è rimasto pulito. Aggiungere `platform/gtk/` a fianco di
`platform/win32/`, riusando `core/` invariato. Non pianificare per questo ora, ma non
precludertelo mai.

---

## 9. Workflow per Claude Code

1. All'inizio di ogni task, dichiara: quale fase, quali file toccherai, quali test aggiungerai.
2. Scrivi il test **prima** dell'implementazione per tutto ciò che sta in `core/`.
3. Esegui `make test` e `make win` prima di dichiarare finito. Riporta l'output reale.
4. Se una feature richiede una libreria esterna, **fermati e chiedi**. Non aggiungerla.
5. Se stai per scrivere `#include <windows.h>` dentro `core/`, hai sbagliato progettazione:
   fermati e ripensa il confine.
6. Commit atomici, un messaggio per feature, in inglese, formato:
   `feat(core): add base64 encoder with UTF-8 validation`
7. Non refactorare codice che non c'entra col task corrente.
8. Se scopri che un requisito di questo file è irrealizzabile o sbagliato, **dillo**
   invece di aggirarlo in silenzio.
9. **Ogni volta che fai una modifica (o una serie di modifiche correlate) a questo
   repository, fai commit e push su GitHub**: `git push origin master` verso
   `git@github.com:pierporz/editorplusplus.git` (remote già configurato come `origin`).
   Questo vale per ogni sessione di lavoro, non solo per la prima: non lasciare mai
   lavoro committato solo in locale senza pusharlo, a meno che l'utente non chieda
   esplicitamente di non farlo per quella sessione.

---

## 10. Definizione di "fatto"

Una feature è completa quando:
- [ ] `make test` passa (se tocca `core/`)
- [ ] `make win` compila senza warning
- [ ] `make smoke` non crasha
- [ ] La logica sta in `core/` ed è testata; `platform/` è solo cablaggio
- [ ] Nessun file supera 500 righe
- [ ] Il binario è ancora < 4 MB
- [ ] L'utente l'ha provata su Windows e ha confermato
- [ ] Commit fatto e pushato su `origin master` (https://github.com/pierporz/editorplusplus)
