# Baaz

A keyboard-driven command palette for Archicad 30 on Windows. Tap **Space** on
the plan to open a search bar, type, and press Space or Enter to run:

- **Tools** by two-letter chord: `wa` Wall, `dr` Door, `sl` Slab, `se` Section,
  `tx` Text… every tool in the Toolbox that the API can activate (type `?` for the list).
- **Views and sheets** from the Project Navigator, by name or number (`a101`, `gfc mas`).
- **Layer combinations**.

Holding Space still works as Archicad's Magic Wand, and so does Space + click.
The search bar follows Archicad's light or dark theme.

| Key | Action |
|---|---|
| Space (tap) | Open the search bar; tap again on an empty search to close it |
| Space (hold) | Magic Wand, as usual |
| Space / Enter | Run the selected result |
| Shift+Space | Type a space (multi-word searches) |
| ↑ / ↓ / Tab | Move the selection |
| Esc | Close |
| `?` | Cheatsheet of all chords |
| `:settings` | Settings dialog (hold time, number of results, chords on/off) |

Settings are saved in `%USERPROFILE%\.baaz\config.json`.

## Building

Requirements: Windows, Visual Studio 2022 (C++ desktop workload), CMake 3.20+,
Python 3, and the [Archicad 30 API Development Kit](https://archicadapi.graphisoft.com/).

```bash
cmake -S . -B build -DARCHICAD_DEVKIT_DIR="C:/path/to/API Development Kit 30"
cmake --build build --config Release
```

The add-on is `build/addon/Release/Baaz.apx`. In Archicad, open
**Options › Add-On Manager**, add it, and restart Archicad.

`ARCHICAD_DEVKIT_DIR` is the folder that contains `Support/Inc`; it can also come
from an environment variable of the same name. Without it, only the tests build.

### Add-on ID (MDID)

Archicad only loads add-ons whose `MDID` resource is a developer ID and add-on ID
pair issued by Graphisoft (request one from the Graphisoft developer program).
`addon/resources/RFIX/AddOnFix.grc` holds placeholders that Archicad rejects with
"This add-on cannot be validated". Replace them with your own IDs.

For local testing before you have IDs, put an `'MDID' 32500` resource in
`addon/resources/AddOnFix.local.grc` (git-ignored) and configure with
`-DBAAZ_DEV_MDID=ON`. Never distribute a build made that way.

## Tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

- `test_search_engine_native` covers chord, alphanumeric, multi-word, typo and
  Unicode search, plus speed with 1,000 items.
- `test_message_hook` drives the real Spacebar hook through a Win32 message queue,
  the way Archicad's event loop does (including `PM_NOREMOVE` peeks): taps, holds,
  Space + click, text fields, modal dialogs, app switching, and typing in the HUD.

Neither needs Archicad or the DevKit.

## Layout

```
addon/
  include/, src/   add-on sources (namespace Baaz)
  resources/       RINT/AddOn.grc (name), RFIX/AddOnFix.grc (MDID), RFIX.win/AddOnMain.rc2
tests/             native tests, built without the Archicad API
tools/             CompileResources.py (runs the DevKit's ResConv during the build)
```

## Contributing

Every change, by a person or an AI agent, follows the rules in [REVIEW.md](REVIEW.md);
[CLAUDE.md](CLAUDE.md) (and [AGENTS.md](AGENTS.md) for other agents) explains how to work
here. CI enforces what it can: the rule checks in `tools/check_rules.py`, function
complexity, warning-free `/W4` builds, the tests, and CodeQL security analysis.

## Licensing

Baaz is source-available under the [MIT License with the Commons Clause](LICENSE).

- **Free to use, copy and change**, including at work in a firm and for paid
  client projects.
- **Not for sale.** Nobody may sell Baaz, or sell a product or service whose
  value comes entirely or substantially from it (including paid hosting,
  consulting or support for it), without separate permission from the licensor.
  For that, contact [@parikshit99](https://github.com/parikshit99) on GitHub.

## Credits

`tools/CompileResources.py` comes from
[Tapir](https://github.com/ENZYME-APD/tapir-archicad-automation)
(MIT License, see `tools/LICENSE-tapir.txt`).
