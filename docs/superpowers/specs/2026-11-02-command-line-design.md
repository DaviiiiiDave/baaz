# Baaz command line: every Archicad command by name

**Status:** draft for review, 2026-11-02
**Repository:** fork `DaviiiiiDave/baaz`, branch `feature/befehlszeile`. Not yet proposed upstream.
**Archicad versions:** 29 (tested), 30 (target, untested)

## 1. Intent

Baaz today is a switcher. It knows 43 hand-listed tools plus the views, layouts and layer
combinations of the open project. The wish behind this design: work in Archicad the way Rhino
users work, where *every* command can be reached by typing its name.

> "Eigentlich will man auf alles zugreifen was so geht. Halt so wie in Rhino."
> (Dave, 2026-11-02)

Rhino's second half, a command that then asks for its options in the command line, has no
counterpart in Archicad: options live in the Info Box and the Pet Palette. That part is out of
scope here (see section 9).

Three needs make the hand-listed approach a dead end:

- **Coverage.** Archicad has several hundred menu commands. A hand-kept table never catches up.
- **Language.** Archicad ships in many languages. A hand-kept table needs a translation per
  language. Baaz must work in every Archicad language (requirement from Dave).
- **Office menus differ.** Users rebuild their menus in *Options › Work Environment › Menus*.
  A fixed table does not know where a command sits in someone's menu.

## 2. Goals, non-goals, anti-goals

**Goals**

- G1. Every command in the user's own Archicad menu bar can be found by its name, as Archicad
  shows it in the user's language, and run from the HUD.
- G2. Entries of dynamic menu lists can be run the same way, where the menu offers them:
  graphic override combinations, model view option combinations, layer combinations, pen sets.
  This gives switchable graphic overrides in Archicad 29, where the API offers no setter.
- G3. A result shows where the command lives (its menu path) so similar names can be told apart.
- G4. Works with a customised menu bar, not only with Archicad's standard one.

**Non-goals**

- Command options inside the command line (Rhino's `Circle: Deformable / Vertical / 2Point`).
- Commands that exist only on toolbars or in context menus, not in the menu bar.
- macOS.

**Anti-goals: this would be a failure even if it technically works**

- A1. Baaz runs a *different* command than the one the user picked. Any doubt about which live
  menu item belongs to a name means: do not offer that command.
- A2. The hook path gets slower or does file work (see R15). Typing in Archicad must feel the
  same as without Baaz.
- A3. Baaz changes the user's work environment, menus or schemes on disk.

## 3. What was verified (evidence)

All tests ran on 2026-11-02 against Archicad 29 (German, build 3100) with a project open, using
throwaway PowerShell probes outside the Archicad process. Nothing below has been tested inside
Baaz, and nothing has been tested on Archicad 30.

| # | Question | How | Result |
| - | -------- | --- | ------ |
| E1 | Does the official API run Archicad's own menu commands? | Read `ACAPI_Interface.h`, `ACAPinc.h` (AC29 DevKit) | No. `ACAPI_MenuItem_*` work only on the calling add-on's own menus (`APIERR_BADINDEX` otherwise). No other execute-by-id function exists. |
| E2 | Do Tapir 1.8.0 or the Tapir MCP do it? | Read Tapir 1.8.0 sources, `SzamosiMate/tapir-archicad-MCP` master | No. Tapir switches some state (model view options, graphic override combination from AC30, renovation filter, drawing scale, favorites) but runs no menu commands. The MCP only forwards Tapir and the JSON API. |
| E3 | Is the main window's menu bar a Win32 menu? | `GetMenu` on the `DGFrameWindow` | Yes. Top-level titles are readable and localised (`&Ablage`, `Ansicht`, …). |
| E4 | Can item names be read from the live menu? | `GetMenuString`, UI Automation on an open popup | No. Items are owner-drawn and return no text; UI Automation sees the popup but no children. |
| E5 | Where do the names come from then? | Exported the active command-layout scheme (Work Environment) as XML | The XML lists every menu, item and separator in order, with a stable command id and the item's name in the UI language (see 4.1). |
| E6 | Are submenus filled before the user opens them? | Read item counts | No. They are empty until opened, and emptied again when Archicad rebuilds its menus. |
| E7 | Can they be filled without the user? | Sent `WM_INITMENU` and `WM_INITMENUPOPUP(hSubMenu, index)` to the frame window | Yes, including nested submenus. No command runs. |
| E8 | Does posting the live id run the command? | Matched *View › Ruler* by position (live id 255), `PostMessage(frame, WM_COMMAND, 255, 0)` | Yes, the ruler turned on. |
| E9 | Do dynamic lists work the same way? | *Documentation › Graphic Override* filled, `WM_COMMAND 1841` | Yes, the second entry ("20 Vorentwurfsplanung") became active. |
| E10 | Are all combinations in that menu? | Dave's screenshots | No. The menu shows "No overrides" plus the first 9 combinations, then "More graphic overrides…", which opens a dialog with all 24. |
| E11 | Are live ids stable? | Read before and after a menu rebuild in the same session | Yes, within the session. Unknown across sessions and versions. |

Live ids and command ids are different numbers: *Ruler* is command id `1557` in the XML and live
id `255` in the menu. No arithmetic links them, so the link is made by position.

## 4. How it works

### 4.1 The command catalogue: the exported scheme

Archicad can export the command-layout scheme from the Work Environment as XML. Each menu item
looks like this:

```xml
<Item command="1557-0x00000000-0x00000000" comment="Lineal"/>
<Item command="2129920001-0x47732f64-0x38c147a6" comment="Ebene sichtbar/unsichtbar umschalten"/>
```

- `comment` is the item's name in the language Archicad runs in. This is what Baaz searches.
  `&` marks the access key and is removed for display and search.
- `command` is a stable id: `<number>-<MDID part 1>-<MDID part 2>`. Built-in commands have MDID
  `0x0`. Add-on commands carry the add-on's MDID, and their number is
  `(menu resource id << 16) | item index` (for example `2129920001 = 32500 << 16 | 1`).
- File layout, as seen in the AC29 exports: UTF-8, root `<Scheme>` in namespace
  `http://www.graphisoft.com/Development/WorkEnvironment/Scheme/2018/01`, then
  `GlobalSettings/CommandLayout`. Inside it, `<Menu ID="…" title="…">` elements are defined once,
  either at the top or inline where first used. Anywhere else a menu appears as
  `<Menu IDREF="…"/>`, which points to that definition.
- A menu's children are `<Item command comment>`, `<Separator/>` and
  `<PopupItem><Menu …/></PopupItem>` (a submenu, either defined inline or by `IDREF`). The
  submenu's title is the `title` of the referenced `<Menu>`. Items may carry extra attributes
  (`dynamic="yes"`, `mode`); they are ignored.
- `<MenuBar ID menubarguid name>` holds the top-level menus, each as `<Menu …>` or
  `<Menu IDREF>`. `<ToolBar>` elements are ignored.
- There are exactly two menu bars: context "Nicht-Text" (`menubarguid`
  `2085E4D3-5694-4BB9-ACAC-3AED5BE50091`, used by every non-text window: floor plan, 3D,
  sections, layouts, …) and context "Text" (`2CAC215F-221C-420F-AAEB-8462A659B8D9`, GDL and
  text editors). Baaz uses only the "Nicht-Text" bar. Its `name` is localised ("Grundriss"), so
  the GUID is used, not the name. Which items are enabled still depends on the window type;
  disabled items are handled in 4.3.

The same structure, without names, is stored as binary `.bin` files under
`%APPDATA%\GRAPHISOFT\Archicad <version> <language> R1\Arbeitsumgebung\Befehle\`. The binary
format is undocumented, so Baaz reads only the XML export.

### 4.2 Linking catalogue entries to live menu items

Baaz walks the exported floor-plan menu bar and the live Win32 menu bar side by side:

- top-level menus are matched by title (access keys removed);
- inside a menu, popups and separators must line up; items between two separators form a
  segment;
- a segment whose live item count equals the exported count is matched item by item;
- a segment that differs is either a known dynamic list (4.4) or is skipped. Skipped items are
  not offered (A1).

Placeholder items that Archicad replaces at run time ("Add-On Befehle im …", "Keine …") are why
segments can differ. In the test, *View* had one placeholder that the live menu dropped, and
*Graphic Override* had its placeholder replaced by ten entries.

### 4.3 Filling and running

- **Filling.** Before the menu is read, Baaz sends `WM_INITMENU` and then `WM_INITMENUPOPUP` for
  each submenu, top-down, to the frame window. This runs from the HUD window's own messages,
  never from the hook (R15).
- **Running.** Right before running, Baaz fills the menu path of the chosen item again, checks
  that the linked position still holds the same kind of item, refuses it if it is disabled
  (`MF_GRAYED`; such results are shown greyed out, D3), and posts `WM_COMMAND` with the live id
  to the frame window. If the check
  fails, nothing runs and the HUD says so.

### 4.4 Dynamic lists

Some menus hold lists Archicad builds from the project: graphic override combinations, model view
option combinations, layer combinations, pen sets. The XML has only a placeholder item there.
The placeholder's command id is language-independent, so it identifies the list:

| List | Placeholder in the export | Names from | Note |
| ---- | ------------------------- | ---------- | ---- |
| Graphic override combinations | `23040` "Keine grafischen Überschreibungen" | `ACAPI_GraphicalOverride_GetOverrideCombinationList` (AC27+) | Menu shows "No overrides" + 9 (E10); then `566` "More graphic overrides…" follows. |
| Model view options | `9472` "Keine Modelldarstellungs-Kombinationen" | `ACAPI_Navigator_ModelViewOptions_GetNum/Get` | Menu limit not checked. |
| Layer combinations | `6660` "Keine Ebenenkombinationen" | already indexed by `NavigatorIndexer` | Baaz already switches these through the API; the menu is not needed. |
| Pen sets | `1551` "Keine Stift-Sets" | not checked | |

The live segment that replaces a placeholder is linked to API names by order: live entry *n*
belongs to API entry *n*. This was checked once (E9: live entry 2 = "20 Vorentwurfsplanung",
the second entry in Archicad's own list). Before an entry is run, the count of live entries must
fit the API list (the shown count, or "No overrides" + 9 for graphic overrides); if not, the list
is not offered.

*(My call:)* the first version covers graphic override combinations and model view options.
Pen sets wait until their menu and API order are checked. Layer combinations keep their
existing API path.

Entries the menu does not show are not run through the menu. Where the API can switch them
(graphic override combinations from Archicad 30 with
`ACAPI_GraphicalOverride_SetActualOverrideCombination`, model view options with
`ACAPI_Navigator_ChangeViewOptions`), Baaz uses the API.

### 4.5 What the user sees

A command result shows its name on the left and its menu path on the right, for example
`Lineal` … `Ansicht`, or `20 Vorentwurfsplanung` … `Dokumentation › Grafische Überschreibung`.
The right column is the same column that idea 1 (folder path for views and layouts) uses
instead of `VIEW`/`SHEET`, so both features share one display rule. Today that column is a fixed
90 px (× DPI scale) in `HudWindow::Render`, too narrow for paths. *(My call:)* the column may
grow up to about 40 % of the HUD width; a path that still does not fit is trimmed from the left
and keeps its last parts (`… › Grafische Überschreibung`), since the nearest menu says the most.

### 4.6 Language

Names come from the user's own export, so they are in whatever language Archicad runs in. Baaz
needs no translation tables for commands. The 43 built-in tool entries and their chords stay as
they are.

## 5. Where the export comes from

There is no API to read the active scheme with names, so the user exports it once:

- *Options › Work Environment*, export the profile (Dave's export wrote one XML per scheme,
  among them `Befehle\Befehle_<n>.xml`).
- The user copies the command scheme XML into `%USERPROFILE%\.baaz\commands\`. *(My call: a
  fixed folder inside Baaz's own folder, so no path setting is needed.)*
- If several `.xml` files are there, Baaz checks each one's "Nicht-Text" bar against the live
  menu bar (top-level titles, popup and separator positions) and uses the one that fits. If none
  fits, or more than one fits equally, no commands are offered. *(My call: matching beats "newest
  file", because the newest file is not necessarily the scheme in use.)*

**Staleness.** Position matching cannot see a same-length change: if the user swaps two items
inside a segment after exporting, a name would point at the wrong live item (A1). The live menu
gives no other signal, because item names are not readable (E4) and live ids are not command
ids. So Baaz checks the time instead *(my call)*: Archicad writes the work-environment schemes to
`%APPDATA%\GRAPHISOFT\Archicad <version> <language> R1\Arbeitsumgebung\Befehle\*.bin` when menus
change. If any of these files is newer than the export in `.baaz\commands\`, the export counts
as stale: no commands are offered, and the HUD shows one line asking for a new export. The
folder name contains version and language, so Baaz finds it by listing
`%APPDATA%\GRAPHISOFT\` for the folder that matches the running Archicad; if that is not
possible, staleness is not checked and the HUD says so once.

## 6. Safety

Running menu commands is a different class of action from switching views. Menu commands can
change the project (Delete, Explode, Merge), open dialogs, save, or quit Archicad.

- Commands run only on an explicit Enter or click in the HUD, never from typing alone.
- Mapping doubt means the command is not offered (A1).
- All commands are offered (D1); the safety lies in running exactly the chosen one.

## 7. Relation to REVIEW.md

The fork is not bound by upstream's rules (decision by Dave, 2026-11-02), but a later proposal to
upstream has to deal with them:

| Rule | Conflict | How this design handles it |
| ---- | -------- | -------------------------- |
| R14 Baaz only navigates | Menu commands can change the project. | The fork offers all commands (D1). Upstream would need to agree. |
| R17 Synthesized input only for Magic Wand | `WM_COMMAND` is not `SendInput`, but it is a synthesized click. | Posted only to Archicad's own frame window, only after an explicit choice. |
| R15 Hooks stay cheap | Reading the XML and filling menus cost time. | Done in HUD messages and at project open, never in the hook. |
| R5 API calls in three files | `ACAPI_` calls for dynamic list names. | Belong in `NavigatorIndexer.cpp` / `CommandExecutor.cpp`. Win32 menu calls are not `ACAPI_`. |
| R1 One purpose per PR | The feature is large. | Delivered in steps (section 8). |

## 8. Delivery in steps

Each step is one pull request in the fork and can be tested in Archicad on its own.

1. **Catalogue.** Read the exported XML into a tree of menus, items and command ids. Native
   tests with a trimmed copy of a real export: the "Nicht-Text" bar with *View* and
   *Documentation*, one add-on submenu, and the "Text" bar header. Before committing the
   fixture, remove personal data: the AC29 export contains the user's Graphisoft ID as a menu
   item (*Help* menu) and the names of installed third-party add-ons.
2. **Linking.** Walk catalogue and live menu side by side (4.2). The live menu is behind a small
   interface so tests can feed a fake menu; the linking rules, placeholders and staleness
   decision are tested natively.
3. **Running.** Filling, re-check, `WM_COMMAND`. These Win32 calls into Archicad's own window
   have no test double and are verified by hand in Archicad. First run: *View › Ruler*.
4. **Search and display.** Command results in the HUD with menu path on the right.
5. **Dynamic lists.** Graphic overrides, model view options; API names; API switch where the
   menu stops.

## 9. Related ideas, not part of this design

Collected in the same session; each needs its own decision:

1. Folder path for views and layouts in the right column. *Decided; shares 4.5.*
2. Freely bindable trigger key instead of Space.
3. Own chords for views, layouts and layer combinations.
4. Multilingual tool names for the 43 built-in tools. *Commands are covered by 4.6.*
5. Jump to stories.
6. `?` cheatsheet cuts long lines.
7. Graphic override combinations. *Covered by G2 and 4.4.*
8. Model view options. *Covered by G2 and 4.4.*
9. API switches without a menu path: favorites (`ApplyFavoriteToToolDefault` pattern),
   renovation filter, drawing scale.
10. Picking a geometry method right after a tool (Rhino-style options).

## 10. Decisions (Dave, 2026-11-02)

- **D1. All menu commands are offered**, including ones that change the project, save or quit.
  Running one is the same as clicking it in the menu.
- **D2. One-time manual export** into `%USERPROFILE%\.baaz\commands\` is accepted.
- **D3. Disabled items are shown greyed out** and cannot be run.
- **D4. The 43 built-in tools rank above menu commands** with similar names (the tool *Wand*
  before a command containing "Wand").

## 11. Risks and unknowns, left to the build steps

- Commands that open a dialog: expected to work like *Ruler*; untested.
- Add-on commands (MDID ≠ 0): untested.
- The text-editing menu bar (context "Text") is ignored; Baaz does not open there anyway.
- Whether the live menu bar really stays the same in 3D, section and layout windows is assumed
  from the export (one "Nicht-Text" bar), not measured.
- The staleness check by file time (section 5) assumes Archicad rewrites the `.bin` when menus
  change; not tested.
- Cost of filling every submenu: not measured. If noticeable, fill per top-level menu on demand.
- Live ids across sessions and across Archicad 30: unknown, so ids are never stored, only
  looked up.
- Archicad rebuilds its menus (E6). Baaz must not keep live ids from before a rebuild.
