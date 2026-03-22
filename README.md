# Space Trader 1.2.0 — Sega Genesis Port
## Build & Architecture Notes

---

### Overview

This is a complete source-level port of Pieter Spronck's **Space Trader 1.2.0**
(Palm OS, 2002) to the **Sega Genesis / Mega Drive** using **SGDK 1.70**.

The porting process was mostly vibe-coded using the free version of Claude.

The game logic (trading economy, encounters, ships, combat, quests, galaxy map)
is preserved verbatim from the original C source.  The Palm OS UI toolkit,
event system, database API, and bitmap resources are replaced by a Genesis-native
text-mode interface built on SGDK's VDP tile engine.

---

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| SGDK | 1.70 | Install to `/opt/sgdk` or set `GDK=` |
| m68k-elf-gcc | ≥ 9.x | Ships with SGDK |
| GNU Make | any | |

---

### Build

```bash
cd spacetrader-genesis
make GDK=/opt/sgdk
# Output: out/rom.bin  (~256 KB padded to 1 MB)
```

If your SGDK uses the standard `makefile.gen` include approach:

```bash
make sgdk GDK=/opt/sgdk
```

---

### Controls (Joypad)

| Button | Action |
|---|---|
| D-Up / D-Down | Move cursor / scroll list |
| D-Left / D-Right | Change quantity / secondary navigation |
| A | Confirm / Select / OK |
| B | Back / Cancel |
| C | Info / Alternative action |
| Start | Options shortcut |

---

### Save Game

The port uses **battery-backed SRAM** (standard on most flash carts and
some original cartridges).  The SRAM layout is:

```
Offset 0x0000: [2 bytes]  Game save magic (0xC0DE)
Offset 0x0002: [~700 bytes]  SAVEGAMETYPE struct (full game state)
Offset 0x02BE: [2 bytes]  High-score magic (0xBEEF)
Offset 0x02C0: [3 × 28 bytes]  HIGHSCORE[3] table
```

Total SRAM used: ~900 bytes.  Carts with 8 KB SRAM (the most common size)
have ample room.

If no battery backup is available the game still runs; it simply cannot
persist between sessions.

---

### Architecture

#### File map

| File(s) | Role |
|---|---|
| `src/palmcompat.h` | Complete Palm OS → C99 type and API shim |
| `src/palmcompat_extra.h` | Additional Palm types (SWord, Handle, FtrGet, etc.) |
| `src/external.h` | Rewritten global-variable declaration header |
| `src/ui.h` / `src/ui.c` | Genesis UI engine: VDP text rendering, widgets, SRAM, all Palm API stubs |
| `src/main.c` | SGDK `int main()` entry point; splash, name entry, delegates to game |
| `src/Field.c` | Replaced: field widget → ui_field_buf text storage |
| `src/Merchant.c` | Patched: SaveGame/LoadGame→SRAM, AppEventLoop→SGDK, Palm entry removed |
| `src/Traveler.c` | Patched: OpenDatabase/SaveStatus→SRAM |
| `src/Global.c` | Patched: Palm includes removed |
| `src/*.c` (all others) | Unmodified game logic; compile cleanly via palmcompat.h |

#### UI engine

The `ui_*` functions provide a 40×28 character tile display on the Genesis
VDP Plane B (8×8 pixel tiles at 320×224).

**Screen state machine**: the Palm OS `FrmGotoForm(id)` macro sets
`ui_current_form = id`, which causes `GEN_EvtGetEvent` to inject a
`frmOpenEvent` on the next call.  The original `AppHandleEvent` dispatches
this to the appropriate form handler, which calls `FrmDrawForm` (no-op)
and then populates the screen via the `WinDrawChars` / `setLabelText` /
`EraseRectangle` etc. stubs — all of which ultimately write to the tile
plane via `ui_print`.

**Modal dialogs** (`FrmDoDialog`): For quantity-entry forms
(buy/sell/plunder amounts), `GEN_FrmDoDialog` presents a number spinner
(Left/Right to change value, A to confirm).  For yes/no forms it shows a
two-line message box.  The return value maps to the original Palm button-ID
constant expected by the calling code.

**Galactic chart**: `ui_draw_galaxy_map()` renders all 120 solar systems
as dot characters scaled from the 150×110 coordinate space to the 38×22
tile body area.  Visited systems use `*`, unvisited use `.`.

---

### Known Limitations

1. **No graphical ship art**: The Palm bitmap resources (`.rsrc` files) are
   Palm-proprietary format and cannot be compiled for Genesis.  Ship images
   in the encounter screen are omitted; the encounter text is displayed
   instead.  Adding tile graphics is straightforward via `res/*.res` SGDK
   resource files.

2. **Single save slot**: The Palm version supported two save slots (main +
   "switch game").  The Genesis port uses one SRAM slot.  The `SaveGame(2)` /
   `LoadGame(2)` switch-game path returns `false`, which the game handles
   gracefully.

3. **Commander name entry**: Uses a letter-picker grid (A-Z, a-z, 0-9,
   special chars).  Navigate with D-pad, A=add letter, B=backspace, C=done.

4. **No Graffiti / text input**: All text entry is via the letter-picker or
   spinner widgets.  Quantity fields that originally accepted keyboard input
   now use the spinner.

5. **News/newspaper screen**: The scrolling newspaper text works correctly
   since it uses `DrawChars`/`DisplayPage` which route through `WinDrawChars`
   to our tile renderer.

---

### Porting decisions

- **Palm `Boolean`** → `uint8_t` (0/false, 1/true).  The original code
  sometimes assigns integer results (0/1) directly, which remains valid.

- **`MemHandleNew` / `MemHandleFree`** → bump allocator in an 8 KB static
  pool.  The game allocates handles during form display and frees them
  immediately; the bump allocator never overflows within a session.

- **`FrmDoDialog` return values**: The Palm version returned the resource ID
  of the button the user pressed.  Our stub returns the correct button-ID
  constant for each known form (BuyItemYesButton, GetLoanEverythingButton,
  etc.) or a generic `formID+1` / `formID+2` pair for unknown forms.

- **`evtWaitForever`** (-1) passed to `GEN_EvtGetEvent` causes the function
  to block at `ui_vsync()` until a joypad event occurs, matching the
  original behaviour.

---

### License

The original Space Trader source code is released under the
**GNU General Public License v2** (see `License.txt`).
This port inherits that license.

SGDK and its runtime library are licensed separately; see the SGDK
repository for details.
