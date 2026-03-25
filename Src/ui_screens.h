/**
 * ui_screens.h  -  Screen navigation interface for Space Trader Genesis
 *
 * This header declares ONLY what ui_screens.c provides:
 * the ScreenID enum, the game loop, screen navigation, and the
 * FrmGotoForm->palm_form_to_screen bridge.
 *
 * All dialog/alert functions are in ui.h / ui.c.
 */

#ifndef UI_SCREENS_H
#define UI_SCREENS_H

/* Screen identifiers (replaces Palm form IDs in game logic) */
typedef enum {
    SCREEN_NONE = 0,
    SCREEN_SYSTEM_INFO,
    SCREEN_WARP,
    SCREEN_EXECUTE_WARP,
    SCREEN_GALACTIC_CHART,
    SCREEN_BUY_CARGO,
    SCREEN_SELL_CARGO,
    SCREEN_DUMP_CARGO,
    SCREEN_SHIPYARD,
    SCREEN_BUY_SHIP,
    SCREEN_BUY_EQUIPMENT,
    SCREEN_SELL_EQUIPMENT,
    SCREEN_PERSONNEL,
    SCREEN_BANK,
    SCREEN_COMMANDER_STATUS,
    SCREEN_CURRENT_SHIP,
    SCREEN_QUESTS,
    SCREEN_AVERAGE_PRICES,
    SCREEN_ENCOUNTER,
    SCREEN_PLUNDER,
    SCREEN_SPECIAL_EVENT,
    SCREEN_SPECIAL_CARGO,
    SCREEN_NEWSPAPER,
    SCREEN_DESTROYED,
    SCREEN_RETIRE,
    SCREEN_UTOPIA,
    SCREEN_NEW_COMMANDER,
    SCREEN_QUIT
} ScreenID;

/* Screen navigation */
void     ui_goto_screen(ScreenID id);
ScreenID ui_current_screen(void);

/* Main game loop - runs until SCREEN_QUIT */
void ui_game_loop(void);

/* Called by FrmGotoForm macro (palmcompat.h) */
void palm_form_to_screen(int formID);

#endif /* UI_SCREENS_H */
