# Command line: read Archicad's command table (design)

**Status:** draft for review, 2026-11-13
**Repository:** fork `DaviiiiiDave/baaz`, branch `feature/dg-command-table`. Not proposed upstream
yet (decided 2026-11-12: build in the fork first, cut an upstream proposal later).
**Archicad versions:** 29 and 30 (both probed)

This design replaces how the command line finds and runs Archicad's commands. It supersedes
the live-scheme addendum (`2026-11-03-command-line-live-scheme-design.md`) completely and the
base spec (`2026-11-02-command-line-design.md`) where section 3 says so.

## 1. Intent

The command line works today in Archicad 29 through a workaround: the user exports the command
scheme once, Baaz reads the menu structure from Archicad's binary scheme file, fills the Win32
menu bar from outside and posts `WM_COMMAND` with the id found at the matching menu position.
Archicad 30 has no Win32 menu bar any more (`GetMenu` on the frame window returns null; the bar
is a child window), so none of this works there.

A probe showed that Archicad keeps a table of all its commands, with names in the UI language,
and runs any of them by a stable id. Baaz reads that table directly, in Archicad 29 and 30, and
the whole workaround goes. The command line should be able to do everything Archicad offers.

## 2. Evidence (probe, 2026-11-12)

A throwaway build added a hidden HUD entry that dumped the table and ran one command. Measured
in German Archicad 29 and Archicad 30, each with a project open. Nothing else was tested.

| # | Question | Result |
| - | -------- | ------ |
| P1 | Is there a table of all commands? | `DG::CommandDescriptor::GetCommandTable()` (`DGCommandDescriptor.hpp`, DevKit of AC29 and AC30): a hash table from `DG::Command` (command id, developer id, product id) to `DG::CommandDescriptor*`. AC29: 1564 entries, AC30: 1448. |
| P2 | Does it have names? | Yes, in the UI language, for all but 2 entries ("Lineal", "Sichern", "Wand"). AC29 getter `GetCleanText()`, AC30 `GetDisplayText()` (renamed; AC29 `GetText`/AC30 `GetRawText` keep the `&`). |
| P3 | What else does an entry say? | `GetCategory()` (the group of *Options › Work Environment › Commands*, for example "04 Ausschnitt", "12 Werkzeuge", "Navigator Kontext Menüs"), `IsVisible()`, `IsEnabled()`, `IsChecked()`, `GetDescription()`. |
| P4 | Does it run a command? | `DG::Application::GetInstance()->PostCommandMessage(DG::Command(1557, 0, 0))` returned true and toggled the ruler, in AC29 and AC30. |
| P5 | Is the key the scheme export's id? | Yes. `<number>-<MDID1>-<MDID2>` of the export equals (command id, developer id, product id). Built-in commands have developer and product id 0; add-on commands carry the add-on's MDID. |
| P6 | Is the key stable across versions? | 1437 keys occur in both versions, 1433 of them with the same name (the rest are rewordings such as "Lern Center" → "Lernzentrum"). |
| P7 | Is `GetOSCommandId()` stable? | No: only 411 of the 1437 shared keys keep it. For the ruler it is 255, the Win32 menu id the AC29 workaround posts. Never store it. |
| P8 | Cost? | Enumerating the table and writing every entry to a file took less than one timer tick (≤ 16 ms). |
| P9 | What are the invisible entries? | Placeholders of empty menus ("Keine Hilfe Dateien", "Add-On Befehle im Optionen-Menü.", "Keine Varianten-Kombinationen"), 118 in AC30. |
| P10 | Do context-menu groups have their own id ranges? | Yes, identical in AC29 and AC30, see 4.2. |
| P11 | Are dynamic list entries in the table? | No. Graphic override combinations appear only as their placeholder (23040, invisible). |

## 3. Decisions (2026-11-12)

- **D6. One way for both versions.** Archicad 29 and 30 read the command table. The old path
  is removed: scheme export, `.bin` reader, menu linking, Win32 menu filling and `WM_COMMAND`.
- **D7. What is offered (option "C+").** Every visible command of Archicad and of the loaded
  add-ons, except Archicad's own commands of four palette context menus (Navigator, Drawing
  Manager, Issue Management, Model Review palette). Reason: those act on what is selected in a
  palette the user is not looking at; a typed "Löschen" must not delete a view. The context menu
  of the drawing window stays in ("Ebene ausblenden" acts on the selection the user sees), as do
  status-bar and palette-shape commands.
- **D8. Fork first.** Built in the fork without regard to REVIEW.md; an upstream proposal is cut
  separately later.

**What stays from the base spec:** D1 (all commands, including ones that change the project),
D3 (disabled commands greyed out, not runnable), D4 (the built-in tools rank above commands),
anti-goals A1 to A3, section 6 (commands run only on an explicit Enter or click).

**What this replaces in the base spec:** G1 (now: every command the table offers under D7, not
only those in the menu bar), G3 (category instead of menu path, 4.3), G4 (no longer relevant),
D2 and D5 (no export, no menu matching), sections 4.1 to 4.3, 5 and 11. G2 and 4.4 (dynamic
lists) were never built and stay out (section 6).

## 4. How it works

### 4.1 Reading

- The table is read every time the index is refreshed on HUD open (`WM_BAAZ_REFRESH_INDEX`),
  from the HUD window's own message, never from the hook (R15).
- For every entry: the key (command id, developer id, product id), the name (P2, chosen by
  `ACAPI_VERSION`), the category, visible and enabled.
- Null descriptors and entries with an empty name are skipped.

### 4.2 Filter (D7)

An entry is offered when all of these hold:

1. it is visible;
2. its name is not empty;
3. it is not one of Archicad's own commands (developer id 0 and product id 0) in these ranges:

| Range (command id) | Group (German UI) | Exceptions, offered |
| ------------------ | ----------------- | ------------------- |
| 257 – 266 | Context menu of the Model Review palette | none |
| 13825 – 14000 | Navigator Kontext Menüs | 13983, 13984 (category "06 Dokumentation": "Zur aktuellen Ausgabe hinzufügen/entfernen"), 13988 (category "05 Planung": "Schnitt/Ansichtsbereich") |
| 19201 – 19224 | Zeichnungsmanager Kontext-Menüs | none |
| 30465 – 30471 | Issue-Management | none |

The filter uses ids only, never category names, so it works in every UI language. The ranges
and exceptions are what both measured versions have; a later version may add commands in or
next to a range (section 9).

### 4.3 What the user sees

- A command result shows its name on the left and its category on the right, with a leading
  two-digit number and the space after it removed ("04 Ausschnitt" → "Ausschnitt"). Categories
  without a number are shown as they are. *(My call.)* The category is what tells the six
  "Löschen" apart.
- *(My call:)* the right column, today a fixed 90 px × DPI scale in `HudWindow::Render`, grows up
  to 40 % of the HUD width as the base spec 4.5 intended; a longer category is cut at the end with
  an ellipsis.
- *(My call:)* rows with the same name and the same category stay separate rows; they are
  different commands (for example two "Energiefluss" in the drawing context menu), and hiding one
  could hide the one the user wants.
- **Disabled commands are greyed out (D3).** D3 was decided but never drawn: today `enabled` is
  stored and checked only at run time, and a disabled command looks like any other row. Drawing
  it is part of this change. *(My call:)* a grey row can be selected like any row; Enter or a
  click on it runs nothing and shows the refused line (4.4).
- Built-in tools rank above commands (D4); this keeps its current implementation.

### 4.4 Running

- On Enter or click, Baaz looks the key up in the table again. It runs the command only if the
  entry still exists, passes the filter and is enabled; then it calls `PostCommandMessage` with
  the key. *(My call:)* a command whose state changed since the HUD opened is refused like today.
- Only the key is kept in a result. `GetOSCommandId()` is never used (P7).
- A refused or failed run shows the existing refused line in the HUD, worded for the new cause
  (for example "The last command did not run: it is no longer available or is disabled").

### 4.5 Hints

- The export guide, the states "no export" and "N commands not searchable", and the
  *Work Environment* shortcut go away; nothing needs an export any more.
- *(My call:)* if reading the table throws or no entry passes the filter, no commands are offered
  and the HUD shows the existing "cannot be read" line, worded for this Archicad version.
- The refused line stays (4.4).
- *(My call:)* the hint row loses its only action, so it becomes plain text at the top of the HUD
  and can no longer be selected. The selection logic for it (`s_hintSelected` and the Up/Down,
  Enter and click cases) and the tests that cover selecting it or opening Work Environment from it
  go with it; the test that the hint is never the default selection and never runs on Space keeps
  its point and is adapted, not removed.

### 4.6 Language

Names come from Archicad in its UI language; the filter needs no names (4.2). The 43 built-in
tools and their chords stay as they are.

## 5. What is removed

- Sources: `SchemeReader`, `BinSchemeReader`, `MenuLink`, `CommandSource` (with `LiveMenu`), and
  in `CommandHint` everything about the export.
- Tests and fixtures that only cover the removed code: `test_command_line_native` (with its
  trimmed export fixture), `test_bin_scheme`, `test_live_menu`, the parts of `test_command_hint`
  about export states.
- `SearchItem` fields used only by the old path (`menuPosition`, `liveId`).
- Rows of the `CLAUDE.md` layout table that describe removed files, and every text that tells
  the user to export a scheme.
- *(My call:)* Baaz stops reading `%USERPROFILE%\.baaz\commands\` but does not delete anything
  there; it never wrote that folder.

## 6. Goals, non-goals, anti-goals

- **Goals:** every command offered under D7 can be found by name and run, in Archicad 29 and 30,
  with no setup step; the result is the same in every UI language.
- **Non-goals:** dynamic list entries (graphic override combinations, model view options, pen
  sets; P11); options after a command (Rhino style); the four palette context menus of D7;
  macOS.
- **Anti-goals:** running a different command than the one picked (A1); work in the hook path
  (A2); changing the user's work environment (A3); offering a command that acts on a palette
  selection the user does not see (D7).

## 7. Constraints

- The command table is part of the DevKit's DG library, not of the documented `ACAPI_` API. It
  already changed between AC29 and AC30 (P2). Every DG call sits behind the existing
  `ACAPI_VERSION` switch, so a rename breaks the build, not Archicad.
- R5 (Archicad calls only in `BaazAddon.cpp`, `CommandExecutor.cpp`, `NavigatorIndexer.cpp`): the
  fork is not bound by it (D8), but keeping it costs little and keeps the upstream path open. So
  the DG calls go into those files, and the filter and display rules into a file without Archicad
  calls, so they can be tested natively.
- No new dependency: the DG library is part of the DevKit and already linked.
- Warning-free at `/W4`; no new lizard exceptions.

## 8. Relation to upstream (for the later proposal)

- R14 (Baaz only navigates) still conflicts with D1, as before.
- R17 no longer applies: nothing is posted to a window; Archicad's own `PostCommandMessage` runs
  the command.
- The undocumented `.bin` format and the manual export are gone, which removes the main objection
  noted in the addendum.
- New point to name: the DG table is DevKit API without `ACAPI_` documentation (section 7).

## 9. Risks and unknowns

- The ranges of 4.2 are inferred from two versions. A future version may add commands to a range
  or move a context menu. Checking after each Archicad update: compare a fresh dump with this
  table.
- Not measured: an English or other non-German Archicad (ids should be the same, names and
  categories differ); commands that open a dialog; add-on commands run through
  `PostCommandMessage`; Teamwork-only commands in a solo project; whether the table changes while
  Archicad runs (add-ons loaded later).
- `PostCommandMessage` queues the command. If its state changes between the check of 4.4 and the
  moment Archicad handles the message, Archicad decides; Baaz cannot see that.

## 10. Left to the builder

- The exact split between the API files and the testable file (R5), and the data type passed
  between them.
- Hint wording (English, like the other HUD texts).
- Ranking between commands with the same name.
- Order of removal and new code, split into plan tasks and stages.

## 11. Checks

- **Automated (native, no Archicad):** the filter (invisible, empty name, each range, each
  exception, range bounds, an add-on command with an id inside a range is offered); category
  trimming ("04 Ausschnitt", "Navigator Kontext Menüs", empty); the refused and unreadable hint
  lines; D4 ranking still holds with commands from the new source; Enter on a disabled command
  runs nothing and shows the refused line; the hint row is never selected (HUD key handling in
  `test_message_hook`).
- **Build, both versions:** CI builds against one DevKit only, so the add-on is also built locally
  against the AC29 and the AC30 DevKit; both getter branches of 4.1 must compile warning-free.
- **Manual, Archicad 29 and 30:** "Lineal" toggles the ruler; a command that opens a dialog;
  an add-on command; a disabled command is grey and does not run; "Löschen" offers no Navigator
  or Drawing Manager entry; "Ebene ausblenden" works on a selected element; the HUD opens as fast
  as before; nothing in `.baaz\commands\` is needed.
