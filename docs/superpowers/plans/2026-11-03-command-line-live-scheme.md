---
stages:
  - id: 5
    label: "Live menu + live scheme (Task 3, 3a, 3b, 3c)"
---

# Command Line: Live Scheme Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Menu commands keep working after the user changes Archicad's menus, without a new export; every state without commands is explained in the HUD with a short guide.

**Architecture:** A strict hand-written reader turns the active command-scheme `.bin` into the same `MenuNode` tree the XML reader produces, without item names. Names are attached by full command id from the newest fitting export. `CommandSource` picks the `.bin` by fit score, drops the time-based staleness check and reports a state that the HUD shows as one pinned hint row.

**Tech Stack:** C++20, Win32, CMake + Ninja Multi-Config, native assert-style tests (as the base plan).

**Spec:** `docs/superpowers/specs/2026-11-03-command-line-live-scheme-design.md` (the addendum; section numbers below refer to it) on top of `docs/superpowers/specs/2026-11-02-command-line-design.md` and its plan `docs/superpowers/plans/2026-11-02-command-line.md` (Tasks 1-3 done). Tasks 4 and 5 of the base plan stay; Task 4's README line changes (end of this plan).

## Global Constraints

- All Global Constraints of the base plan apply, with these changes: commits end with the `Co-Authored-By` trailer of the model that writes the commit; the time-based staleness check is gone.
- No Graphisoft `.bin` file and no personal or office scheme is committed (addendum 7, option C). Automated tests build synthetic `.bin` bytes in test code.
- The local real-file test takes its paths only from the environment variables `BAAZ_TEST_SCHEME_BIN` and `BAAZ_TEST_SCHEME_XML` (`;`-separated lists of equal length, pairs of the same state); unset → it prints `[SKIP] real scheme files not configured` and passes. No path is written into code or CMake.
- Scheme marker: `6FCD168F-A26C-4547-AF9D-ED9A5FC54DD7` (first 512 bytes of a `.bin`; `componentType` of the XML `<Scheme>` root). Size cap: `kMaxInputBytes` (8 MB) for every file.
- Observed `.bin` header: `57 57 78 56 34 12 05 00`. Any other header → unreadable.
- Work Environment command id: `7682-0x00000000-0x00000000`.
- Full command id = number + MDID part 1 + MDID part 2. Names, placeholders and the Work Environment lookup use the full id.
- HUD hint texts in English, like the other HUD texts.

## Review Focus

1. **`.bin` written by a newer Archicad with a changed layout** → nothing offered, the "cannot be read" hint, no crash. Tests: Task 3a `TestUnknownHeaderIsUnreadable`, `TestUnknownRecordIsUnreadable`, `TestTruncatedIsUnreadable`.
2. **A command moved to another menu without exporting** → offered and run at its new place. Test: Task 3b `TestMovedCommandKeepsItsName`.
3. **A command added to a menu after the export** → not offered, counted, "N commands are new" hint. Test: Task 3b `TestUnnamedCommandIsCountedNotOffered`.
4. **Two scheme files fit equally and were written at the same time** → no commands (A1). Test: Task 3b `TestTiedSchemesWithSameTimeOfferNothing`.
5. **Typing `lineal` and pressing Enter while a hint is shown** → runs *Lineal*, not the hint. Test: Task 3c `TestHintIsNeverDefaultSelection`.

---

### Task 3a: `.bin` scheme reader (stage 5)

**Files:**
- Create: `addon/include/BinSchemeReader.hpp`, `addon/src/BinSchemeReader.cpp`
- Create: `tests/test_bin_scheme.cpp` (synthetic `.bin` builder + reader tests + local real-file test)
- Modify: `addon/include/SchemeReader.hpp` (`MenuNode` gains `unsigned mdid1 = 0; unsigned mdid2 = 0;`), `addon/src/SchemeReader.cpp` (fill them from `command="n-0x…-0x…"`)
- Modify: `tests/test_command_line_native.cpp` (MDID parsing), `tests/CMakeLists.txt`, `CMakeLists.txt`, `CLAUDE.md` (Layout row)

**Interfaces:**
- Consumes: `MenuNode`, `MenuNodeType`, `CleanName`, `kMaxInputBytes` (Task 1).
- Produces:
  ```cpp
  struct BinScheme {
      std::string guid;                  // scheme GUID from the header, e.g. "2F4881B8-…"
      std::vector<MenuNode> floorPlanBar; // "Nicht-Text" bar; popup titles set, item names empty
  };
  class BinSchemeReader {
  public:
      // false = unreadable (unknown header or record, bad length, over budget, no floor-plan bar);
      // out is untouched then. Never a partial tree.
      static bool Read(const std::string& bytes, BinScheme& out);
  };
  ```

- [ ] **Step 1: Derive the grammar from the real files, read-only.** Use a `.bin` and the XML export of the same state (addendum 8 method; the controller supplies a local pair and an AC30 standard file). Write the grammar as the header comment of `BinSchemeReader.hpp`: header, every record type with its tag, fields, lengths and how submenus and both menu bars are referenced, and how `dynamic` items are marked. The 55- and 39-byte records must be explained there; anything unexplained makes `Read` return false. Also find out, and write into the report: what `Zuletzt verw. Arbeitsumgebung\Befehle\Befehle.bin` stands for (it appears while menus are changed and vanishes when the change is undone), and whether the active profile can be told reliably from `Arbeitsumgebung\ProfileSet.xml` or `Zuletzt verw. Arbeitsumgebung\Profile\Profile.xml` (addendum 8).
- [ ] **Step 2: Write the failing tests** in `tests/test_bin_scheme.cpp` with a test-code builder for synthetic `.bin` bytes that follows the grammar of Step 1:
  ```cpp
  void TestReadsMenusSeparatorsAndPopups();   // bar [Menu "Ansicht" [Item 21761, Sep, Popup "X" [Item 1557]]] round-trips: types, numbers, mdids, titles, order
  void TestKeepsMdidParts();                  // Item 2129920001-0x47732f64-0x38c147a6 -> mdid1 == 0x47732f64, mdid2 == 0x38c147a6
  void TestIgnoresTextMenuBar();              // the "Text" bar (GUID 2CAC215F-…) is not in floorPlanBar
  void TestUnknownHeaderIsUnreadable();       // first byte changed -> Read false
  void TestUnknownRecordIsUnreadable();       // unknown tag inside a menu -> Read false
  void TestTruncatedIsUnreadable();           // every prefix shorter than the full file -> Read false
  void TestOversizeIsUnreadable();            // kMaxInputBytes + 1 -> Read false
  void TestRealFilesMatchTheirExport();       // env pairs: Read(bin) == ReadFloorPlanMenuBar(xml) on type, number, mdids, popup titles, order; item names ignored; [SKIP] when unset
  ```
  Plus in `test_command_line_native.cpp`: `TestSchemeReaderKeepsMdidParts` (same id as above, from XML).
- [ ] **Step 3: Run to see them fail.** `ctest --test-dir build -C Release -R "BinScheme|CommandLine" --output-on-failure` → build or assert failures.
- [ ] **Step 4: Implement `BinSchemeReader::Read`** with the same strictness as `SchemeReader`: bounds check before every read, a node budget, recursion depth ≤ 64, titles through `CleanName`. Add the MDID fields to `SchemeReader`.
- [ ] **Step 5: Run the five checks** (base plan). Then run `TestRealFilesMatchTheirExport` once with the controller's pairs set and paste its output into the report.
- [ ] **Step 6: Commit.** `feat(command-line): read the command scheme Archicad keeps current`

### Task 3b: Names, scheme choice, no staleness (stage 5)

**Files:**
- Modify: `addon/include/MenuLink.hpp`, `addon/src/MenuLink.cpp` (`LinkedCommand` gains `commandNumber`, `mdid1`, `mdid2`; unnamed items are linked with an empty name; `IsExportStale` removed; placeholder table keyed by full id)
- Modify: `addon/include/CommandSource.hpp`, `addon/src/CommandSource.cpp`
- Modify: `tests/test_command_line_native.cpp`, `tests/test_live_menu.cpp`

**Interfaces:**
- Consumes: `BinSchemeReader::Read`, `BinScheme` (Task 3a); `LiveMenu`, `Run`, title matching (Task 3).
- Produces:
  ```cpp
  // Sets the name of every Item of bar whose full id has an Item in exportBar; others stay empty.
  void AttachNames(std::vector<MenuNode>& bar, const std::vector<MenuNode>& exportBar);   // MenuLink.hpp
  enum class CommandState { Ready, NoExport, NewCommands, Unreadable };
  struct CommandIndex {
      std::vector<SearchItem> commands;   // named, linked commands only
      CommandState state = CommandState::Unreadable;
      size_t unnamed = 0;                 // NewCommands: linked, non-placeholder items without a name
      SearchItem workEnvironment;         // kind Command, position of 7682; liveId 0 if not found
  };
  static CommandIndex CommandSource::BuildCommandIndex(HWND frame, const std::vector<DynamicList>& lists);
  ```

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  void TestAttachNamesByFullId();                 // same number, different mdids -> each gets its own name
  void TestMovedCommandKeepsItsName();            // structure has 1557 in another menu than the export -> linked at the new position as "Lineal"
  void TestUnnamedCommandIsCountedNotOffered();   // id unknown to the export -> not in commands, unnamed == 1, state NewCommands
  void TestPlaceholderIsNeverCounted();           // 23040 / 9472 without names -> unnamed == 0
  void TestNewestFittingExportGivesNames();       // two fitting exports (sub-folders) -> names of the newer; a non-fitting newer one is ignored
  void TestExportFoundBySchemeMarker();           // export in "<any>\Befehle\x.xml" found; an XML without the marker ignored
  void TestSchemeChosenByFitThenTime();           // higher fit wins; equal fit -> newer write time wins
  void TestTiedSchemesWithSameTimeOfferNothing(); // equal fit and equal time -> commands empty, state Unreadable
  void TestNoExportState();                       // .bin present, no export -> state NoExport, commands empty, workEnvironment found
  void TestUnreadableBinState();                  // only an unreadable .bin -> state Unreadable
  void TestMenuChangeWithoutExportStillOffers();  // replaces the old stale-export tests: newer .bin than export -> commands offered
  ```
  Delete the tests of the removed staleness check (`TestStaleExport`, `TestStatusLineWhenStale`, the "cannot check" tests) and say in the commit message why.
- [ ] **Step 2: Run to see them fail.** `ctest --test-dir build -C Release -R "CommandLine|LiveMenu" --output-on-failure`.
- [ ] **Step 3: Implement.** Candidates: every marker `.bin` found as today under `%APPDATA%\GRAPHISOFT\Archicad <major>…` (≤ 3 levels), read with `BinSchemeReader`, cached by path + write time + size. Exports: recursive under `.baaz\commands`, by marker, cached the same way. Choice and states exactly as addendum 3-5. Only if Task 3a's report established the active profile reliably: its scheme GUID picks the `.bin` and the fit score must still be ≥ 0; otherwise fit score, then write time. Remove `CheckFreshness`, `Freshness`, `kCannotCheck`, `s_checkNoted`, `IsExportStale`. Placeholder table by full id: set the MDID parts of 1027, 1293, 20754 from the standard export so that only the dynamic variant stays a placeholder; record in the report which static commands become offerable. `CommandExecutor` and `HudWindow` adapt to `CommandIndex` (HUD shows nothing new yet; Task 3c).
- [ ] **Step 4: Run the five checks.**
- [ ] **Step 5: Commit.** `feat(command-line): take the menu structure from the live scheme`

### Task 3c: Hint row in the HUD (stage 5)

**Files:**
- Create: `addon/include/CommandHint.hpp`, `addon/src/CommandHint.cpp` (texts and selection rule, no Win32)
- Modify: `addon/src/HudWindow.cpp` (pinned row above the results, keys, click), `addon/src/CommandExecutor.cpp` (remember a refused run)
- Modify: `tests/test_search_engine_native.cpp` or a new `tests/test_command_hint.cpp`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `CommandIndex`, `CommandState` (Task 3b); `CommandSource::Run`.
- Produces:
  ```cpp
  struct HintRow { std::wstring title; std::vector<std::wstring> guide; bool canOpenWorkEnvironment = false; };
  // Empty title = no row. lastRunRefused adds "The last command did not run: its menu item changed or is disabled."
  HintRow MakeHintRow(CommandState state, size_t unnamed, bool workEnvironmentFound, bool lastRunRefused);
  // Index of the first result row selected after a query: always 0 (the first result), never the hint.
  size_t DefaultSelection(bool hasHint, size_t resultCount);
  ```

- [ ] **Step 1: Write the failing tests.**
  ```cpp
  void TestNoHintWhenReady();               // Ready, 0, true, false -> title empty
  void TestNoExportHintHasGuide();          // guide: Enter opens Work Environment -> export the profile to …\.baaz\commands -> open the HUD again
  void TestNewCommandsHintCounts();         // NewCommands, 3 -> title contains "3"
  void TestUnreadableHintHasNoGuide();      // Unreadable -> no guide, canOpenWorkEnvironment false
  void TestGuideNamesMenuPathWithoutCommand(); // workEnvironmentFound false -> guide names "Options › Work Environment" instead of Enter
  void TestHintIsNeverDefaultSelection();   // DefaultSelection(true, 5) selects the first result, not the hint
  void TestRefusedRunIsShownOnce();         // lastRunRefused true -> line present; next build without refusal -> gone
  ```
- [ ] **Step 2: Run to see them fail.**
- [ ] **Step 3: Implement** `CommandHint` and wire it: the row is drawn above the results, takes no slot from `maxResults`, is reached with Up from the first result, runs on Enter or click only (Space never), and Enter runs `workEnvironment` through `CommandSource::Run`.
- [ ] **Step 4: Run the five checks.**
- [ ] **Step 5: Manual check in Archicad 29** (integration branch as in the base plan; Dave): remove every `.xml` and `.xml.bak` file from `.baaz\commands` → hint "export", Enter opens Work Environment; export the profile into `.baaz\commands`, OK → *Lineal* runs; move *Lineal* to another menu, OK, no export → *Lineal* runs from the new place; add a command the export does not know → "1 commands are new"; type `lineal` + Enter with a hint shown → ruler toggles. Repeat in Archicad 30 when its export of the same state exists.
- [ ] **Step 6: Commit.** `feat(command-line): explain missing commands in the HUD`

## Changes to the base plan

- **Task 3's PR** carries Tasks 3a-3c on the same branch (`feature/befehlszeile-livemenu`), because Task 3 alone never offers commands in the normal export flow (addendum 2). This is one deviation from "one PR per task" and needs Dave's go.
- **Task 4, README line** becomes: export the whole profile from *Options › Work Environment* into `%USERPROFILE%\.baaz\commands\`; once is enough until new commands are added; the three hints and what they mean; the undocumented `.bin` format and what happens after an Archicad update.
- **Task 4, ranking and display** stay; the hint row of Task 3c is not a search result and is not ranked.
