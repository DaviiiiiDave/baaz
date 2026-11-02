---
stages:
  - id: 3
    label: "Scheme reader: exported XML to menu tree"
  - id: 4
    label: "Menu linking, export choice, staleness"
  - id: 5
    label: "Live Win32 menu: fill, read, run"
  - id: 6
    label: "Search and display: command results"
  - id: 7
    label: "Dynamic lists: graphic overrides, model view options"
---

# Baaz Command Line Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every command of the user's Archicad menu bar can be found by its localised name in the Baaz HUD and run from there.

**Architecture:** A hand-written reader turns the exported command-layout scheme (XML) into a menu tree. A linker walks that tree beside the live Win32 menu bar (behind a `MenuSource` interface) and links names to live command ids by position, anchoring around known placeholder items. Results become `SearchItem`s of a new kind; running one re-checks the live position and posts `WM_COMMAND` to Archicad's frame window.

**Tech Stack:** C++20, Win32, GDI+, Archicad DevKit (AC29 / AC30), CMake + Ninja Multi-Config, native assert-style tests.

**Spec:** `docs/superpowers/specs/2026-11-02-command-line-design.md` (read it before any task; section numbers below refer to it).

## Global Constraints

- Dependencies: Archicad DevKit, Win32, GDI+, C++ standard library only (REVIEW R11). No XML library: the reader is hand-written.
- `ACAPI_` calls only in `BaazAddon.cpp`, `CommandExecutor.cpp`, `NavigatorIndexer.cpp` (R5). Win32 menu calls are allowed elsewhere.
- No file I/O, no menu filling and no `ACAPI_` call in the hook path; all of it runs from `HudWindow`'s own messages (R15).
- Baaz reads `%USERPROFILE%\.baaz\commands\*.xml` and the first 512 bytes of `*.bin` files under `%APPDATA%\GRAPHISOFT\`. It writes nothing new anywhere (A3, R16).
- A command whose live position is in doubt is never offered and never run (A1).
- Floor-plan ("Nicht-Text") menu bar: `menubarguid="2085E4D3-5694-4BB9-ACAC-3AED5BE50091"`. The "Text" bar (`2CAC215F-221C-420F-AAEB-8462A659B8D9`) is ignored.
- Dynamic-list placeholders: graphic override combinations `23040`, model view options `9472`.
- Command-layout scheme marker in `.bin` files: ASCII `6FCD168F-A26C-4547-AF9D-ED9A5FC54DD7`.
- Menu path separator: `" › "` (` › `).
- Function cyclomatic complexity ≤ 12 (R7, `lizard`); no new `whitelizard.txt` entries.
- All code, comments, commit messages, README text in English. Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Before every commit run the five checks in `CLAUDE.md` ("Commands"). Docs under `docs/` are gitignored: add them with `git add -f`.
- This branch is based on `main` (Archicad 30). Manual tests in Archicad 29 run from a local integration branch that also merges `feature/ac-version`, `fix/space-in-canvas-text` and `fix/space-click-wand` (as `local/ac29-test` does); that branch is never pushed.

## Review Focus

1. **Export from another Archicad language or version than the one running** → top-level titles differ → no commands are offered, no crash. Test: Task 2 `TestFitScoreRejectsOtherLanguage`.
2. **Menus changed after the export, same item count** → export counts as stale, no commands, one status line. Tests: Task 2 `TestStaleExport`, Task 3 `TestStatusLineWhenStale`.
3. **Exported file with UTF-8 BOM, CRLF line ends and XML comments** → read exactly like a plain file. Test: Task 1 `TestBomCommentsAndCrlf`.
4. **Archicad rebuilt its menus between indexing and Enter** (live id at the position changed) → nothing runs, HUD stays usable. Test: Task 3 `TestRunRefusesChangedId`.
5. **Names with `&&`, `&amp;` or access-key `&`** → shown as Archicad shows them ("Raster- & Bearbeitungsebenen-Optionen", "Elementfang"). Test: Task 1 `TestNamesUnescaped`.

---

### Task 1: Scheme reader (stage 3)

**Files:**
- Create: `addon/include/SchemeReader.hpp`, `addon/src/SchemeReader.cpp`
- Create: `tests/fixtures/command_scheme_ac29_de.xml`
- Create: `tests/test_command_line_native.cpp`
- Modify: `tests/CMakeLists.txt` (new target `test_command_line_native`, test name `CommandLineNative`, compile definition `BAAZ_TEST_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"`)
- Modify: `CMakeLists.txt:19-27` (`BAAZ_SHARED_SOURCES` gets `SchemeReader.cpp`)

**Interfaces:**
- Produces:
  ```cpp
  enum class MenuNodeType { Item, Separator, Popup };
  struct MenuNode {
      MenuNodeType type = MenuNodeType::Item;
      std::wstring name;            // comment or title; access keys removed
      unsigned commandNumber = 0;   // first part of command="<n>-<mdid1>-<mdid2>"
      std::vector<MenuNode> children;  // Popup only
  };
  class SchemeReader {
  public:
      // Top-level menus of the "Nicht-Text" menu bar; empty if missing or malformed.
      static std::vector<MenuNode> ReadFloorPlanMenuBar(const std::string& utf8Xml);
  };
  ```

- [ ] **Step 1: Build the fixture.** Start from the user's exported command scheme (`Befehle_*.xml`, ask Dave for the current export). Keep: the XML header and `<Scheme>` root; the "Nicht-Text" `<MenuBar>` with only `Ansicht` and `Dokumentation`; inside `Dokumentation` keep `Grafische Überschreibung` and `Modelldarstellung` popups and the `Grundriss-Schnittebene...` item; every `<Menu ID>` those reference by `IDREF`; the "Text" `<MenuBar>` with one menu titled `Text-Only`. Remove everything else, in particular the Help menu (contains the user's Graphisoft ID and name) and all third-party add-on menus. Save as UTF-8 with BOM and CRLF, and add one `<!-- comment -->` inside `CommandLayout`.

- [ ] **Step 2: Write the failing tests** in `tests/test_command_line_native.cpp` (style of `test_search_engine_native.cpp`: `assert`, `std::cout << "[PASS] ..."`, called from `main`):
  ```cpp
  void TestReadsFloorPlanBarOnly();   // names == {L"Ansicht", L"Dokumentation"}; no L"Text-Only"
  void TestRulerPosition();           // Ansicht.children[9]: Item, name L"Lineal", commandNumber 1557
  void TestIdrefResolved();           // Ansicht.children[0]: Popup L"Bildschirmdarstellungs-Optionen", !children.empty()
  void TestPlaceholderNumbers();      // Dokumentation > "Grafische Überschreibung" children[2].commandNumber == 23040
                                      // Dokumentation > "Modelldarstellung" children[2].commandNumber == 9472
  void TestNamesUnescaped();          // Ansicht.children[6].name == L"Raster- & Bearbeitungsebenen-Optionen"
                                      // Ansicht.children[13].name == L"Elementfang"
  void TestBomCommentsAndCrlf();      // fixture (BOM, CRLF, comment) reads like the same text without them
  void TestMalformedIsEmpty();        // "", "<Scheme>", unclosed tag, missing Nicht-Text bar -> empty
  ```
  Positions follow the spec's evidence for Dave's scheme (spec section 3, E8). If the fixture's Ansicht differs, take positions from the fixture and say so in the commit message.

- [ ] **Step 3: Run to see them fail.** `cmake --build build --config Release` then `ctest --test-dir build -C Release -R CommandLineNative --output-on-failure`. Expected: link error or failing asserts, because `SchemeReader` does not exist.

- [ ] **Step 4: Implement `SchemeReader::ReadFloorPlanMenuBar`.** A small tokenizer for start tags, end tags, self-closing tags, attributes in double quotes, comments, the `<?xml ?>` declaration, and the entities `&amp; &lt; &gt; &quot; &apos; &#n; &#xn;`. Convert UTF-8 to UTF-16 with `MultiByteToWideChar(CP_UTF8, …)`; skip a leading BOM. First collect every `<Menu ID>` definition (top-level and inline) in a map, then build the "Nicht-Text" bar, resolving `IDREF` through the map (guard against reference cycles by depth limit 16). Name cleanup: `&&` → `&`, a single `&` removed. Keep every function under complexity 12.

- [ ] **Step 5: Run the tests.** Same command. Expected: all `[PASS]`, then the full five checks from `CLAUDE.md` green.

- [ ] **Step 6: Commit.** `feat(command-line): read the exported command scheme into a menu tree`.

### Task 2: Menu linking (stage 4)

**Files:**
- Create: `addon/include/MenuLink.hpp`, `addon/src/MenuLink.cpp`
- Modify: `tests/test_command_line_native.cpp`, `tests/CMakeLists.txt`, `CMakeLists.txt` (`BAAZ_SHARED_SOURCES`)

**Interfaces:**
- Consumes: `MenuNode`, `MenuNodeType` (Task 1).
- Produces:
  ```cpp
  struct LiveEntry { MenuNodeType type = MenuNodeType::Item; unsigned liveId = 0; bool enabled = true; };
  class MenuSource {
  public:
      virtual ~MenuSource() = default;
      virtual std::vector<std::wstring> TopLevelTitles() = 0;            // access keys removed
      // path[0] = top-level index, then popup indices; fills the menu before reading it
      virtual std::vector<LiveEntry> Entries(const std::vector<int>& path) = 0;
  };
  struct LinkedCommand {
      std::wstring name;
      std::wstring menuPath;        // "Dokumentation › Grafische Überschreibung"
      std::vector<int> position;    // full path including the item's own index
      unsigned liveId = 0;
      bool enabled = true;
  };
  struct DynamicList { unsigned placeholder = 0; std::vector<std::wstring> names; };
  class MenuLink {
  public:
      static std::vector<LinkedCommand> Link(const std::vector<MenuNode>& bar, MenuSource& live,
                                             const std::vector<DynamicList>& lists);
      // -1 if top-level titles differ; otherwise the number of linked commands
      static int FitScore(const std::vector<MenuNode>& bar, MenuSource& live);
      // true when any scheme file is newer than the export
      static bool IsExportStale(const FILETIME& exportTime, const std::vector<FILETIME>& schemeTimes);
  };
  ```

- [ ] **Step 1: Placeholder table.** In `MenuLink.cpp`, a `static const unsigned kPlaceholderCommands[]` with every command number from the AC29 standard scheme whose item Archicad replaces at run time. Derive it once from the standard export ("Standardbefehle 29.xml", ask Dave): items whose German name starts with `Keine `, `Kein `, `Letzte`, `Add-On` or `Add-On-`, plus `23040` and `9472`. The numbers are language-independent; list the derivation rule in a comment.

- [ ] **Step 2: Write the failing tests** with a `FakeMenu : MenuSource` in the test file that serves fixed titles and entries per path:
  ```cpp
  void TestLinksRuler();          // fake Ansicht as measured (E8): [P,S,1233,256,391,P,P,393,S,255,265,P,S,415,416,371,369,P,261,P,S,P,P,S,P,1180,1176,P,S,P]
                                  // -> "Lineal": liveId 255, menuPath L"Ansicht", position {ansichtIndex, 9}
  void TestAnchorsFromSegmentEnd();  // same fake: "Orbit" -> 1180, "3D Rundgang" -> 1176 (placeholder 1803 dropped)
  void TestCountMismatchSkipsSegment(); // remove 256 from the fake: "Transparentpause", "Konstruktionsraster darstellen",
                                        // "Darstellung der Bearbeitungsebene" not linked; "Lineal" still 255
  void TestDisabledCarriedOver();  // entry 255 enabled=false -> LinkedCommand.enabled == false
  void TestGraphicOverrideList();  // fake GO popup [408,S,1840..1849,S,566,S,407], list {23040, 24 names}:
                                   // names[1] L"20 Vorentwurfsplanung" -> 1841; "Grafische Überschreibungsregel..." -> 407;
                                   // only 10 list entries linked; menuPath L"Dokumentation › Grafische Überschreibung"
  void TestListTooShortNotOffered();  // list with 3 names for 10 live entries -> none of 1840..1849 linked
  void TestFitScoreRejectsOtherLanguage(); // fake titles L"View", L"Document" -> FitScore == -1, Link returns empty
  void TestStaleExport();          // IsExportStale(t, {t-1, t+1}) true; (t, {t-1}) false; (t, {}) false
  ```

- [ ] **Step 3: Run to see them fail.** `ctest --test-dir build -C Release -R CommandLineNative --output-on-failure`. Expected: failures for the new tests.

- [ ] **Step 4: Implement `MenuLink`.** Match top-level menus by title. Inside a menu, split catalogue and live entries into segments at separators. Align segments from the start up to the first segment that holds a placeholder, and from the end back to the last one; live segments left in the middle form the placeholder region. Inside an aligned segment: link item by item when counts are equal; when they differ and the segment holds placeholders, link items before the first placeholder from the segment start and items after the last placeholder from the segment end; link nothing else. Types must match at every linked position (Item↔Item, Popup↔Popup), otherwise drop the segment. Recurse into aligned popups. A placeholder region whose placeholder number is in `lists`: its first live segment is the list; link entry *k* to `names[k]` only if `names.size()` ≥ entry count.

- [ ] **Step 5: Run the tests.** Expected: all `[PASS]`; five checks green.

- [ ] **Step 6: Commit.** `feat(command-line): link scheme names to live menu positions`.

### Task 3: Live menu, loading and running (stage 5)

**Files:**
- Create: `addon/include/CommandSource.hpp`, `addon/src/CommandSource.cpp`
- Create: `tests/test_live_menu.cpp`
- Modify: `addon/include/SearchEngine.hpp` (`ItemKind::Command`; `SearchItem` gets `std::vector<int> menuPosition; unsigned liveId = 0; bool enabled = true;`)
- Modify: `addon/src/CommandExecutor.cpp:70-83` (case `ItemKind::Command`)
- Modify: `addon/src/HudWindow.cpp:402-404` (append command items on `WM_BAAZ_REFRESH_INDEX`)
- Modify: `tests/CMakeLists.txt`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `MenuSource`, `MenuLink`, `DynamicList` (Task 2); `SchemeReader` (Task 1); `MainWindow()` from `BaazAddon.hpp`.
- Produces:
  ```cpp
  class LiveMenu : public MenuSource {     // Win32 menu bar of a frame window
  public:
      explicit LiveMenu(HWND frame);
      std::vector<std::wstring> TopLevelTitles() override;
      std::vector<LiveEntry> Entries(const std::vector<int>& path) override; // sends WM_INITMENU once, WM_INITMENUPOPUP per level
  };
  class CommandSource {
  public:
      // Reads %USERPROFILE%\.baaz\commands\*.xml (re-read only when a file time changes),
      // picks the export with the best FitScore, checks staleness, links.
      static std::vector<SearchItem> BuildCommandIndex(HWND frame, const std::vector<DynamicList>& lists);
      // Re-fills the path, requires Item + enabled + same liveId, then PostMessage(frame, WM_COMMAND, liveId, 0).
      static bool Run(const SearchItem& item, HWND frame);
  };
  ```

- [ ] **Step 1: Write the failing tests** in `tests/test_live_menu.cpp`: a real test window with a Win32 menu bar (`CreateMenu`, `AppendMenuW`) whose window procedure appends items to an empty popup on `WM_INITMENUPOPUP` and records ids from `WM_COMMAND`:
  ```cpp
  void TestFillsOnDemand();          // popup empty before; LiveMenu::Entries({0}) returns the appended items
  void TestRunPostsCommand();        // Run(item with liveId 255) -> pump -> recorded id == 255
  void TestRunRefusesDisabled();     // item greyed (MF_GRAYED) -> Run returns false, nothing recorded
  void TestRunRefusesChangedId();    // window proc now appends 256 at that position -> Run false, nothing recorded
  void TestStatusLineWhenStale();    // temp folder with export older than a fake .bin carrying the marker
                                     // -> BuildCommandIndex returns exactly one ItemKind::Help item
  ```
  Make the commands folder and the `%APPDATA%` root injectable for tests through a `CommandSource::SetFoldersForTest(std::wstring commands, std::wstring appData)` in the non-API build only (`#ifndef ACAPI_VERSION`).

- [ ] **Step 2: Run to see them fail.** `ctest --test-dir build -C Release -R LiveMenu --output-on-failure`. Expected: build or assert failures.

- [ ] **Step 3: Implement `LiveMenu` and `CommandSource`.** Export choice (spec section 5): parse every `.xml`, take the one with the highest `MenuLink::FitScore`; if the best score is -1 or two files share it, offer no commands and return the out-of-date Help item below. Staleness: from `GetModuleFileNameW(nullptr, …)` take the Archicad folder name (`Archicad 29`) and its major version; list `%APPDATA%\GRAPHISOFT\Archicad <major>.*` folders; collect `*.bin` files up to three levels deep whose first 512 bytes contain the scheme marker; their newest write time is compared with the chosen export's write time by `MenuLink::IsExportStale`. If no folder is found, skip the check and return one Help item `"Command list: cannot check whether the export is current"` once per session. Stale or no fitting export (with at least one `.xml` present): return one Help item `"Command list is out of date. Export the command scheme again (see README)."` and no commands. No `.xml` at all: return nothing.

- [ ] **Step 4: Wire it in.** `CommandExecutor::Execute`: `case ItemKind::Command: return CommandSource::Run(item, MainWindow());`. `HudWindow` `WM_BAAZ_REFRESH_INDEX`: build the navigator index, append `CommandSource::BuildCommandIndex(MainWindow(), {})`, pass the union to `SearchEngine::SetProjectItems`.

- [ ] **Step 5: Run all tests.** Five checks green.

- [ ] **Step 6: Manual check in Archicad 29** (integration branch, see Global Constraints): copy Dave's export to `%USERPROFILE%\.baaz\commands\`; open HUD, type `Lineal`, Enter → ruler toggles. Then change any menu item in *Options › Work Environment › Menus*, confirm that a `Befehle\*.bin` file time changes, and that the HUD now shows the out-of-date line. Note how long the first HUD open takes with filling; if the delay is noticeable, report it before Task 4.

- [ ] **Step 7: Commit.** `feat(command-line): fill and run Archicad menu commands by name`.

### Task 4: Search and display (stage 6)

**Files:**
- Create: `addon/include/ResultText.hpp`, `addon/src/ResultText.cpp`
- Modify: `addon/src/SearchEngine.cpp:275-300` (ranking), `addon/src/HudWindow.cpp:321-369` (tag column, greyed rows)
- Modify: `tests/test_search_engine_native.cpp`, `tests/test_command_line_native.cpp`
- Modify: `README.md` (export workflow: where to export, which file, the folder, the out-of-date line)

**Interfaces:**
- Consumes: `ItemKind::Command`, `SearchItem::enabled` (Task 3).
- Produces:
  ```cpp
  // Trims from the left, keeping whole parts: "… › Grafische Überschreibung"
  std::wstring TrimMenuPathLeft(const std::wstring& path, const std::function<float(const std::wstring&)>& width,
                                float maxWidth);
  ```

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  // test_search_engine_native.cpp
  void TestToolBeforeCommand();   // SetProjectItems({Command L"Wall Tool"}); Query(L"wall tool")[0].kind == ItemKind::Tool
  void TestCommandFound();        // SetProjectItems({Command L"Lineal", tag L"Ansicht"}); Query(L"lineal")[0].title == L"Lineal"
  // test_command_line_native.cpp, width = character count
  void TestTrimKeepsLastParts();  // TrimMenuPathLeft(L"Dokumentation › Grafische Überschreibung", len, 28)
                                  //   == L"… › Grafische Überschreibung"
  void TestTrimShortPathUnchanged(); // L"Ansicht" with maxWidth 28 unchanged
  ```

- [ ] **Step 2: Run to see them fail.** Both test targets.

- [ ] **Step 3: Implement.** Ranking (D4): before sorting, a Command candidate's score is lowered by `kCommandRankPenalty = 10.0`, so a tool with a comparable match comes first; the existing tie-break stays. HUD: the tag column is `max(90 × scale, measured tag width)` capped at 40 % of the HUD width; the tag text goes through `TrimMenuPathLeft`; rows with `enabled == false` draw title and tag with the muted brush, and Enter on them does nothing (D3; `Run` refuses them anyway).

- [ ] **Step 4: Run all tests, update README (R20).** Five checks green.

- [ ] **Step 5: Manual check in Archicad 29.** Type `überschreibung`: results show the menu path on the right, trimmed from the left where long; a command that is greyed out in Archicad's menu is greyed out in the HUD.

- [ ] **Step 6: Commit.** `feat(command-line): show menu commands with their menu path`.

### Task 5: Dynamic lists (stage 7)

**Files:**
- Modify: `addon/include/NavigatorIndexer.hpp`, `addon/src/NavigatorIndexer.cpp` (`static std::vector<DynamicList> DynamicLists();`, plus API keys for entries)
- Modify: `addon/include/SearchEngine.hpp` (`ItemKind::GraphicOverrideCombo`, `ItemKind::ModelViewOption`), `addon/src/CommandExecutor.cpp`, `addon/src/CommandSource.cpp`, `addon/src/HudWindow.cpp:403`
- Modify: `addon/include/MenuLink.hpp` (`DynamicList` gains `std::vector<std::string> keys;` parallel to `names`)
- Modify: `tests/test_command_line_native.cpp`

**Interfaces:**
- Consumes: `DynamicList`, `MenuLink::Link`, `CommandSource::BuildCommandIndex` (Tasks 2–3).
- Produces: `NavigatorIndexer::DynamicLists()`; list entries that were not linked to the menu become API items: `ItemKind::GraphicOverrideCombo` (key = combination GUID, Archicad 30 only) and `ItemKind::ModelViewOption` (key = attribute index as text). The key is stored in the existing `SearchItem::guid` field; the menu path goes into `tag` as for commands.

- [ ] **Step 1: Check the order in Archicad 29 before coding.** A throwaway probe (not committed) prints the names from `ACAPI_GraphicalOverride_GetOverrideCombinationList` + `GetOverrideCombination` and from `ACAPI_Navigator_ModelViewOptions_GetNum/Get` in API order. Dave compares them with *Dokumentation › Grafische Überschreibung* and *› Modelldarstellung*. Record: does "Keine Überschreibungen" appear in the API list, and does API order equal menu order? If the order differs, stop and ask Dave before Step 2 (the spec's linking rule depends on it).

- [ ] **Step 2: Write the failing tests.**
  ```cpp
  void TestUnlinkedEntriesBecomeApiItems(); // 24 names, 10 live entries -> 10 Command items + 14 items of the list's API kind
  void TestApiItemsCarryKeys();             // API item k has key == keys[10 + k]
  ```
  `BuildCommandIndex` produces the API items; give it the lists through its existing parameter.

- [ ] **Step 3: Run to see them fail.**

- [ ] **Step 4: Implement.** `NavigatorIndexer::DynamicLists()` returns `{23040, names, guids}` and `{9472, names, indices}` in the order Step 1 confirmed. `CommandExecutor`: `GraphicOverrideCombo` → `ACAPI_GraphicalOverride_SetActualOverrideCombination` under `#if ACAPI_VERSION >= 30`, otherwise these items are not created; `ModelViewOption` → `ACAPI_Navigator_ChangeViewOptions(nullptr, &index)` (the pattern Tapir 1.8.0 uses in `SetViewOptionsCommand`). `HudWindow` passes `NavigatorIndexer::DynamicLists()` into `BuildCommandIndex`.

- [ ] **Step 5: Run all tests.** Five checks green.

- [ ] **Step 6: Manual check in Archicad 29.** Type `vorentwurf` → Enter → "20 Vorentwurfsplanung" becomes active (menu path). Type the name of the 15th combination → not offered in AC29 (expected). Switch a model view option by name. If Archicad 30 is available: the 15th combination switches through the API.

- [ ] **Step 7: Commit.** `feat(command-line): switch graphic override and model view option combinations by name`.

---

## Delivery

Each task is one pull request into the fork's `main`, opened only after the previous one is merged (no stacked branches). Before each `gh pr create`: the PR quality pipeline (review-pr, security-review, verification-before-completion) and Dave's go. PR text in English, stating what was verified in Archicad 29 and that Archicad 30 is untested (R21).
