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
    for (;;)
    {
        ui_vsync();
        if (ui_joy_pressed & BTN_A) return;
        if (ui_joy_pressed & BTN_B) return;
        if (ui_joy_pressed & BTN_START) return;
    }
}

/* -----------------------------------------------------------------------
 * Commander-name entry screen (replaces Palm's NewCommanderForm)
 * Shown when there is no save, or user chose New Game.
 * --------------------------------------------------------------------- */
static void commander_name_screen(void)
{
    ui_clear_screen();
    ui_title("NEW COMMANDER");
    ui_printf(0, UI_BODY_TOP, PAL_NORMAL,
              "Choose your name, pilot:");

    UITextInput ti;
    ui_textinput_init(&ti, NAMELEN, UI_BODY_TOP + 2, UI_BODY_TOP + 1);
    ui_textinput_draw(&ti);

    for (;;)
    {
        ui_vsync();
        if (ui_textinput_update(&ti))
        {
            /* Copy into global NameCommander */
            strncpy(NameCommander, ti.buf, NAMELEN);
            NameCommander[NAMELEN] = '\0';
            return;
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
            return;
        }
        /* B = default Normal */
        if (ui_joy_pressed & BTN_B)
        {
            Difficulty = NORMAL;
            return;
        }
    }
}

/* -----------------------------------------------------------------------
 * Main entry point (SGDK calls int main())
 * --------------------------------------------------------------------- */
/* SGDK 1.70 expects main() to return bool */
bool main(void)
{
    /* ── Hardware setup ── */
    VDP_setScreenWidth320();

    /* ── UI engine init ── */
    ui_init();

    /* Enable SGDK SRAM support */
    SRAM_enable();
    SRAM_disable();   /* immediately disable; sram_* functions re-enable as needed */

    /* ── Splash screen ── */
    show_splash();

    /* ── Determine launch mode ── */
    Boolean has_save = sram_has_save();
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
        commander_name_screen();
        difficulty_screen();
        /* Zero the save so AppStart doesn't find a stale one */
        SRAM_enable();
        uint16_t zero = 0;
        SRAM_writeBuffer(&zero, 0, sizeof(zero));
        SRAM_disable();
    }

    /* ── Initialise game state (loads save or sets up defaults) ── */
    Err err = GenAppStart();
    if (err)
    {
        ui_clear_screen();
        ui_printf(0, 14, PAL_WARN, "Startup error! Halting.");
        for (;;) ui_vsync();
    }

    /* If new game, start fresh (replaces Palm's NewCommanderFormHandleEvent) */
    if (new_game)
    {
        StartNewGame();
    }

    /* ── Main game loop ── */
    GenAppLoop();

    /* ── Shutdown: save state ── */
    GenAppStop();

    /* Halt */
    for (;;) ui_vsync();
    return FALSE;
}
