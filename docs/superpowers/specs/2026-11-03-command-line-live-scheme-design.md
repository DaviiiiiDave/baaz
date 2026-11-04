# Command line: read the live command scheme (design addendum)

Addendum to `2026-11-02-command-line-design.md` ("the base spec"). It replaces two decisions of
the base spec and adds the user-facing hints that were missing. Everything not mentioned here
stays as the base spec and its decision D5 (top-level menus matched by unique title) describe.

| Base spec | Replaced by |
|---|---|
| 4.1 last paragraph: "the binary format is undocumented, so Baaz reads only the XML export" | Section 3 below: the menu **structure** comes from the scheme `.bin`, the **names** from an export |
| 5 "Staleness" (export older than any scheme `.bin` = stale) | Removed. Section 2 below explains why it cannot work |
| 5 export choice ("matching beats newest; a tie offers nothing") | Section 4 below: newest fitting export wins |

## 1. Intent

The user adapts Archicad's menus to their work all the time; that is normal use, not an
exception (Dave, 2026-11-03). Baaz must keep working through that. It must never silently lose
its commands, and it must never run a wrong command (A1 of the base spec stays absolute). When
Baaz cannot offer commands, it always says so and says what to do, in a short guide.

## 2. Why the current design fails in practice (measured 2026-11-03, Archicad 29)

1. **The time rule can never pass.** Archicad writes the active profile's `.bin` files (one per
   component: commands, shortcuts, workspaces, extras, office standards, preferences) together
   when the user closes *Options › Work Environment* with OK, even without any change (one
   measurement: export 14:02:20, those six files rewritten 14:04:24.932 with no change in
   between; the inactive standard scheme `Standardbefehle 29.bin` was not touched and kept its
   date of 2025-12-25). The export itself happens inside that dialog, so every export is older
   than the `.bin` written at the following OK. Three earlier "gaps" (1.59 s, 2.35 s, ~4 s) were
   the user's time between *Export* and *OK*. A tolerance in seconds cannot fix this.
2. Closing and restarting Archicad does **not** rewrite the scheme `.bin` (13:25:33 unchanged
   across a restart at 13:33).
3. A fresh export copied the normal way (file time preserved) was not offered, before and after
   a restart. With a copy that got a new file time, everything worked: *Lineal* from the floor
   plan, a command from a submenu, and after a menu change nothing was offered.
4. An Archicad profile export writes into sub-folders with localised, generated names
   (`Exportiertes Profil\Befehle\Befehle_<random>.xml`). A second export into the same folder
   adds a file instead of replacing it.

## 3. The decided design (Dave, 2026-11-03: "mitlesen")

Baaz reads the menu structure from the active command scheme `.bin` that Archicad itself keeps
current, and takes only the item **names** from an export.

- **Structure from the `.bin`.** The scheme `.bin` is a serialisation of the same tree as the
  XML export, without the localised item names. Observed (AC29 and AC30, read-only):
  - not compressed (about 3.4 bit/byte, no zlib stream); same 8-byte header
    `57 57 78 56 34 12 05 00` in all three inspected files (AC29 office scheme, AC29 standard
    scheme, AC30 standard scheme);
  - the scheme GUID as ASCII after the header (for example `2F4881B8-…` = the scheme the
    user's profile references), the scheme name and the menu titles as UTF-16; the titles are
    localised in the language Archicad runs in (German *Ablage, Bearbeiten, Ansicht, Planung,
    Dokumentation, Optionen, Fenster, Hilfe, Teamwork* found in all three files, standard and
    custom, AC29 and AC30; other UI languages not inspected), so D5 title matching works on the
    `.bin` alone, without an export;
  - records tagged in UTF-16 (`Item`, `Separator`, `Popup`, `Menu`, `MenuBar`); an `Item`
    record carries the command number as u32 little-endian followed by the two MDID parts
    (u32 each); most `Item` records are 29 bytes apart (others 55 and 39), identical in AC29 and
    AC30;
  - the items of a menu appear in menu order (View: 21761, 1540, 1818, … at offsets 24504,
    24533, 24562); submenus are defined as separate blocks, like the XML's `IDREF` menus;
  - item names are not stored (`Lineal` occurs 0 times).
  The reader produces the same `MenuNode` tree that `SchemeReader` produces from XML, with the
  full command id (`number-MDID1-MDID2`) on every item and no name. Unnamed items keep their
  place in the tree, because positions must stay right.
- **Names from an export.** Baaz attaches names by full command id from the export described in
  section 4. The id includes the MDID parts, so add-on commands with the same number no longer
  collide (this also separates the ambiguous numbers 1027, 1293, 20754 noted in Task 2, where
  the MDID differs). An item whose id has no name in the export is **not offered**; it still
  counts for positions.
- **Items of add-ons that are not loaded are removed first** (measured 2026-11-03, Archicad 29,
  Dave's office scheme; decided by Dave, option A). The `.bin` keeps the configured layout,
  including commands of add-ons that are not loaded. The live menu and the export leave those
  commands out, and also a submenu that has nothing left: the add-on menu *CI Tools* has 15
  submenus in the `.bin` and 2 in the live menu and in the export; the 13 hidden ones hold only
  commands whose MDID pair occurs nowhere in the export's floor-plan bar. Before linking, Baaz
  therefore removes from the `.bin` tree every `Item` with a non-zero MDID whose pair
  (MDID1, MDID2) occurs in no `Item` of the chosen export's floor-plan bar, and then every popup
  that had an `Item` or popup before this removal and has none left. Built-in commands (MDID 0)
  are never removed; popups that were empty already stay. With this rule the office `.bin`
  equals its export in both bars; the standard scheme is unchanged. Accepted cost: an add-on
  installed after the export is removed as well; if Archicad shows its commands, positions in
  that top-level menu no longer fit, so that menu is not linked until the next export (nothing
  wrong is run), and those commands are not counted as new.
- **Which `.bin` is the active scheme: only the one the active profile names** (decided by
  Dave 2026-11-04, option A; replaces an earlier choice by fit score and write time, whose
  fallbacks could pick an inactive scheme such as the standard one and link wrong names).
  Established in Task 3a: the work environment in effect is a folder next to the one that holds
  `ProfileSet.xml` (German: `Zuletzt verw. Arbeitsumgebung` next to `Arbeitsumgebung`; seen in
  Archicad 27, 29 and 30). Its `Profile\` folder holds one profile XML whose `SchemeRef` with
  `componentType="6FCD168F-A26C-4547-AF9D-ED9A5FC54DD7"` names the active command scheme GUID.
  A command scheme the user changed without saving it as a named scheme is stored in that same
  folder (`Befehle\Befehle.bin`, unnamed) and is then the one named. `ProfileSet.xml` holds only
  defaults. Baaz finds the profile without localized folder names: under the running Archicad's
  `%APPDATA%\GRAPHISOFT\Archicad <major>…` folder, the profile XML files in a `<dir>\Profile\`
  folder whose `<dir>` holds no `ProfileSet.xml`. The scheme is the `.bin` (searched as today,
  up to three levels deep, recognised by the marker) whose header GUID equals the named GUID.
  **No commands are offered** (third hint) when there is not exactly one such profile file, not
  exactly one command `SchemeRef` in it, no readable `.bin` with that GUID, or that `.bin` fits
  no top-level menu of the live bar or links no command. There is no fallback to other `.bin`
  files.
- **No staleness rule.** The structure is always the current one, so a menu change no longer
  needs a new export. The base spec's run-time protection stays: before running, Baaz fills the
  live path again and requires an enabled `Item` with the same live id (base spec 4.3).
- **Re-read without restart.** The `.bin` and the exports are re-read when their write time or
  size changes, checked on every HUD open (as the export cache does today).

## 4. Where the names come from

- The user exports the whole profile from *Options › Work Environment* into
  `%USERPROFILE%\.baaz\commands\` (D2 of the base spec, folder unchanged). Sub-folders are
  allowed and their names do not matter.
- Baaz searches `.baaz\commands\` recursively for scheme XML files, recognised by the command
  scheme marker (`componentType="6FCD168F-A26C-4547-AF9D-ED9A5FC54DD7"` on the `<Scheme>` root),
  not by file or folder name.
- Of the exports whose top-level titles fit the live bar (D5), the **newest** is used (Dave,
  2026-11-03). Old exports never need cleaning up. *(My call:)* only one export is used, not a
  merge of several, so names never come from two languages.

## 5. What the user sees: one hint line with a short guide

When Baaz cannot offer every command, the HUD shows one line at the **top**, as soon as the HUD
opens, independent of what is typed, followed by a short guide (Dave, 2026-11-03: "Hinweis und
dann eine Mini-Anleitung"). Views, tools and everything else stay searchable as usual.

| State | Hint (wording left to the builder, English like the other HUD texts) |
|---|---|
| No export found | Commands need one export. Guide: Enter opens Work Environment → export the profile to `…\.baaz\commands` → open the HUD again |
| Some items have no name (added to the menus after the export) | *N* commands are new. Same guide |
| The active profile or its `.bin` cannot be found or read (unknown format, for example after an Archicad update), or that `.bin` fits no menu | Command list cannot be read for this Archicad version. No commands are offered |

- *(My call)* "*N* commands are new" counts only items that link to a live item, are not a
  placeholder (dynamic lists, base spec 4.4) and have no name in the export. Right after a fresh
  export of the current state, *N* is therefore 0; commands of add-ons the export does not
  contain are removed before linking (section 3) and are not counted.
- *(My call)* The hint row is never the default selection, takes no slot from the result count,
  is reachable with the arrow keys and runs on Enter or click only, not on Space. Typing
  `lineal` and pressing Enter must keep running *Lineal*.
- **Enter on the hint opens *Options › Work Environment*.** Baaz runs that menu command like any
  other. Its id is `7682-0x00000000-0x00000000` in the inspected export; because the structure
  comes from the `.bin`, Baaz knows its live position even before any export exists. *(My
  call)* If it cannot be found, the guide names the menu path instead.
- The export itself and the choice of target folder stay manual: the AC29 DevKit has no API for
  work-environment schemes (searched `Support/Inc`; only unrelated "scheme" symbols), and the
  base spec section 5 says the same.
- Base spec carry-overs that this line also covers: a command that was refused at run time
  shows why (base spec 4.3 "the HUD says so"), and the one-time "cannot check" note of Task 3 is
  no longer needed (no staleness check).

## 6. Goals, non-goals, anti-goals

- **Goals:** menu changes keep working without a new export; one export is enough until new
  commands are added; every state without commands is explained in the HUD with a guide; works
  in Archicad 29 and 30.
- **Non-goals:** exporting automatically; writing anything (Baaz still writes nothing, R16);
  reading the `.bin` of other components (shortcuts, toolbars); the "Text" menu bar.
- **Anti-goals:** running a command whose position or name is in doubt (A1); guessing a name
  for an unnamed item; partial trust in a `.bin` the reader does not fully understand.

## 7. Constraints

- The `.bin` reader is hand-written and strict, like `SchemeReader`: it knows every record type
  it accepts, checks every length and count against the file size, has a node budget, and
  returns "unreadable" (not a partial tree) on anything unexpected, including an unknown header.
  Unreadable means: no commands, and the third hint.
- Read-only access to `%APPDATA%\GRAPHISOFT\…` and `.baaz\commands\`; same size caps as today
  (8 MB per file); no file I/O in the hook path (R15); no new dependency (R11); `ACAPI_` only in
  the three files of R5.
- Test data (Dave, 2026-11-03, option C): **no Graphisoft `.bin` file goes into the public
  repo.** The automated tests build synthetic `.bin` files in test code. In addition, one local
  test compares the reader against the real files on the developer's machine (a `.bin` and the
  XML export of the same state, AC29 and AC30); it is skipped, and says so, when those files are
  not present (CI, other machines). Personal or office schemes are used only in that local test
  and in manual checks, never committed.
- The undocumented format is a point for the upstream maintainer (in addition to R14/R17 noted
  in the Task 3 review); the PR text must say so.

## 8. Left to the builder (verify, then decide)

- The exact `.bin` grammar: what the 55- and 39-byte records are, how submenus are referenced,
  how both menu bars and the toolbars are laid out, how `dynamic` items are marked. Method: parse
  a `.bin` and the XML export of the **same state** (no OK between them) and require the two trees
  to be identical (ids, order, separators, popups, titles); keep that as the local test of
  section 9.
- *(Resolved in Task 3a, see section 3:)* what `Zuletzt verw. Arbeitsumgebung\Befehle\Befehle.bin`
  represents (an unsaved command scheme of the profile in effect) and which profile is active
  (the one in that folder, not `ProfileSet.xml`).
- Whether the live menu changes at the same moment as the `.bin` (at OK). If not, the window in
  between is covered by the fit score and the run-time re-check, and must be described.
- How the existing Task 3 code is reused: `LiveMenu`, `Run`, title matching (D5) and the export
  loading stay; `CheckFreshness`/`IsExportStale` and the "cannot check" note go.
- Splitting this into plan tasks and stages.

## 9. Checks

- Automated (synthetic `.bin` built in test code): reader produces the expected tree;
  truncated file, unknown record, wrong header, oversize file each give "unreadable"; names
  attached by full id (same number, different MDID); unnamed item keeps positions and is not
  offered; items of add-ons absent from the export are removed, popups emptied by that are
  removed, built-in items and already-empty popups stay; newest fitting export wins; only the
  `.bin` the active profile names is used (missing, ambiguous or unreadable profile or `.bin`,
  or no fit → no commands, never another `.bin`); each of the three hint states.
- Local, skipped when the files are missing: the real AC29 and AC30 `.bin` give the same tree as
  the XML export of the same state. Needs one AC30 profile export taken without pressing OK
  afterwards (close the dialog with Cancel); open with Dave.
- Manual, Archicad 29 and 30: change menus without exporting → the moved command still runs from
  its new place; add a command that the export does not know → "N commands are new"; no export →
  hint, Enter opens Work Environment.
- After the first Archicad 30 update: repeat the manual check and compare the `.bin` header and
  record layout (Dave, 2026-11-03: test it at the first update).
