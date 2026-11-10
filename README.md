# Space Trader 1.2.2 — Sega Genesis / Mega Drive port (v0.1.0)

A source-level port of Pieter Spronck's **Space Trader 1.2.2** (Palm OS) to the
**Sega Genesis / Mega Drive**, built with **SGDK**.

The game logic is the original C source. The Palm form handlers still run:
every Genesis screen opens the original handler and delivers the original
button IDs to it, so trading, warping, combat, the bank, the shipyard, quests
and special events follow the original rules.

## What is in the ROM

- **Original graphics**, converted from the Palm colour resources
  (`assets_src/MerchantColor.rsrc`) by `tools/convert_assets.py`:
  - all 15 ship types, each in its normal / damaged / shielded /
    shielded-and-damaged form, composed at run time like the Palm
    `ShowShip()` (hull damage and shield strength shown as a split picture);
  - the title, Retired, Utopia and Destroyed pictures (160x160);
  - the system, visited-system and wormhole chart icons;
  - the pirate / police / trader / alien / special encounter icons, the
    auto-attack indicator and the About icon.
- **Graphical Short Range Chart and Galactic Chart** with a crosshair cursor,
  fuel range circle, wormholes, tracking and Find System.
- **Original text**: all 167 alerts, 242 help / quest strings and the
  newspaper mastheads and headlines come from `assets_src/Merchant.rsrc`.
- **All three menus** (Command / Game / Help) including Options, Retire,
  High Scores, Snapshot, Switch Game and the help pages.
- **Three save slots** in SRAM: current game (written on arrival at a system
  and when the menu opens), Snapshot, and Switch Game.
- **Sound**: the Palm game contains no sound code or sound resources; the only
  sounds were the Palm OS alert beeps. The port plays PSG beeps for the same
  four alert types (information, confirmation, warning, error).

## Build

Needs SGDK (built and tested with **2.11**; only APIs that also exist in 1.70
are used) and Python 3 + Pillow if you want to regenerate the data tables.

```bash
export GDK=/opt/sgdk
make                # -> out/rom.bin
python3 tools/convert_assets.py   # optional: regenerates src/gfx_data.* and src/text_data.*
```

The source directory is now `src/` (lower case) so the project also builds on
case-sensitive file systems.

## Controls

| Where | Buttons |
|---|---|
| Lists and menus | Up/Down move, A select, B back |
| System Information | A chart, B menu, C commander status, Start newspaper, Up special, Down mercenary |
| Charts | D-pad jumps between systems, A select / track, C other chart or Find, Start menu |
| Target System | A warp, C average prices, Up cost specification, Left/Right other systems |
| Amount dialogs | Left/Right 1, Up/Down 10 (hold to accelerate), A OK, B none, C all |
| Encounter | buttons shown on the bottom line; Up pokes a tribble |
| Text entry | D-pad, A add, B delete, Start done, C cancel |

## Differences from the Palm version

- The pen is replaced by the joypad; the Shortcuts form and the
  hardware-button and rectangular-button options have no meaning here and are
  left out.
- The cheat forms (reached by typing a cheat code in Find System) are only
  partly usable: they shared one text field in this port.
- 160x160 pictures are shown at their original size; ships are shown at 2x.
- Colours are reduced to the Genesis 9-bit palette.

## Source map

| File | Role |
|---|---|
| `src/gfx.c`, `src/gfx.h` | bitmap layer (plane B), ship compositor, cursor sprite, PSG beeps |
| `src/gfx_data.c`, `src/text_data.c` | generated from the Palm resources |
| `src/ui.c`, `src/ui_extra.c` | text UI, dialogs, alerts, help viewer, keyboard, SRAM, Palm API shims |
| `src/ui_screens.c`, `src/ui_charts.c` | the Genesis screens |
| `src/palmcompat*.h` | Palm OS types and API shims |
| everything else in `src/` | original game source |

### License

The original Space Trader source code is released under the
**GNU General Public License v2** (see `License.txt`).
This port inherits that license.

SGDK and its runtime library are licensed separately; see the SGDK
repository for details.