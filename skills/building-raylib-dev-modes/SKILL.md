---
name: building-raylib-dev-modes
description: Use when a raylib game needs a developer mode or tool screen - a sound board or sound test, an asset, model, enemy or prop viewer or gallery, a sandbox or test arena launched by a flag - or when adding a dev-*.sh launcher script, a --sounds / --assets / --viewer style flag, per-item screenshots or a contact sheet, or when asked which sounds or assets a raylib game is missing.
---

# Dev modes for a raylib game with kit_dev.h

`kit_dev.h` (https://github.com/binRick/raylib-dev-kit) is a single header. The game registers
what it has; the header provides the screens, the `--dev` command line, the help listing and
per-item screenshots. Register with it. Do not add another one-off flag, launcher script or
hand-typed list of names.

## Which registration

| Need | Call |
|---|---|
| List every sound with its trigger; find missing ones | `kit_dev_sounds(table, n)` |
| Look at model files (.glb .obj ...) one at a time, with counts | `kit_dev_gallery(mode, about, items, n, NULL, NULL, 0)` |
| Look at things the game draws itself, in different states | `kit_dev_gallery(mode, about, items, n, draw_fn, keys, nkeys)` |
| A playable sandbox (empty arena, god mode, one enemy type) | `kit_dev_custom(mode, about, keys, nkeys)` + `if (kit_dev_is(mode))` in the game |

## Wiring (once per game)

1. One `.c` file: `#define KIT_DEV_IMPLEMENTATION` before `#include "kit_dev.h"`.
2. Register every mode, then `kit_dev_init(argc, argv)`, all BEFORE `InitWindow` (so `--dev`
   and bad names exit without opening a window).
3. After `InitWindow` and `InitAudioDevice`: `if (kit_dev_run()) { CloseWindow(); return 0; }`.
   On browser/iOS frame-callback builds use `if (kit_dev_step()) return;` at the top of the
   frame function instead.
4. Copy `dev.sh` beside `run.sh` and set its `BIN=`.

## Writing the sound table

One row per MOMENT, found by reading the game code for events (jump, hit, pickup, door, menu
move, win, lose), not by listing the files in the assets folder. Then for each moment:

- has its own sound: `KIT_DEV_OK`
- nothing plays: `file = NULL`, `KIT_DEV_MISSING`
- plays a sound that also belongs to another moment: `KIT_DEV_SHARED`, and say whose in `note`
- plays but is wrong (too long, clicks, too quiet): `KIT_DEV_IMPROVE` with the reason in `note`

`trigger` is the moment in plain words ("pushing a locked door"), not the function name.

## Draw functions

- Called inside `BeginMode3D`, item at the origin standing on y = 0, about 2.5 units tall fills
  the stage. Do not call `BeginDrawing`, `ClearBackground` or `BeginMode3D` in it.
- Read `stage->item`, `stage->state` (1-9, the last number key) and `stage->time`.
- Document each state in the `KitDevKey` list; that list is the help text.
- Models you create need a lit shader to read as shapes: `kit_dev_shader()`, or the game's own.

## Looking at assets yourself

```bash
./dev.sh                               # what modes and items exist
./dev.sh props --state 3 --shots /tmp/shots    # one PNG per item, then quits
```

Then read the PNGs. Use this to verify an asset or drawing change instead of asserting it
looks right. The run's tally line counts warnings: a missing sound or model file shows there.
Needs a real display session (a locked or headless Mac cannot create the GL context).

## Common mistakes

- Calling `kit_dev_init` after `InitWindow`: `--dev` then flashes a window before printing.
- Keeping the old `--sounds` flag and `dev-sounds.sh` alongside the new mode. Remove them.
- A sound table generated from the asset folder: it can never show a missing sound.
- Asset paths that only resolve from one working directory. Resolve them the way the game does
  (after its own `ChangeDirectory`), since the board marks unreadable files MISSING.
- Forgetting `ClearBackground` in the game's own frame when drawing a gradient or skybox
  instead: the depth buffer is never cleared and 3D objects vanish behind last frame's.
