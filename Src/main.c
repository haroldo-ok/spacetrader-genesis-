/**
 * main.c  -  Space Trader 1.2.0 for Sega Genesis (SGDK 1.70)
 *
 * Joypad:  A=confirm  B=back  C=alt  Start=menu  D-Pad=navigate
 * SRAM:    [2B magic][~700B save][2B hs-magic][84B highscores]
 */

#include "genesis.h"
#include "external.h"
#include "ui.h"
#include "ui_screens.h"

extern Err  GenAppStart(void);
extern void GenAppLoop(void);
extern void GenAppStop(void);

/* -----------------------------------------------------------------------
 * Splash / title screen
 * --------------------------------------------------------------------- */
static u16 show_splash(void)
{
    ui_clear_screen();
    ui_printf(4,  4, PAL_TITLE,  " ___  ___  _   ___ ___  ");
    ui_printf(4,  5, PAL_TITLE,  "/ __|| _ |/ | / __| __| ");
    ui_printf(4,  6, PAL_TITLE,  "|__ ||  _/ _ | (__| _|  ");
    ui_printf(4,  7, PAL_TITLE,  "|___/|_|/_/ |_|___|___| ");
    ui_printf(3,  9, PAL_HILIGHT,"  T R A D E R  1 . 2 . 0 ");
    ui_printf(0, 11, PAL_NORMAL, "  Sega Genesis Port");
    ui_printf(0, 12, PAL_DIM,    "  Pieter Spronck / SGDK 1.70");
    ui_printf(0, UI_STATUS_ROW,  PAL_STATUS, "  A=New Game       B=Continue  ");
    for (;;) {
        ui_vsync();
        if (ui_joy_pressed & (BTN_A|BTN_B|BTN_START))
            return ui_joy_pressed;
    }
}

/* -----------------------------------------------------------------------
 * Main entry point
 * --------------------------------------------------------------------- */
/* SGDK sys.c declares: bool main(bool hardReset) where bool = unsigned short (u16).
 * Use u16 explicitly to avoid LTO type-mismatch with SGDK's bool=u8 in our compat. */
u16 main(u16 hardReset)
{
    (void)hardReset;
    VDP_setScreenWidth320();
    kprintf("main: start");
    ui_init();
    SRAM_enable();
    SRAM_disable();

    u16 splash_btn = show_splash();
    kprintf("main: splash done, btn=0x%04x", (int)splash_btn);

    Boolean has_save = sram_has_save();
    kprintf("main: has_save=%d", (int)has_save);

    /* Decide new vs continue based on button pressed and save availability */
    Boolean new_game;
    if (splash_btn & BTN_B) {
        /* Player wants to continue */
        if (has_save) {
            new_game = false;
        } else {
            /* No save found — inform and start new game */
            ui_clear_screen();
            ui_title("NO SAVE FOUND");
            ui_printf(0, UI_BODY_TOP + 2, PAL_WARN,   "No saved game found.");
            ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL, "Starting a new game...");
            ui_status("Press any button");
            for (;;) {
                ui_vsync();
                if (ui_joy_pressed) break;
            }
            new_game = true;
        }
    } else {
        /* A or Start: new game (ignore existing save) */
        new_game = true;
    }

    if (new_game) {
        /* Zero magic bytes so GenAppStart doesn't load the stale save */
        SRAM_enable();
        uint16_t zero = 0;
        SRAM_writeBuffer(&zero, 0, sizeof(zero));
        SRAM_disable();
    }

    Err err = GenAppStart();
    kprintf("main: GenAppStart=%d", (int)err);
    if (err) {
        ui_clear_screen();
        ui_printf(0, 14, PAL_WARN, "Startup error! Halting.");
        for (;;) ui_vsync();
    }

    if (new_game) {
        /* StartNewGame() initialises state and ends with
         * FrmGotoForm(NewCommanderForm) which sets SCREEN_NEW_COMMANDER.
         * screen_new_commander() handles name, difficulty, and skill points. */
        StartNewGame();
        kprintf("main: StartNewGame done");
    } else {
        /* Loaded save - go straight to the system info screen */
        ui_goto_screen(SCREEN_SYSTEM_INFO);
    }

    kprintf("main: entering GenAppLoop");
    GenAppLoop();
    kprintf("main: GenAppLoop returned");

    GenAppStop();
    for (;;) ui_vsync();
    return 1;  /* SGDK sys.c expects non-zero = success */
}
