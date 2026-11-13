---
stages:
  - id: 3
    label: "Command catalogue: filter, labels, run check (native tests)"
  - id: 4
    label: "Switch HUD and executor to Archicad's command table"
  - id: 5
    label: "Remove the scheme/menu workaround"
  - id: 6
    label: "HUD display: grey disabled commands, wider right column"
  - id: 7
    label: "Both DevKits built, manual check in Archicad 29 and 30"
---

# Command line from Archicad's command table — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The command line offers and runs Archicad's commands from `DG::CommandDescriptor::GetCommandTable()` in Archicad 29 and 30, and the export/`.bin`/Win32-menu workaround is removed.

**Architecture:** A new file without Archicad calls, `CommandCatalog`, holds the filter (D7), the category label and the run check, and is tested natively. `NavigatorIndexer` reads the DG table into plain `CommandEntry` rows; `CommandExecutor` re-reads it on Enter and calls `PostCommandMessage`. The HUD keeps its refresh-on-open flow; its hint row becomes plain text.

**Tech Stack:** C++20, Win32, GDI+, Archicad DevKit 29 and 30 (DGLib), CMake + Ninja Multi-Config, native assert-based tests.

**Spec:** `docs/superpowers/specs/2026-11-13-command-line-dg-command-table-design.md`

## Global Constraints

- All code, comments, test names, commit messages and HUD texts in English.
- Archicad calls (`ACAPI_` and DG) only in `BaazAddon.cpp`, `CommandExecutor.cpp`, `NavigatorIndexer.cpp` (R5), each behind `#ifdef ACAPI_VERSION`; the non-API branch keeps the tests building.
- AC29 name getter `GetCleanText()`, AC30 `GetDisplayText()`, chosen by `#if ACAPI_VERSION >= 30`.
- Never store or use `GetOSCommandId()`; a command is identified only by (command id, developer id, product id).
- Warning-free at `/W4` with `-DBAAZ_WARNINGS_AS_ERRORS=ON`; cyclomatic complexity ≤ 12, no new entries in `whitelizard.txt` (split functions instead).
- No new dependency (DGLib is already linked through `DEVKIT_MODULE_LIBS`).
- No local paths, user names or MDID values in committed files. `docs/` is git-ignored: add plan/spec files with `git add -f`.
- Before every commit run, from the worktree root: `python tools/check_rules.py`, `python -m lizard addon/src tests -C 12 -m -w -W whitelizard.txt`, the build and `ctest --test-dir build -C Release --output-on-failure`.
- Commit identity must be the noreply address; check `git log origin/main..HEAD --format='%ae %ce'` before any push.

## Review Focus

1. A command that is disabled or gone by the time Enter is pressed must not run and must show the refused line — pinned by `TestRunnableNeedsSameKeyOfferedAndEnabled` (Task 1).
2. A command with the same title as a built-in tool ("Wand") must rank below the tool (D4) — pinned by `TestToolRanksAboveCommandWithSameName` (Task 1).
3. An add-on command whose id lies inside a filtered range must still be offered — pinned in `TestFilterRanges` (Task 1).
4. Up from the first result must stay on the results now that the hint has no action — pinned by `UpNeverSelectsHint` (Task 2).
5. A long category must not overlap the title — no automated test (GDI+ drawing); manual check in Task 5, item 7.

---

### Task 1: Command catalogue (Stage 3)

**Files:**
- Create: `addon/include/CommandCatalog.hpp`, `addon/src/CommandCatalog.cpp`
- Create: `tests/test_command_catalog.cpp`
- Modify: `addon/include/SearchEngine.hpp` (fields of `SearchItem`)
- Modify: `CMakeLists.txt` (`BAAZ_SHARED_SOURCES`), `tests/CMakeLists.txt` (new test target)
- Modify: `tests/test_search_engine_native.cpp` (one D4 test)

**Interfaces:**
- Produces (in `CommandCatalog.hpp`, namespace `Baaz`):

```cpp
// One row of Archicad's command table, copied out so the rules below need no Archicad API
struct CommandEntry {
    unsigned long commandId = 0;
    unsigned long developerId = 0;
    unsigned long productId = 0;
    std::wstring name;       // display text, without '&'
    std::wstring category;   // Work Environment group, e.g. "04 Ausschnitt"
    bool visible = true;
    bool enabled = true;
};

class CommandCatalog {
public:
    static bool IsOffered(const CommandEntry& entry);
    static std::wstring CategoryLabel(const std::wstring& category);
    static std::vector<SearchItem> MakeItems(const std::vector<CommandEntry>& table);
    static bool IsRunnable(const std::vector<CommandEntry>& table, const SearchItem& item);
};
```

- Produces (in `SearchEngine.hpp`, `SearchItem`): add
  `unsigned long commandId = 0; unsigned long developerId = 0; unsigned long productId = 0;`
  (comment: "Command: key in Archicad's command table"); keep `enabled` (comment: "Command: false
  when Archicad reports it disabled"). `menuPosition` and `liveId` stay until Task 3 removes them,
  because `CommandSource.cpp`, `MenuLink.cpp` and the old tests still use them.

- [ ] **Step 1: Write the failing tests** in `tests/test_command_catalog.cpp` (same style as `test_command_hint.cpp`: `#undef NDEBUG`, `assert`, one `[PASS]` line per test, `main` calls all):

  - `TestOfferedPlainCommand`: `{1557,0,0,L"Lineal",L"04 Ausschnitt",true,true}` → `IsOffered` true.
  - `TestInvisibleAndUnnamedAreDropped`: same entry with `visible=false` → false; with `name=L""` → false.
  - `TestFilterRanges`: for developer 0 / product 0: ids 256, 267, 13824, 14001, 19200, 19225, 30464, 30472 → true; 257, 266, 13825, 13900, 14000, 19201, 19224, 30465, 30471 → false; exceptions 13983, 13984, 13988 → true. Add-on rows inside a range are offered: `{13880,1,2}`, `{13880,0,5}`, `{19210,7,0}` → true.
  - `TestCategoryLabel`: `"04 Ausschnitt"`→`"Ausschnitt"`, `"12 Werkzeuge"`→`"Werkzeuge"`, `"Navigator Kontext Menüs"` unchanged, `""`→`""`, `"4 Foo"` unchanged, `"04"` unchanged, `"04Ausschnitt"` unchanged.
  - `TestMakeItemsMapsAndFilters`: table of three rows (offered enabled, offered disabled, invisible) → two items; each has `kind == ItemKind::Command`, `title == name`, `tag == CategoryLabel(category)`, the three ids copied, `enabled` copied.
  - `TestRunnableNeedsSameKeyOfferedAndEnabled`: item made from `{1557,0,0,…,enabled}`: runnable against a table holding that row; not runnable when the row is disabled, when the table lacks it, when only `{1557,1,0}` or `{1557,0,1}` is present, and for an item with id 13880 (filtered) even if the table holds it enabled.

  In `tests/test_search_engine_native.cpp` add `TestToolRanksAboveCommandWithSameName`: `SetProjectItems` with one item `{title L"Wand", tag L"Werkzeuge", kind Command}`; `Query(L"wand")` → first result is the tool (`kind == ItemKind::Tool`), the command is also in the results.

- [ ] **Step 2: Register the test and see it fail**

  In `tests/CMakeLists.txt` add target `test_command_catalog` from `test_command_catalog.cpp` and `../addon/src/CommandCatalog.cpp`, include dir `../addon/include`, `UNICODE _UNICODE`, `add_test(NAME CommandCatalog …)`. Add `CommandCatalog.cpp` to `BAAZ_SHARED_SOURCES`.
  Run: configure + build. Expected: build fails, `CommandCatalog.hpp` not found.

- [ ] **Step 3: Implement `CommandCatalog` in `addon/src/CommandCatalog.cpp`**

  The ranges as one `static const` table of `{first, last}` pairs (257–266, 13825–14000, 19201–19224, 30465–30471) and the exceptions as a second (`13983, 13984, 13988`), applied only when developer and product id are both 0. `CategoryLabel` strips exactly "two ASCII digits followed by one space" at the start. `IsRunnable` finds the row with all three ids equal and requires `IsOffered && enabled`. Add the new `SearchItem` fields (see Interfaces).

- [ ] **Step 4: Run the tests**

  Run: `ctest --test-dir build -C Release --output-on-failure -R "CommandCatalog|SearchEngineNative"`
  Expected: both PASS. `TestToolRanksAboveCommandWithSameName` should pass without code change (D4 is in `SearchEngine` today); if it fails, fix the ranking, not the test.

- [ ] **Step 5: Full checks, commit**

  Run the Global Constraints checks. Commit `feat(commands): catalogue rules for Archicad's command table — Stage 3`.

---

### Task 2: Switch to the command table (Stage 4)

**Files:**
- Modify: `addon/include/NavigatorIndexer.hpp`, `addon/src/NavigatorIndexer.cpp`
- Modify: `addon/src/CommandExecutor.cpp`
- Modify: `addon/include/CommandHint.hpp`, `addon/src/CommandHint.cpp`
- Modify: `addon/include/HudWindow.hpp`, `addon/src/HudWindow.cpp`
- Test: `tests/test_command_hint.cpp`, `tests/test_message_hook.cpp`

**Interfaces:**
- Consumes: `CommandEntry`, `CommandCatalog::MakeItems`, `CommandCatalog::IsRunnable` (Task 1).
- Produces:
  - `static std::vector<CommandEntry> NavigatorIndexer::ReadCommandTable();` — API build: every non-null descriptor of `DG::CommandDescriptor::GetCommandTable()` as a `CommandEntry` (name getter per Global Constraints, `GetCategory()`, `IsVisible()`, `IsEnabled()`); may throw, callers catch. Non-API build: returns `{}`.
  - `HintRow` becomes `{ std::wstring title; std::vector<std::wstring> guide; }` (no `canOpenWorkEnvironment`); `HintRow MakeHintRow(bool commandsReadable, bool lastRunRefused);`. `CommandHint.hpp` no longer includes `CommandSource.hpp`.
  - `static void HudWindow::ApplyCommandsReadable(bool readable);` replaces `ApplyCommandIndex`; `IsHintSelected`, `MoveUp`'s hint branch, `LeaveHint`, `s_hintSelected`, `s_commandState`, `s_notSearchable`, `s_workEnvironment` go.

- [ ] **Step 1: Write the failing tests**

  `tests/test_command_hint.cpp` — replace the export-state tests with:
  - `TestNoHintWhenReadable`: `MakeHintRow(true, false)` → empty title, empty guide.
  - `TestUnreadableHint`: `MakeHintRow(false, false).title == L"Commands cannot be read in this Archicad version."`, guide empty.
  - `TestRefusedRunIsShownOnce`: `MakeHintRow(true, true).title == kRefused`, guide empty; `MakeHintRow(false, true)` → unreadable title and `guide == { kRefused }`; `kRefused = L"The last command did not run: it is no longer available or is disabled."`

  `tests/test_message_hook.cpp`, hint section:
  - Remove `WorkEnvironmentMenu`, `NoExportIndex`, `kWorkEnvironmentId`, `EnterOnHintOpensWorkEnvironment`, `ClickOnHintOpensWorkEnvironment`, `SpaceNeverRunsHint`, `EnterOnHintWithoutActionDoesNothing`, `UpReachesHintAndDownLeavesIt`, and their `Run(...)` lines; remove `g_commands` if nothing uses it any more.
  - `HintIsNeverTheDefaultSelection`: keep, drop the `IsHintSelected` checks (hint title not empty; `sl` + Enter runs chord `SL`).
  - Add `UpNeverSelectsHint`: open HUD, type `sl`, Up, Up, Enter → HUD hidden, `ExecutedChord() == L"SL"`.
  - Add `ClickOnHintRunsNothing`: open HUD, `ClickHintRow()`, pump → HUD still visible, `ExecutedChord()` empty.
  - `ClickOnHiddenHudRunsNothing`: keep without `WorkEnvironmentMenu`/`ApplyCommandIndex`; check HUD hidden and `ExecutedChord()` empty.
  - `RefusedRunIsShownOnce`: unchanged (non-API `Execute` of a Command returns false).
  - Register the two new tests in both delivery modes, next to the others.

- [ ] **Step 2: Run them and see them fail**

  Run: build. Expected: compile errors in `test_command_hint.cpp` (`MakeHintRow` signature) — then, after Step 3's hint change only, `UpNeverSelectsHint` fails (Up selects the hint, Enter does nothing).

- [ ] **Step 3: Implement**

  - `CommandHint.cpp`: new signature and the two texts above; the refused line is the title when there is no other line, else one guide line.
  - `NavigatorIndexer.cpp`: `ReadCommandTable` (include `DGCommandDescriptor.hpp`); enumerate with `table.Enumerate([&](const DG::Command& cmd, DG::CommandDescriptor* const& desc) {…})`; convert names with the existing `ToWString`.
  - `CommandExecutor.cpp`: `case ItemKind::Command: s_refusedRun = !RunCommand(item);`. API `static bool RunCommand(const SearchItem& item)`: inside `try`, `CommandCatalog::IsRunnable(NavigatorIndexer::ReadCommandTable(), item)`, then `DG::Application::GetInstance()` non-null and `PostCommandMessage(DG::Command(item.commandId, item.developerId, item.productId))`; `catch (...)` → false. Non-API: returns false. Drop the `CommandSource` include and `MainWindow()` argument.
  - `HudWindow.cpp` (drop the `CommandSource.hpp` include, add `CommandCatalog.hpp`): `WM_BAAZ_REFRESH_INDEX` builds items as today plus `CommandCatalog::MakeItems(NavigatorIndexer::ReadCommandTable())` inside the existing `try`; readable = no exception and at least one command item; then `ApplyCommandsReadable(readable)`. Initial readable state is `true` (a fresh HUD must not flash a hint, as today). `ExecuteSelection`, `ClickAt`, `MoveUp`, `HandleKey` (Down/Tab/Space) lose every hint-selection branch; `DrawHint` loses `selected`.

- [ ] **Step 4: Run all tests**

  Run: `ctest --test-dir build -C Release --output-on-failure`
  Expected: all PASS. The old `CommandLineNative`, `BinScheme`, `LiveMenu` tests still build and pass (their sources are untouched until Task 3).

- [ ] **Step 5: Build the add-on against the AC30 DevKit, full checks, commit**

  Configure with `-DARCHICAD_DEVKIT_DIR=<AC30 DevKit> -DBAAZ_WARNINGS_AS_ERRORS=ON`, build Release, run the Global Constraints checks. Commit `feat(commands): read and run Archicad's command table in the HUD — Stage 4`.

---

### Task 3: Remove the workaround (Stage 5)

**Files:**
- Delete: `addon/include/SchemeReader.hpp`, `addon/src/SchemeReader.cpp`, `addon/include/BinSchemeReader.hpp`, `addon/src/BinSchemeReader.cpp`, `addon/include/MenuLink.hpp`, `addon/src/MenuLink.cpp`, `addon/include/CommandSource.hpp`, `addon/src/CommandSource.cpp`
- Delete: `tests/test_command_line_native.cpp`, `tests/test_bin_scheme.cpp`, `tests/bin_scheme_builder.hpp`, `tests/test_live_menu.cpp`, `tests/fixtures/command_scheme_ac29_de.xml` (the folder's only file); drop the `BAAZ_TEST_FIXTURES` definition with the target
- Modify: `CMakeLists.txt` (`BAAZ_SHARED_SOURCES`), `tests/CMakeLists.txt` (drop the three targets), `addon/include/SearchEngine.hpp` (drop `menuPosition`, `liveId`), `CLAUDE.md` (layout table)

**Interfaces:** none new. After this task nothing references `CommandState`, `CommandIndex`, `LiveMenu`, `MenuNode`, `.baaz\commands`.

- [ ] **Step 1: Delete files and their build entries; drop the two `SearchItem` fields**

- [ ] **Step 2: Prove nothing is left**

  Run: `git grep -n -E "SchemeReader|BinScheme|MenuLink|CommandSource|LiveMenu|CommandState|CommandIndex|menuPosition|liveId|baaz.?.?commands" -- addon tests CMakeLists.txt CLAUDE.md`
  Expected: no output.

- [ ] **Step 3: Update `CLAUDE.md`**

  Layout table: remove the rows for `SchemeReader.cpp`, `BinSchemeReader.cpp`, `MenuLink.cpp`, `CommandSource.cpp`; add `addon/src/CommandCatalog.cpp` — "Which commands of Archicad's command table are offered and may run, and their labels; no Archicad API."; extend `NavigatorIndexer.cpp` ("… and Archicad's command table") and `CommandExecutor.cpp` ("… settings, Archicad commands").

- [ ] **Step 4: Full checks against the AC30 DevKit, commit**

  Expected: build and all remaining tests green, `check_rules.py` and lizard clean. Commit `refactor(commands): remove scheme export, .bin reader and menu linking — Stage 5`.

---

### Task 4: HUD display (Stage 6)

**Files:**
- Modify: `addon/src/HudWindow.cpp` (`Render`)

UI-only (no logic the native tests can observe); verified manually in Task 5.

- [ ] **Step 1: Grey disabled commands**

  In the result loop: title brush is `mutedBrush` when `!item.enabled` and the row is not selected, `selTagBrush` when it is disabled and selected; otherwise as today.

- [ ] **Step 2: Right column up to 40 %**

  Per row: measure the tag with `g.MeasureString` in `fonts.hint`; `tagW = clamp(measured + padX, 90 × scale, 0.4 × width)`; the title gets `width − padX − tagW`. `formatRight` gets `StringTrimmingEllipsisCharacter`. Move the drawing of one row into a `static void DrawResultRow(...)` so `Render` stays ≤ 12 complexity.

- [ ] **Step 3: Full checks against the AC30 DevKit, commit**

  Commit `feat(hud): grey disabled commands, right column fits the category — Stage 6`.

---

### Task 5: Both versions, manual check (Stage 7)

- [ ] **Step 1: Build against both DevKits**

  Configure two build folders: `-DBAAZ_AC_VERSION=30 -DARCHICAD_DEVKIT_DIR=<AC30 DevKit>` and `-DBAAZ_AC_VERSION=29 -DARCHICAD_DEVKIT_DIR=<AC29 DevKit>`, both with `-DBAAZ_WARNINGS_AS_ERRORS=ON`. Expected: both build warning-free, `ctest` green in both. Check the registered MDID in each `Baaz.apx` before deploying (byte scan, values not written down here).

- [ ] **Step 2: Deploy and check in Archicad 30, then 29** (Archicad of that version closed while copying; hash of the copy equals the build)

  1. HUD opens with no hint line.
  2. `lineal` + Enter toggles the ruler.
  3. A command that opens a dialog opens it.
  4. An add-on command runs.
  5. A disabled command is grey; Enter on it runs nothing and the next HUD shows the refused line once.
  6. `löschen` lists no Navigator or Drawing Manager entry; *Bearbeiten › Löschen* is there with category "Bearbeiten".
  7. A long category ("Speichern in Wiederherstellungsdatei Statusanzeige-Paneel") ends in "…" and does not overlap the title.
  8. "Ebene ausblenden" with an element selected hides its layer.
  9. `wand` lists the Wall tool first.
  10. The HUD opens as fast as before (no visible delay).

- [ ] **Step 3: Report results to the maintainer**, item by item, with anything not checked named as such. No stage change without the maintainer's OK.

---

Delivery: one branch, one pull request into the fork's `main` after Task 5 and the review pipeline (`feature/dg-command-table`).
