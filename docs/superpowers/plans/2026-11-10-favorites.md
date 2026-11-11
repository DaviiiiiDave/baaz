---
stages:
  - id: 3
    label: "Search: favorite kind, tool filter, ranking"
  - id: 4
    label: "HUD: Shift+Space always types a space"
  - id: 5
    label: "Archicad: list and apply favorites, README, AC29 build"
  - id: 6
    label: "Acceptance in Archicad 29"
---

# Baaz Favorites Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A favorite of the open project can be found by name in the Baaz HUD; running it applies the favorite to the tool defaults and activates its tool, and the user places the element.

**Architecture:** Favorites become `SearchItem`s of a new kind `ItemKind::Favorite`, read in `NavigatorIndexer` with the Archicad 29+ object API (`ACAPI::Favorite`). `SearchEngine` drops favorites whose (type, variation) has no row in its tool table and ranks favorites between tools and everything else on equal score. `CommandExecutor` applies a favorite with `GetElementDefault()` + `SetAsArchicadDefault()` and then activates the tool as for tools today.

**Tech Stack:** C++20, Win32, GDI+, Archicad DevKit (AC29 / AC30), CMake + Ninja Multi-Config, native assert-style tests.

**Spec:** `docs/superpowers/specs/2026-11-10-favorites-design.md` (read it before any task; section numbers below refer to it).

## Global Constraints

- Archicad API for favorites: `#include "ACAPI/Favorite.hpp"`; `ACAPI::Favorite::GetFavoriteManager()`, `FavoriteManager::GetAllFavorites()`, `FavoriteManager::GetFavoriteByName(const GS::UniString&)`, `Favorite::GetName()`, `Favorite::GetElementType()`, `Favorite::GetParentFolder()`, `FavoriteFolder::IsRoot()`, `FavoriteFolder::GetName()`, `Favorite::GetElementDefault()`, `ElementDefault::SetAsArchicadDefault()`. All return `ACAPI::Result<…>` (`IsOk()`, `IsErr()`, `Unwrap()`). Header: DevKit `Support/Modules/ArchicadAPI/ACAPI/Favorite.hpp`, `@since Archicad 29`.
- Do not use `ACAPI_Favorite_Get`, `ACAPI_Element_ChangeDefaults*` or `ACAPI_CallUndoableCommand`; `tools/check_rules.py` must stay unchanged (spec 9).
- `ACAPI_` and `ACAPI::` calls only in `BaazAddon.cpp`, `CommandExecutor.cpp`, `NavigatorIndexer.cpp` (R5). `SearchEngine.cpp` stays free of API headers.
- Right column text for a favorite: innermost folder name, or `FAV` at top level (spec 4.2).
- Ranking on equal score: Tool, then Favorite, then all other kinds, then title (spec 4.3).
- Function cyclomatic complexity ≤ 12 (R7, `lizard`); no new `whitelizard.txt` entries.
- All code, comments, commit messages, README text in English. Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Author and committer: the repository's configured noreply identity.
- Before every commit run the checks in `CLAUDE.md` ("Commands"). Docs under `docs/` are gitignored: add them with `git add -f`.
- Build directory with DevKit and local MDID: `build/ac29-dev` (configure command in Task 3, step 5). `addon/resources/AddOnFix.local.grc` is present and must never be committed.
- Stage changes need evidence and the maintainer's OK. A task's commit carries the `— Stage N→N+1` suffix and the `.atrium.yaml` update (local, not committed) only after the maintainer approved that change; until then commit without the suffix.
- `MessageHookEdgeCases` is known to be flaky (typically `HudWindow::GetQuery() == L"wa"`, seen before this work). A red run is re-run once; a second red run with a different failure is a real failure.

## Review Focus

1. Favorite names with punctuation and several words ("KS + WDVS 42 cm"): typing the words must find them. Test in Task 1 (`TestFavoriteFoundByWords`).
2. A favorite and a view with the same leading word ("Decke STB 20" favorite, "Decke EG" view): the favorite comes first. Test in Task 1 (`TestFavoriteRanksAfterToolBeforeView`).
3. Component, MEP and unknown-type favorites (type ID with no tool row, type ID 0, wrong variation): never listed. Test in Task 1 (`TestFavoritesWithoutToolAreDropped`).
4. Shift+Space on an empty search, followed by Esc: the HUD stays open with a space, and Esc still closes it. Test in Task 2.
5. Running a favorite in a window where its tool is unavailable (layout, 3D): nothing is created, Archicad does not crash. No test double; acceptance item in Task 4.

---

### Task 1: Search: favorite kind, tool filter, ranking (Stage 3)

**Files:**
- Modify: `addon/include/SearchEngine.hpp`
- Modify: `addon/src/SearchEngine.cpp` (`SetProjectItems`, `Query` comparator `better`, `HelpLines` text)
- Test: `tests/test_search_engine_native.cpp`

**Interfaces:**
- Produces: `ItemKind::Favorite` (appended to `enum class ItemKind`). A favorite `SearchItem` uses `title` = favorite name, `tag` = folder text, `kind` = `ItemKind::Favorite`, `toolType` / `toolVariation` = the favorite's element type ID and variation. No new `SearchItem` fields.
- Produces: `SearchEngine::SetProjectItems` drops every `ItemKind::Favorite` item for which no tool row has the same `typeId` and `variation`, and the row with `typeId == 0` (Arrow) never matches. Requires `SearchEngine::Initialize()` to have run (it does at add-on start, `BaazAddon.cpp:44`).

- [ ] **Step 1: Write the failing tests**

Add a helper in the test file:

```cpp
static SearchItem Favorite(const wchar_t* name, int typeId, int variation, const wchar_t* folder = L"FAV") {
    SearchItem item;
    item.title = name;
    item.tag = folder;
    item.kind = ItemKind::Favorite;
    item.toolType = typeId;
    item.toolVariation = variation;
    return item;
}
```

Tests (register each in `main`, update the "ALL N ... PASSED" count):

```cpp
void TestFavoritesWithoutToolAreDropped() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({
        Favorite(L"KS 24 cm", 1, 0),                         // Wall: listed
        Favorite(L"Eckfenster Holz", 4, 'CRWD'),             // Corner Window: listed
        Favorite(L"Metall - Handlauf", 52, 0),               // railing component: dropped
        Favorite(L"Kabeltrasse", 0, 0),                      // no type: dropped
        Favorite(L"Fenster Sonder", 4, 12345),               // Window type, unknown variation: dropped
    });
    auto favoritesIn = [](const std::wstring& query) {
        size_t n = 0;
        for (const auto& r : SearchEngine::Query(query, 50)) n += (r.kind == ItemKind::Favorite);
        return n;
    };
    assert(favoritesIn(L"ks 24") == 1);
    assert(favoritesIn(L"eckfenster") == 1);
    assert(favoritesIn(L"handlauf") == 0);
    assert(favoritesIn(L"kabeltrasse") == 0);
    assert(favoritesIn(L"sonder") == 0);
}

void TestFavoriteRanksAfterToolBeforeView() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({
        { L"Decke EG", L"VIEW", ItemKind::View, "GUID_DECKE" },
        Favorite(L"Decke STB 20", 8, 0, L"Decken+Böden"),
        Favorite(L"Wallpaper", 1, 0),
    });
    auto decke = SearchEngine::Query(L"decke");
    assert(decke.size() >= 2);
    assert(decke[0].kind == ItemKind::Favorite && decke[0].title == L"Decke STB 20");
    assert(decke[1].kind == ItemKind::View);
    assert(decke[0].tag == L"Decken+Böden");

    // "Curtain Wall Tool", "Wall End Tool", "Wall Tool" and "Wallpaper" all score 93
    auto wall = SearchEngine::Query(L"wall", 20);
    size_t fav = 0;
    while (fav < wall.size() && wall[fav].kind != ItemKind::Favorite) ++fav;
    assert(fav > 0 && fav < wall.size());
    for (size_t i = 0; i < fav; ++i) assert(wall[i].kind == ItemKind::Tool);

    assert(SearchEngine::Query(L"wa")[0].title == L"Wall Tool");   // exact chord still wins
}

void TestFavoriteFoundByWords() {
    SearchEngine::Initialize();
    SearchEngine::SetProjectItems({ Favorite(L"KS + WDVS 42 cm", 1, 0, L"Wände") });
    auto results = SearchEngine::Query(L"ks wdvs");
    assert(!results.empty());
    assert(results[0].title == L"KS + WDVS 42 cm");
}
```

- [ ] **Step 2: Build and run, verify the new tests fail**

Run: `cmake --build build/ac29-dev --config Release --target test_search_engine_native && build/ac29-dev/tests/Release/test_search_engine_native.exe`
Expected: compile error (`Favorite` is not a member of `ItemKind`), then, after adding only the enum value, assertion failures in the three new tests.

- [ ] **Step 3: Implement**

- `SearchEngine.hpp`: append `Favorite` to `ItemKind`; declare `private: static bool HasTool(int typeId, int variation);`.
- `SearchEngine.cpp`: `HasTool` returns true if a row of `s_tools` has `typeId != 0` and the same `toolType` and `toolVariation`. `SetProjectItems` skips favorite items for which `HasTool` is false.
- Comparator `better` in `Query`: replace the tool-first rule with a kind rank (Tool 0, Favorite 1, everything else 2) as a file-local `static int KindRank(ItemKind kind)`; keep score first and title last.
- `HelpLines`: the search line reads `L"Type to search views, sheets, layer combinations, favorites and tools  \x2022  ? help  \x2022  :settings"`.

- [ ] **Step 4: Run all checks, verify green**

Run the five checks from `CLAUDE.md` with `build/ac29-dev` as build directory (`python tools/check_rules.py`, `python -m lizard addon/src tests -C 12 -m -w -W whitelizard.txt`, configure, build, `ctest --test-dir build/ac29-dev -C Release --output-on-failure`).
Expected: all pass; `test_search_engine_native` prints the new count.

- [ ] **Step 5: Commit**

```bash
git add addon/include/SearchEngine.hpp addon/src/SearchEngine.cpp tests/test_search_engine_native.cpp
git commit -m "feat(search): favorites as results, only with a tool of their own — Stage 3→4"
```

---

### Task 2: HUD: Shift+Space always types a space (Stage 4)

**Files:**
- Modify: `addon/src/HudWindow.cpp:371-382` (`HandleKey`, `VK_SPACE`)
- Test: `tests/test_message_hook.cpp` (next to `ShiftSpaceTypesSpace`, registered after it in the `Run` list)

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: in `HandleKey`, Shift held → always `SetQuery(s_query + L' ')`, checked before the empty-query rule. Plain Space keeps its current three cases (spec 4.4).

- [ ] **Step 1: Write the failing test**

```cpp
static void ShiftSpaceOnEmptyQueryTypesSpace(bool peek) {
    Tap(VK_SPACE, peek);                 // open
    BYTE keys[256] = {};
    keys[VK_SHIFT] = 0x80;
    SetKeyboardState(keys);
    Tap(VK_SPACE, peek);
    keys[VK_SHIFT] = 0;
    SetKeyboardState(keys);
    CHECK(HudWindow::IsVisible());
    CHECK(HudWindow::GetQuery() == L" ");
    Tap(VK_ESCAPE, peek);
    CHECK(!HudWindow::IsVisible());
}
```

Register: `Run("Shift+Space on an empty query types a space", ShiftSpaceOnEmptyQueryTypesSpace, peek);`

- [ ] **Step 2: Run, verify it fails**

Run: `cmake --build build/ac29-dev --config Release --target test_message_hook && ctest --test-dir build/ac29-dev -C Release -R MessageHookEdgeCases --output-on-failure`
Expected: FAIL in the new test (`IsVisible()` false: today Shift+Space on an empty query closes the HUD). "Space on empty query closes HUD" still passes.

- [ ] **Step 3: Implement** the Shift check first in the `VK_SPACE` case and update the comment above it to the new rule.

- [ ] **Step 4: Run all checks, verify green** (same five checks as Task 1, step 4; flaky rule from Global Constraints applies).

- [ ] **Step 5: Commit**

```bash
git add addon/src/HudWindow.cpp tests/test_message_hook.cpp
git commit -m "feat(hud): Shift+Space types a space also on an empty search — Stage 4→5"
```

---

### Task 3: Archicad: list and apply favorites, README, AC29 build (Stage 5)

**Files:**
- Modify: `addon/src/NavigatorIndexer.cpp` (new `CollectFavorites`, called from `BuildProjectIndex`)
- Modify: `addon/src/CommandExecutor.cpp` (new `ApplyFavorite`, API and non-API versions; `Execute` switch)
- Modify: `README.md` (feature list; key table row "Shift+Space")

**Interfaces:**
- Consumes: `ItemKind::Favorite` and the favorite `SearchItem` layout from Task 1.
- Produces: `static void CollectFavorites(std::vector<SearchItem>& items)` in `NavigatorIndexer.cpp`; `static bool ApplyFavorite(const SearchItem& item)` in `CommandExecutor.cpp`.

No native test: both functions are API-only (spec 5, R18 row); the non-API `ApplyFavorite` returns `true` like the other non-API stubs there.

- [ ] **Step 1: `CollectFavorites`** (inside `#ifdef ACAPI_VERSION`, include `"ACAPI/Favorite.hpp"` there)

For each favorite of `GetFavoriteManager().GetAllFavorites()`: skip if `!IsValid()` or name or element type is `IsErr()`. Fill a `SearchItem` per Task 1's layout; `toolType` / `toolVariation` from `API_ElemType::typeID` / `variationID`. Folder text: `GetParentFolder()`; if `IsErr()` or `IsRoot()` → `L"FAV"`, else the folder's `GetName()`. Filtering is not done here; `SearchEngine::SetProjectItems` does it.
Wrap the loop in `try { … } catch (...)`: on an exception, drop the favorites collected in this call and return; views, layouts and layer combinations stay. Call it last in `BuildProjectIndex`.

- [ ] **Step 2: `ApplyFavorite`** (API version)

`GetFavoriteByName(GS::UniString(reinterpret_cast<const GS::UniChar::Layout*>(item.title.c_str())))` → `GetElementDefault()` → `SetAsArchicadDefault()`; any `IsErr()` returns `false`; then `return SetTool(item);`. Whole body in `try { … } catch (...) { return false; }`. Non-API version: `return true;`. Add `case ItemKind::Favorite: return ApplyFavorite(item);` to `Execute`.

- [ ] **Step 3: README**

Feature list, after "Layer combinations": `- **Favorites** of the open project, by name: the tool becomes active with the favorite's settings, and you place the element yourself. Only favorites with a tool of their own are listed (not railing posts, stair treads and other components).`
Key table row: `| Shift+Space | Type a space (multi-word searches), also on an empty search |`

- [ ] **Step 4: Run all checks, verify green** (five checks; `python tools/check_rules.py` must pass unchanged).

- [ ] **Step 5: Build the deploy APX and check its MDID**

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.42
cmake -S . -B build/ac29-dev -G "Ninja Multi-Config" -DBAAZ_AC_VERSION=29 -DBAAZ_DEV_MDID=ON -DARCHICAD_DEVKIT_DIR="<Archicad 29 DevKit folder>"
cmake --build build/ac29-dev --config Release
```

Expected: `build/ac29-dev/addon/Release/Baaz.apx` exists; a byte scan finds the registered MDID pair from `AddOnFix.local.grc` (little-endian, in that order) and not the placeholder pair from `AddOnFix.grc`.

- [ ] **Step 6: Commit**

```bash
git add addon/src/NavigatorIndexer.cpp addon/src/CommandExecutor.cpp README.md
git commit -m "feat: list favorites and apply them to the tool defaults — Stage 5→6"
```

---

### Task 4: Acceptance in Archicad 29 (Stage 6)

Done by hand in Archicad; the build is deployed and the results are read back.

- [ ] **Step 1: Deploy.** Archicad closed (check the process `Archicad.exe`, not `archicad-server`). Back up the installed APX as `Baaz.apx.bak-vor-favoriten`, copy the new one with elevation (`Start-Process pwsh -Verb RunAs -Wait`), compare hashes.
- [ ] **Step 2: Run the acceptance list from spec section 6** (read the favorite names from Archicad beforehand and note one search word per item). Includes Review Focus 5 (layout, 3D window).
- [ ] **Step 3: Record results** in the PR description: per item worked / differed / not available in the template. Any partial application or failing type: stop and report to the maintainer (spec 9).
