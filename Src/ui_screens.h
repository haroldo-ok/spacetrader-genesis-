/**
 * ui_screens.h  –  Clean UI/game-logic interface for Space Trader Genesis
 *
 * This header defines the ONLY way game logic should talk to the UI layer.
 * No Palm OS types, no form IDs, no pixel coordinates.
 *
 * Design principle:
 *   Game logic calls a ui_* function with semantic data.
 *   The UI function draws, waits for input, and returns a semantic result.
 *   Game logic never calls VDP_drawText, JOY_readJoypad, etc. directly.
 */

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

/* -----------------------------------------------------------------------
 * Screen identifiers (replaces Palm form IDs in game logic)
 * --------------------------------------------------------------------- */
typedef enum {
    SCREEN_NONE = 0,
    SCREEN_SYSTEM_INFO,      /* docked: system overview + navigation menu  */
    SCREEN_WARP,             /* warp destination selection                 */
    SCREEN_EXECUTE_WARP,     /* warp confirmation                         */
    SCREEN_GALACTIC_CHART,   /* full galaxy map                           */
    SCREEN_BUY_CARGO,        /* commodity buy screen                      */
    SCREEN_SELL_CARGO,       /* commodity sell screen                     */
    SCREEN_DUMP_CARGO,    /* discard cargo (police confiscation)       */
    SCREEN_SHIPYARD,         /* shipyard main                             */
    SCREEN_BUY_SHIP,         /* ship purchase                             */
    SCREEN_BUY_EQUIPMENT,    /* equipment purchase                        */
    SCREEN_SELL_EQUIPMENT,   /* equipment sale                            */
    SCREEN_PERSONNEL,        /* hire/fire crew members                    */
    SCREEN_BANK,             /* loan / repay                              */
    SCREEN_COMMANDER_STATUS, /* commander stats overview                  */
    SCREEN_CURRENT_SHIP,     /* ship stats overview                       */
    SCREEN_QUESTS,           /* active quests list                        */
    SCREEN_AVERAGE_PRICES,   /* commodity price overview                  */
    SCREEN_ENCOUNTER,        /* space encounter                           */
    SCREEN_PLUNDER,          /* plunder defeated ship                     */
    SCREEN_SPECIAL_EVENT,    /* special event dialog                      */
    SCREEN_SPECIAL_CARGO,    /* special cargo manifest                    */
    SCREEN_NEWSPAPER,        /* galactic headline                         */
    SCREEN_DESTROYED,        /* ship destroyed                            */
    SCREEN_RETIRE,           /* retire commander                          */
    SCREEN_UTOPIA,           /* utopia ending                             */
    SCREEN_NEW_COMMANDER,    /* name + skill entry (new game only)        */
    SCREEN_QUIT
} ScreenID;

/* -----------------------------------------------------------------------
 * Simple dialogs  (blocking – draw, wait for input, return)
 * --------------------------------------------------------------------- */

/* Show an informational message. Player presses A to continue. */
void ui_alert(const char* msg);

/* Show a formatted alert using an alert ID (maps to a message string).
 * Returns 0 for the first/only button, 1 for the second, etc. */
int  ui_alert_id(int alertID);

/* Ask a yes/no question. Returns 1=yes, 0=no. */
int  ui_ask_yesno(const char* question);

/* Ask the player to choose a quantity from 0..max.
 * title = screen title, item = what is being bought/sold.
 * Returns chosen quantity (0 = cancel). */
int  ui_ask_quantity(const char* title, const char* item, int max, int price);

/* -----------------------------------------------------------------------
 * Screen navigation
 * Called by game logic to switch to a new screen.
 * The screen is rendered by the main loop on the next iteration.
 * --------------------------------------------------------------------- */
void ui_goto_screen(ScreenID id);
ScreenID ui_current_screen(void);

/* -----------------------------------------------------------------------
 * Main game loop entry point (replaces GenAppLoop / AppEventLoop)
 * Runs until SCREEN_QUIT is set.
 * --------------------------------------------------------------------- */
void ui_game_loop(void);

/* -----------------------------------------------------------------------
 * Alert ID → message string lookup
 * Replaces FrmAlert(AlertID) throughout game logic.
 * --------------------------------------------------------------------- */
const char* ui_alert_message(int alertID);

/* -----------------------------------------------------------------------
 * Utility: feed the SGDK watchdog (call from any long computation)
 * --------------------------------------------------------------------- */
void ui_watchdog(void);

/* Called by FrmGotoForm macro */
void palm_form_to_screen(int formID);
int  ui_custom_alert(int alertID, const char* s1, const char* s2, const char* s3);

#endif /* UI_SCREENS_H */
