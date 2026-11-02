# Rules for every change

Each change, whoever or whatever wrote it, is reviewed against these rules. Cite them by
number in reviews ("breaks R4"). Rules marked **(checked)** are enforced by CI; the rest are
for the author and the reviewer.

## Scope

- **R1 One purpose per pull request.** No drive-by refactors, renames or reformatting of
  code the change does not need. A large feature is split into reviewable steps.
- **R2 Build only what was asked for.** No speculative options, settings, extension points
  or "while I was here" features.

## Code

- **R3 Reuse before writing.** Search first. A helper needed in a second file moves to a
  header shared by both, never copied.
- **R4 File-local helpers are `static`.** Only what another file needs goes in a header.
  No new globals; state lives in the class that owns it.
- **R5 Only `BaazAddon.cpp`, `CommandExecutor.cpp` and `NavigatorIndexer.cpp` call the
  Archicad API (checked).** Their non-API code paths (`#ifdef ACAPI_VERSION`) keep the
  rest of the add-on buildable and testable without Archicad.
- **R6 Resources come from `.grc` files (checked).** The `.rc2`/`.res` files are generated
  during the build and never committed. Change `RINT/` or `RFIX/`, not generated output.
- **R7 Keep functions small and plain (checked).** Cyclomatic complexity above 12 fails CI
  (switch statements count once). The two existing exceptions are listed in
  `whitelizard.txt`; do not add new ones; split the function instead.
- **R8 No dead code (checked in part).** No commented-out code, unreachable branches,
  unused parameters or includes, debug output (`printf`, `OutputDebugString`, `std::cout`),
  or TODO without a linked issue. Compiler warnings at `/W4` fail CI.
- **R9 Handle errors on purpose.** Check every `GSErrCode` and Win32 result you depend on.
  Fall back visibly (a closed HUD, an unchanged setting), never by crashing Archicad or
  leaving a hook or timer in a half-set state.
- **R10 Comments explain why, not what.** No comments that narrate the change ("added",
  "new", "now"), restate the code, or address the reviewer. Match the comment density of the
  surrounding code.
- **R11 No new dependency without the maintainer's agreement.** The add-on uses only the
  Archicad DevKit, Win32, GDI+ and the C++ standard library.
- **R12 Names use Archicad terms** (tool, view, layout, layer combination, story). No
  `helper`, `utils`, `manager`, `data2`, `new_`, `tmp`.

## Safety and privacy

Baaz runs inside Archicad on files that are often under client confidentiality, and its
hooks see every keystroke and click in Archicad.

- **R13 Nothing leaves the user's machine (checked).** No network access, no telemetry, no
  launching programs, no logging of keystrokes or project data (view names, layer names,
  search text).
- **R14 Baaz only navigates (checked).** It switches tools, views, layouts and layer
  combinations. Anything that creates, changes or deletes elements, attributes, Navigator
  items or files of the project needs the maintainer's agreement first. A borrowed MDID
  (`AddOnFix.local.grc`) and `tools/check_mdid.py` are never committed, and a build made with
  `BAAZ_DEV_MDID` is never distributed (checked).
- **R15 Hooks stay transparent and cheap.** Every message the hook does not handle goes to
  `CallNextHookEx` unchanged. No file I/O, no blocking and no `ACAPI_` calls in the hook
  path beyond `CanSummonHud`; such work runs from the HUD window's own messages.
- **R16 Files are written only to `%USERPROFILE%\.baaz\`**, through `Settings`.
- **R17 Synthesized input only for Magic Wand (checked).** `SendInput` is used only to replay
  Space marked with `SENTINEL_MAGIC_WAND`, and to type chords in the non-API test build.
  Nothing may be typed into other applications.

## Tests

- **R18 Every behaviour change has a test that fails without it.** Spacebar, hook and HUD
  key handling go in `tests/test_message_hook.cpp`, run in both delivery modes; search
  ranking goes in `tests/test_search_engine_native.cpp`.
- **R19 Never weaken a test to make a change pass.** No deleting, skipping, loosening
  assertions or raising time limits without saying why in the pull request.

## Documentation and history

- **R20 Docs move with the code.** A new or changed key, chord, setting or build option
  updates the README in the same pull request.
- **R21 Commit messages say why**, not only what, and pull request descriptions state what
  was verified and what was not (behaviour inside Archicad is never verified by CI).

## Third-party code

- **R22 `tools/CompileResources.py` is Tapir's.** Change it only to fix a bug, keep its
  attribution header, and keep `tools/LICENSE-tapir.txt`.
