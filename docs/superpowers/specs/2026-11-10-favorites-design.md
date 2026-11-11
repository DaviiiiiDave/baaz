# Baaz favorites: pick a favorite by name, then place it yourself

**Status:** draft for review, 2026-11-10
**Repository:** the Baaz fork, branch `feature/favoriten`. Not yet proposed upstream.
**Archicad versions:** 29 (probe tested), 30 (target, untested)

## 1. Intent

Archicad users keep their standard building parts as favorites: a wall build-up, a door, a
stair, a piece of furniture. Today they reach them through the Favorites palette, which means
mouse, scrolling and folders. Baaz already lets them pick a tool by typing. This design lets
them pick a favorite the same way:

Tap Space, type part of the favorite's name, press Space or Enter. The matching tool is now
active with the favorite's settings, exactly as after double-clicking the favorite in the
Favorites palette. The user then places the element with their own clicks.

The user picks the favorite by typing and places the element with their own clicks. Baaz
never places anything automatically.

Favorite names often have several words ("KS + WDVS 42 cm", "T Innen 1Fl 88,5x2,01"), so typing
spaces in the search has to be dependable. That is part of this design (section 4.4).

Own shortcuts for favorites and for everything else in the search are a separate, later design
(section 8).

## 2. Goals, non-goals, anti-goals

**Goals**

- G1. Every favorite of the open project whose element type has a tool in Baaz's tool table can
  be found by its name and run from the HUD.
- G2. Running it does what a double-click in the Favorites palette does: the tool's default
  settings become the favorite's settings, and that tool becomes the active tool.
- G3. A result shows which favorites folder it comes from, so similar names can be told apart.
- G4. Shift+Space always types a space in the HUD.

**Non-goals**

- Placing elements. Baaz never creates an element; the user clicks.
- Component favorites (railing posts, handrails, stair structures, curtain wall panels, column
  segments and the like). They have no tool of their own; they are used inside the settings of
  their parent element.
- Favorites of element types Baaz has no tool for (MEP routes, structural analytical supports
  and links in the test project).
- Creating, renaming, deleting or importing favorites.
- Own shortcuts (section 8).

**Anti-goals: this would be a failure even if it technically works**

- A1. Baaz applies a different favorite than the one the user picked, or the right favorite to
  the wrong tool.
- A2. The favorite's settings are only partly applied (for example the wall build-up but not the
  classification, or the door but not its marker), so the placed element differs from one
  placed from the Favorites palette.
- A3. Opening the HUD gets noticeably slower.
- A4. Component favorites crowd the results so the real favorite cannot be reached (this
  happened in the probe with railings, see section 3).

## 3. What was verified (evidence)

A throwaway probe build of Baaz (not merged, code discarded) ran in Archicad 29 (build
5101) on 2026-11-10 with a test project, 443 favorites. The probe listed every favorite
three times, once per way of applying it, and logged each call's error code.

| # | Question | Result |
| - | -------- | ------ |
| E1 | Which way of applying a favorite works? | `ACAPI::Favorite::Favorite::GetElementDefault()` followed by `ACAPI::ElementDefault::SetAsArchicadDefault()`, then `ACAPI_Toolbox_SetToolBoxMode`. Returned `NoError` for wall, door, window, stair, curtain wall, railing, object (furniture, people, trees) and lamp. Each was placed by hand in Archicad and reported as working. A detailed comparison of classifications and properties against the Favorites palette was not recorded. |
| E2 | Alternative: Tapir's way (`ACAPI_Favorite_Get` + `ACAPI_Element_ChangeDefaults`, with and without an undo scope) | Returned `NoError` where tried (object, stair), but not compared in detail. Not needed, given E1. |
| E3 | Cost of listing all favorites with name, type and folder on every HUD open | 1 to 4 ms for 443 favorites, measured over 23 opens. |
| E4 | Does a new favorite show up without restarting? | Yes: a favorite created while Archicad was running appeared at the next HUD open. |
| E5 | What do the favorites look like? | 443 in total. 147 are component favorites: 74 railing parts, 48 stair parts (structures, treads, risers), 22 curtain wall parts (segments, panels, frames, accessories, junctions), 3 column segments. 24 are MEP or structural analytical favorites, for which the Tapir type lookup returns no type name. Folders are real and can be nested (`Stützen+Träger › Segmente`). |
| E6 | Railing search in the probe | Typing `metallmitglaspaneel` showed eight component rows ("Metall, mit Glaspaneel - 1. Handlauf", "- Innenpfosten", "- Knoten") before the railing itself. Led to decision D4. |
| E7 | Does applying a favorite add a step to the undo list? | No. Ctrl+Z right after applying undid an earlier, unrelated step. Without placing anything there is nothing to undo. |
| E8 | Running a favorite in a 3D window | The Wall tool became active with the favorite's settings. |
| E9 | Running a favorite in a layout | No crash, no error message. A wall favorite is not found there; a text favorite (`marker`) is found and offered. Observation only; why the wall favorite is missing was not investigated. |

Not verified: a detailed comparison of classifications and properties against the Favorites
palette; favorites with a tool variation; a section window; Archicad 30.

The favorite API used in E1 exists from Archicad 29 on (`@since Archicad 29` in
`ACAPI/Favorite.hpp` of the DevKit 29.3100). Baaz builds for 29 and 30 only.

## 4. How it works

### 4.1 Which favorites are listed

On every HUD open, together with views, layouts and layer combinations, Baaz reads all favorites
of the open project (`ACAPI::Favorite::GetFavoriteManager().GetAllFavorites()`). For each one it
reads the name, the element type and the parent folder.

A favorite is listed only if its element type, as the pair of type ID and variation, appears in
Baaz's tool table (`SearchEngine.cpp`, the `tools[]` table). The Arrow row (type ID 0) never
matches. That one rule removes component favorites, MEP and structural analytical favorites,
and anything a later Archicad adds without a tool. A favorite whose name or type cannot be read
is skipped. *Builder's call, set by the author: the pair, not the type ID alone, so a favorite
can never activate a sibling tool such as Window for a corner window (A1).*

All tool types in the table count, including the 2D tools (dimensions, text, label, fill, line
and the like), not only the eight probed in E1 (D4). The untested ones are covered by acceptance
(section 6).

Favorite names are unique: the DevKit documents `API_Favorite::name` as the "Unique name
identifier of the Favorite" (`APIdefs_Favorite.h`). Looking a favorite up by name at run time is
therefore safe.

### 4.2 What a result looks like

- Left: the favorite's name, unchanged.
- Right: the name of the folder that directly contains the favorite (for `Stützen+Träger ›
  Segmente` that is `Segmente`). For a favorite at the top level, `FAV`; if the folder cannot be
  read, `?`, so a failed lookup never looks like a top-level favorite.

The right column is 90 px wide (`HudWindow.cpp:429`); a full folder path does not fit, which is
why only the innermost folder name is shown.

### 4.3 Ranking

Favorites are scored like every other result. When two results have the same score, the order
is: tools, then favorites, then everything else (views, layouts, layer combinations, menu
commands), then by title as today. An exact chord such as `wa` still ranks the tool first.

### 4.4 Shift+Space

Shift+Space always types a space, also when the search is still empty. Today an empty search
closes on Shift+Space (`HudWindow.cpp:376`). Plain Space keeps its current behaviour: on an empty
search it closes the HUD, otherwise it runs the selected result, and it types a space when
nothing matches or the hint row is selected.

### 4.5 Running a favorite

1. Look the favorite up by name (`FavoriteManager::GetFavoriteByName`).
2. `GetElementDefault()`, then `SetAsArchicadDefault()` on the result.
3. Activate the tool with the favorite's own element type (type ID and variation as the favorite
   reports them), the same call Baaz uses for tools today.

If any step fails, Baaz does what it does today when a tool cannot be activated. Nothing is
created or changed in the project apart from the tool defaults.

## 5. Relation to REVIEW.md

The fork is not bound by upstream's rules (maintainer decision), but a later proposal to
upstream has to deal with them:

| Rule | Conflict | How this design handles it |
| ---- | -------- | -------------------------- |
| R14 Baaz only navigates | Applying a favorite changes Archicad's tool defaults. It does not create, change or delete elements. `tools/check_rules.py` does not flag the calls used (E1). | Fine in the fork. Upstream would need parikshit99's agreement. |
| R5 API calls in three files | Reading favorites and applying them call the API. | Reading in `NavigatorIndexer.cpp`, applying in `CommandExecutor.cpp`. The tool table stays in `SearchEngine.cpp`, free of API headers. |
| R12 Archicad terms | | "Favorite", "favorites folder", "tool". |
| R18 Every behaviour change has a test | | Listing rule, ranking and Shift+Space get native tests. Applying a favorite has no test double and is verified by hand in Archicad. |
| R20 Docs move with the code | | README: favorites in the feature list, Shift+Space row updated. |

## 6. Delivery

One pull request into the fork's `main`. It can be tested in Archicad on its own.

Acceptance in Archicad 29, by hand, before merge:

- The probe list from E1 again, now with the real build: wall, door, window, stair, curtain wall,
  railing, object, lamp; each placed element matches one placed from the Favorites palette.
- One favorite each of types not probed in E1, as far as the template has them: slab, roof,
  column, beam, zone, label, dimension, fill, text.
- For one wall and one door: classification and a property of the placed element match one
  placed from the Favorites palette (A2), and the door's marker matches.
- A favorite with a tool variation (corner window, wall end, grid element), if the template has
  one: it activates that tool, not the plain Window or Object tool.
- A railing search shows the railing, not its components.
- A favorite run in a section window: note what happens; Archicad must not crash.
- Shift+Space on an empty search types a space.

## 7. Decisions (maintainer)

- **D1. Pick and place.** Baaz selects the favorite and its tool; the user places. No automated
  placement.
- **D2. Found by name.** The user types the favorite's name.
- **D3. Shift+Space always types a space.** Plain Space keeps running the selected result.
- **D4. Only favorites with a tool of their own** are listed; component favorites are left out.
- **D5. Ranking on equal score:** tools, then favorites, then the rest.
- **D6. Favorites first, own shortcuts afterwards** as a separate design.
- **D7. Probe before design.** The way of applying (E1) was chosen from the probe, not assumed.

## 8. Related ideas, not part of this design

- **Own shortcuts for everything in the search** (tools, favorites, views, layouts, layer
  combinations, menu commands), edited in the Settings dialog. Next design. Earlier decisions
  from 2026-11-02 apply there: favorites are matched by name; own shortcuts win over Baaz's;
  a covered entry stays reachable directly below; duplicates within one layer are refused; `?`
  and everything starting with `:` stay reserved.
- Folder path for views and layouts in the right column (decided 2026-11-02, not built). The 90 px
  column limits it the same way as 4.2.

## 9. Risks and unknowns, left to the build

- Partial application (A2): if acceptance shows that a favorite is applied only in part, or a
  tool type fails, the build stops and the finding goes back to the maintainer. No second way of applying
  (E2) is built in advance. E2 would need an exception in `tools/check_rules.py`, which is
  the maintainer's decision. *Set by the author.*
- Favorites with a tool variation (corner window, wall end, grid element): the probe only met
  variation 0, so whether such favorites report their variation is unknown. With the pair rule
  (4.1) a wrongly reported variation lists the favorite under the plain tool; acceptance checks
  this.
- Section windows: the tool may not be available there. Behaviour unknown. In a layout a wall
  favorite is not offered (E9); the reason is not known.
- Archicad 30: the favorite API is the same header family, but nothing was run.
