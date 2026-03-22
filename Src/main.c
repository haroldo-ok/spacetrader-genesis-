/**
 * main.c  –  Space Trader 1.2.0 for Sega Genesis (SGDK 1.70)
 *
 * This is the ROM entry point.  It initialises the Genesis hardware via
 * SGDK, sets up the UI engine, then delegates to the ported game logic via
 * the three lifecycle functions exposed by Merchant.c.
 *
 * Joypad mapping:
 *   D-Pad          : navigate menus / scroll lists
 *   A              : confirm / select
 *   B              : back / cancel
 *   C              : info / alternate action
 *   Start          : options shortcut
 *
 * Save game:  battery-backed SRAM at cartridge address 0x200000.
 *             Layout:  [2 bytes magic][sizeof(SAVEGAMETYPE) bytes game data]
 *                      [2 bytes hs-magic][MAXHIGHSCORE * sizeof(HIGHSCORE)]
 */

#include "genesis.h"
#include "external.h"   /* all game types + global externs */
#include "ui.h"         /* Genesis UI engine */

/* -----------------------------------------------------------------------
 * Forward declarations of the three lifecycle functions in Merchant.c
 * --------------------------------------------------------------------- */
extern Err  GenAppStart(void);
extern void GenAppLoop(void);
extern void GenAppStop(void);

/* -----------------------------------------------------------------------
 * Forward declaration for the new-game commander-name screen.
 * In the Palm version this was NewCommanderFormHandleEvent.
 * We expose a thin Genesis wrapper that does the letter-picker before
 * calling StartNewGame() from Traveler.c.
 * --------------------------------------------------------------------- */
/* StartNewGame declared in Prototype.h as Boolean StartNewGame() */

/* -----------------------------------------------------------------------
 * Splash / title screen
 * --------------------------------------------------------------------- */
static void show_splash(void)
{
    ui_clear_screen();

    ui_printf(8,  4, PAL_TITLE,  "  ___  ___  ___   ___ ___    ");
    ui_printf(8,  5, PAL_TITLE,  " / __|| _ \\/ _ \\ / __| __|   ");
    ui_printf(8,  6, PAL_TITLE,  " \\__ \\|  _/ (_) | (__| _|    ");
    ui_printf(8,  7, PAL_TITLE,  " |___/|_|  \\__\\_\\\\___|___|   ");
    ui_printf(6,  9, PAL_HILIGHT,"  T R A D E R  1 . 2 . 0     ");
    ui_printf(0, 11, PAL_NORMAL, "  Sega Genesis Port           ");
    ui_printf(0, 12, PAL_NORMAL, "  Original by Pieter Spronck  ");
    ui_printf(0, 13, PAL_DIM,    "  For those familiar with     ");
    ui_printf(0, 14, PAL_DIM,    "  Elite: many ideas here are  ");
    ui_printf(0, 15, PAL_DIM,    "  heavily inspired by it.     ");

    ui_printf(0, 24, PAL_STATUS, "Press A to start              ");
    ui_printf(0, 25, PAL_DIM,    "  A=New Game  B=Load Save     ");

    /* Wait for A or B */
    kprintf("show_splash: waiting for input");
    for (;;)
    {
        ui_vsync();
        if (ui_joy_pressed & BTN_A) { kprintf("show_splash: A pressed"); return; }
        if (ui_joy_pressed & BTN_B) { kprintf("show_splash: B pressed"); return; }
        if (ui_joy_pressed & BTN_START) { kprintf("show_splash: Start pressed"); return; }
    }
}

/* -----------------------------------------------------------------------
 * Commander-name entry screen (replaces Palm's NewCommanderForm)
 * Shown when there is no save, or user chose New Game.
 * --------------------------------------------------------------------- */
/* Preset commander names the player can cycle through */
static const char* _preset_names[] = {
    "Jameson", "Shelby", "Morgan", "Blake", "Quinn",
    "Reyes", "Hayashi", "Novak", "Cruz", "Okafor"
};
#define NUM_PRESET_NAMES 10

static void commander_name_screen(void)
{
    int name_idx = 0;

    kprintf("commander_name_screen: entry");
    strncpy(NameCommander, _preset_names[name_idx], NAMELEN);
    NameCommander[NAMELEN] = '\0';

    ui_clear_screen();
    ui_title("NEW COMMANDER");
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,  "Choose your commander name:");
    ui_printf(0, UI_BODY_TOP + 3, PAL_HILIGHT, "> %-20s", NameCommander);
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL,  "LR = cycle names");
    ui_status("A=Accept  LR=Next/Prev name");
    kprintf("commander_name_screen: screen drawn");

    for (;;)
    {
        ui_vsync();

        if (ui_joy_pressed & BTN_A)
        {
            kprintf("commander_name_screen: accepted name=%s", NameCommander);
            return;
        }
        if (ui_joy_pressed & BTN_RIGHT)
        {
            name_idx = (name_idx + 1) % NUM_PRESET_NAMES;
            strncpy(NameCommander, _preset_names[name_idx], NAMELEN);
            NameCommander[NAMELEN] = '\0';
            ui_printf(0, UI_BODY_TOP + 3, PAL_HILIGHT, "> %-20s", NameCommander);
        }
        if (ui_joy_pressed & BTN_LEFT)
        {
            name_idx = (name_idx + NUM_PRESET_NAMES - 1) % NUM_PRESET_NAMES;
            strncpy(NameCommander, _preset_names[name_idx], NAMELEN);
            NameCommander[NAMELEN] = '\0';
            ui_printf(0, UI_BODY_TOP + 3, PAL_HILIGHT, "> %-20s", NameCommander);
        }
    }
}

/* -----------------------------------------------------------------------
 * Difficulty selection (replaces Palm's NewCommanderForm sliders)
 * --------------------------------------------------------------------- */
static const char* _diff_names[] = {
    "Beginner",
    "Easy",
    "Normal",
    "Hard",
    "Impossible"
};

static void difficulty_screen(void)
{
    kprintf("difficulty_screen: entry");
    ui_clear_screen();
    ui_title("SELECT DIFFICULTY");
    ui_printf(0, UI_BODY_TOP, PAL_NORMAL,
              "Choose your difficulty level:");

    UIList lst;
    ui_list_init(&lst, _diff_names, MAXDIFFICULTY,
                 UI_BODY_TOP + 2, 6, 4, 24);
    lst.cursor = NORMAL;  /* default = Normal */
    ui_list_draw(&lst);
    ui_status("UP/DOWN=select  A=confirm");

    for (;;)
    {
        ui_vsync();
        int sel = ui_list_update(&lst);
        if (sel >= 0)
        {
            Difficulty = (Byte)sel;
            kprintf("difficulty_screen: selected %d", (int)sel);
            return;
        }
        /* B = default Normal */
        if (ui_joy_pressed & BTN_B)
        {
            Difficulty = NORMAL;
            kprintf("difficulty_screen: defaulted to NORMAL");
            return;
        }
    }
}

/* -----------------------------------------------------------------------
 * Main entry point (SGDK calls int main())
 * --------------------------------------------------------------------- */
/* SGDK 1.70 expects main() to return bool */
/* SGDK 1.70: main() returns bool (u16). Use u16 to match LTO signature. */
u16 main(void)
{
    /* ── Hardware setup ── */
    VDP_setScreenWidth320();
    kprintf("main: start");

    /* ── UI engine init ── */
    ui_init();
    kprintf("main: ui_init done");
    kprintf("main: ui_init done");

    /* Enable SGDK SRAM support */
    SRAM_enable();
    SRAM_disable();   /* immediately disable; sram_* functions re-enable as needed */

    /* ── Splash screen ── */
    show_splash();
    kprintf("main: splash done");

    kprintf("main: splash done, checking save");
    /* ── Determine launch mode ── */
    Boolean has_save = sram_has_save();
    kprintf("main: has_save=%d", (int)has_save);
    Boolean new_game = false;

    if (has_save)
    {
        /* Ask: continue or new game? */
        ui_clear_screen();
        ui_title("SPACE TRADER");
        ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,
                  "A saved game was found.");
        ui_printf(0, UI_BODY_TOP + 3, PAL_HILIGHT,
                  "A = Continue saved game");
        ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL,
                  "B = Start new game");
        ui_status("A=Continue  B=New Game");

        for (;;)
        {
            ui_vsync();
            if (ui_joy_pressed & BTN_A) { new_game = false; break; }
            if (ui_joy_pressed & BTN_B) { new_game = true;  break; }
        }
    }
    else
    {
        new_game = true;
    }

    /* ── New game: name + difficulty first ── */
    if (new_game)
    {
        kprintf("main: starting new game flow");
        commander_name_screen();
        kprintf("main: name=%s", NameCommander);
        difficulty_screen();
        kprintf("main: difficulty=%d", (int)Difficulty);
        /* Zero the save so AppStart doesn't find a stale one */
        SRAM_enable();
        uint16_t zero = 0;
        SRAM_writeBuffer(&zero, 0, sizeof(zero));
        SRAM_disable();
    }

    /* ── Initialise game state (loads save or sets up defaults) ── */
    kprintf("main: calling GenAppStart");
    kprintf("main: calling GenAppStart");
    Err err = GenAppStart();
    kprintf("main: GenAppStart returned %d, CurForm=%d", (int)err, (int)CurForm);
    kprintf("main: GenAppStart done, CurForm=%d err=%d", CurForm, (int)err);
    if (err)
    {
        ui_clear_screen();
        ui_printf(0, 14, PAL_WARN, "Startup error! Halting.");
        for (;;) ui_vsync();
    }

    /* If new game, start fresh (replaces Palm's NewCommanderFormHandleEvent) */
    if (new_game)
    {
        kprintf("main: calling StartNewGame, name=%s", NameCommander);
        StartNewGame();
        kprintf("main: StartNewGame done, CurForm=%d", CurForm);
    }

    kprintf("main: entering GenAppLoop, CurForm=%d", CurForm);
    kprintf("main: entering GenAppLoop, CurForm=%d", (int)CurForm);
    /* ── Main game loop ── */
    GenAppLoop();
    kprintf("main: GenAppLoop returned");
    kprintf("main: GenAppLoop exited");

    /* ── Shutdown: save state ── */
    GenAppStop();

    /* Halt */
    for (;;) ui_vsync();
    return 0;
}

