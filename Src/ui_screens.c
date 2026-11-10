/**
 * ui_screens.c  -  Screen implementations for Space Trader Genesis
 *
 * Each function here renders one game screen and handles its input loop.
 * Game logic globals (SolarSystem[], Ship, etc.) are read directly.
 * Game logic functions (DeterminePrices(), BuyItem(), etc.) are called
 * when the player takes an action.
 *
 * This replaces the entire Palm OS form/event dispatch system.
 */

#include "genesis.h"
#include "ui.h"
#include "gfx.h"
#include "ui_screens.h"
#include "external.h"
#include "compat.h"
#ifndef SGDK_GCC
extern void* memset(void* s, int c, unsigned int n);
extern void* memcpy(void* d, const void* s, unsigned int n);
extern char* strncpy(char* d, const char* s, unsigned int n);
extern int   strcmp(const char* a, const char* b);
#endif


/* -----------------------------------------------------------------------
 * Forward declarations for functions defined in other translation units.
 * Declared at file scope so LTO resolves them correctly.
 * --------------------------------------------------------------------- */
extern void DeterminePrices(Byte SystemID);          /* Traveler.c */
extern long StandardPrice(char Good, char Size,      /* Traveler.c */
    char Tech, char Govt, int Res);
extern void DetermineShipPrices(void);               /* BuyShipEvent.c */
extern int  NextSystemWithinRange(int Cur, Boolean Back); /* Traveler.c */
extern int  GetForHire(void);                        /* SystemInfoEvent.c */
extern long TotalShields(SHIP* Sh);                  /* Encounter.c */
extern long TotalShieldStrength(SHIP* Sh);           /* Encounter.c */
extern void Travel(void);                            /* Traveler.c */
extern void PlunderCargo(int Index, int Amount);     /* Cargo.c */
extern char FindSystem[];                            /* WarpFormEvent.c */


/* -----------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------- */
static ScreenID _cur_screen = SCREEN_NONE;
static ScreenID _next_screen = SCREEN_NONE;

/* -----------------------------------------------------------------------
 * Navigation
 * --------------------------------------------------------------------- */
void ui_goto_screen(ScreenID id)
{
    kprintf("ui_goto_screen: %d", (int)id);
    _next_screen = id;
}

ScreenID ui_current_screen(void)
{
    return _cur_screen;
}

ScreenID ui_next_screen(void)
{
    return _next_screen;
}

/* -----------------------------------------------------------------------
 * Local quantity spinner (ui_screens-private)
 * Replaces old ui_ask_quantity; does NOT conflict with ui.c symbols.
 * --------------------------------------------------------------------- */



/* -----------------------------------------------------------------------
 * Forward declarations for screen functions
 * --------------------------------------------------------------------- */
static void screen_system_info(void);
static void screen_warp(void);
static void screen_execute_warp(void);
static void screen_galactic_chart(void);
static void screen_buy_cargo(void);
static void screen_sell_cargo(void);
static void screen_discard_cargo(void);
static void screen_shipyard(void);
static void screen_buy_ship(void);
static void screen_buy_equipment(void);
static void screen_sell_equipment(void);
static void screen_personnel(void);
static void screen_bank(void);
static void screen_commander_status(void);
static void screen_current_ship(void);
static void screen_quests(void);
static void screen_average_prices(void);
static void screen_encounter(void);
static void screen_plunder(void);
static void screen_special_event(void);
static void screen_special_cargo(void);
static void screen_newspaper(void);
static void screen_destroyed(void);
static void screen_retire(void);
static void screen_utopia(void);
static void screen_new_commander(void);
static void screen_shiptype_info(void);
static void screen_main(void);
extern void screen_warp_gfx(void);
extern void screen_galactic_gfx(void);

/* -----------------------------------------------------------------------
 * Screen dispatch table
 * --------------------------------------------------------------------- */
typedef void (*screen_fn_t)(void);

static const struct { ScreenID id; screen_fn_t fn; } _screens[] = {
    { SCREEN_SYSTEM_INFO,      screen_system_info      },
    { SCREEN_WARP,             screen_warp             },
    { SCREEN_EXECUTE_WARP,     screen_execute_warp     },
    { SCREEN_GALACTIC_CHART,   screen_galactic_chart   },
    { SCREEN_BUY_CARGO,        screen_buy_cargo        },
    { SCREEN_SELL_CARGO,       screen_sell_cargo       },
    { SCREEN_DUMP_CARGO,    screen_discard_cargo    },
    { SCREEN_SHIPYARD,         screen_shipyard         },
    { SCREEN_BUY_SHIP,         screen_buy_ship         },
    { SCREEN_BUY_EQUIPMENT,    screen_buy_equipment    },
    { SCREEN_SELL_EQUIPMENT,   screen_sell_equipment   },
    { SCREEN_PERSONNEL,        screen_personnel        },
    { SCREEN_BANK,             screen_bank             },
    { SCREEN_COMMANDER_STATUS, screen_commander_status },
    { SCREEN_CURRENT_SHIP,     screen_current_ship     },
    { SCREEN_QUESTS,           screen_quests           },
    { SCREEN_AVERAGE_PRICES,   screen_average_prices   },
    { SCREEN_ENCOUNTER,        screen_encounter        },
    { SCREEN_PLUNDER,          screen_plunder          },
    { SCREEN_SPECIAL_EVENT,    screen_special_event    },
    { SCREEN_SPECIAL_CARGO,    screen_special_cargo    },
    { SCREEN_NEWSPAPER,        screen_newspaper        },
    { SCREEN_DESTROYED,        screen_destroyed        },
    { SCREEN_RETIRE,           screen_retire           },
    { SCREEN_UTOPIA,           screen_utopia           },
    { SCREEN_NEW_COMMANDER,    screen_new_commander    },
    { SCREEN_SHIPTYPE_INFO,    screen_shiptype_info    },
    { SCREEN_MAIN,             screen_main             },
    { SCREEN_NONE,             NULL                    }
};

/* -----------------------------------------------------------------------
 * Main game loop
 * --------------------------------------------------------------------- */
void ui_game_loop(void)
{
    int i;
    kprintf("ui_game_loop: starting, screen=%d", (int)_next_screen);

    while (_next_screen != SCREEN_QUIT && _next_screen != SCREEN_NONE) {
        _cur_screen = _next_screen;
        _next_screen = SCREEN_NONE;

        /* Find and call the screen function */
        for (i = 0; _screens[i].id != SCREEN_NONE; i++) {
            if (_screens[i].id == _cur_screen) {
                kprintf("ui_game_loop: running screen %d", (int)_cur_screen);
                gfx_reset();
                ui_clear_screen();
                _screens[i].fn();
                break;
            }
        }

        /* If screen function didn't set a next screen, we're done */
        if (_next_screen == SCREEN_NONE) {
            kprintf("ui_game_loop: screen %d set no next, stopping", (int)_cur_screen);
            break;
        }
    }
    kprintf("ui_game_loop: done");
}

/* -----------------------------------------------------------------------
 * Helper macros for screen implementations
 * --------------------------------------------------------------------- */
/* Draw a labelled value on the screen */
#define DRAW_LV(col, row, pal, label, val) \
    ui_printf(col, row, pal, "%-14s %d", label, (int)(val))
#define DRAW_LS(col, row, pal, label, str) \
    ui_printf(col, row, pal, "%-14s %s", label, str)

/* Wait for a button press, return the button mask */
static u16 wait_button(void)
{
    for (;;) {
        ui_vsync();
        if (ui_joy_pressed) return ui_joy_pressed;
    }
}

/* -----------------------------------------------------------------------
 * SCREEN: System Information (main docked screen)
 * Shows current system info + menu of available actions.
 * --------------------------------------------------------------------- */
/* Forward declarations of game functions we call */
extern void DockDetermineActions(void);    /* sets dockable flags */
extern Boolean DockedFormDoCommand(int cmd);  /* execute a docked action */
/* MenuCommand* values come from MerchantRsc.h via external.h - do NOT redeclare */


/* ========================================================================
 * REAL SCREEN IMPLEMENTATIONS
 * Replace the run_palm_screen() stubs one by one.
 * All game logic calls are delegated to the existing handler functions
 * via synthetic events — we don't duplicate any logic.
 * ====================================================================== */

/* Forward declarations of game handler functions */
extern Boolean SystemInformationFormHandleEvent(EventPtr ep);
extern Boolean NewspaperFormHandleEvent(EventPtr ep);
extern Boolean PersonnelRosterFormHandleEvent(EventPtr ep);
extern Boolean WarpFormHandleEvent(EventPtr ep);
extern Boolean GalacticChartFormHandleEvent(EventPtr ep);
extern Boolean BuyCargoFormHandleEvent(EventPtr ep);
extern Boolean SellCargoFormHandleEvent(EventPtr ep);
extern Boolean DiscardCargoFormHandleEvent(EventPtr ep);
extern Boolean ShipYardFormHandleEvent(EventPtr ep);
extern Boolean BuyShipFormHandleEvent(EventPtr ep);
extern Boolean BuyEquipmentFormHandleEvent(EventPtr ep);
extern Boolean SellEquipmentFormHandleEvent(EventPtr ep);
extern Boolean BankFormHandleEvent(EventPtr ep);
extern Boolean CommanderStatusFormHandleEvent(EventPtr ep);
extern Boolean CurrentShipFormHandleEvent(EventPtr ep);
extern Boolean QuestsFormHandleEvent(EventPtr ep);
extern Boolean AveragePricesFormHandleEvent(EventPtr ep);
extern Boolean EncounterFormHandleEvent(EventPtr ep);
extern Boolean PlunderFormHandleEvent(EventPtr ep);
extern Boolean SpecialEventFormHandleEvent(EventPtr ep);
extern Boolean ShiptypeInfoFormHandleEvent(EventPtr ep);
extern Boolean DestroyedFormHandleEvent(EventPtr ep);
extern Boolean RetireFormHandleEvent(EventPtr ep);
extern Boolean UtopiaFormHandleEvent(EventPtr ep);

/* -----------------------------------------------------------------------
 * Generic screen runner using a given form handler function.
 *
 * Delivers frmOpenEvent once, then runs an input loop:
 *   D-Pad Up/Down  → synthetic keyDown (pgUp/pgDown for list scroll)
 *   A              → ctlSelectEvent on the "primary" button
 *   B              → back (FrmGotoForm to previous via game logic, or
 *                    just set next_screen = SCREEN_SYSTEM_INFO as fallback)
 *   Start          → opens the main menu (future work)
 *
 * The handler is expected to call ui_goto_screen() (via FrmGotoForm) when
 * it wants to navigate away.
 * --------------------------------------------------------------------- */
typedef Boolean (*form_handler_t)(EventPtr ep);

/* Button ID arrays for each screen's primary action buttons.
 * We synthesise ctlSelectEvent with these IDs on A press. */

/* Deliver a synthetic open event to register the form and draw it */
static void deliver_open(form_handler_t handler, int formID)
{
    EventType ev;
    ui_labels_clear();
    memset(&ev, 0, sizeof(ev));
    ev.eType = frmLoadEvent;
    ev.data.frmLoad.formID = (u16)formID;
    extern Boolean AppHandleEvent(EventType* ep);
    AppHandleEvent(&ev);

    memset(&ev, 0, sizeof(ev));
    ev.eType = frmOpenEvent;
    ev.data.frmOpen.formID = (u16)formID;
    handler(&ev);
}

/* Deliver a menu command as a menuEvent so DockedFormDoCommand is called */

/* Deliver a button press as a ctlSelectEvent */
static void deliver_button(form_handler_t handler, int buttonID)
{
    EventType ev;
    memset(&ev, 0, sizeof(ev));
    ev.eType = ctlSelectEvent;
    ev.data.ctlSelect.controlID = (u16)buttonID;
    handler(&ev);
}



/* -----------------------------------------------------------------------
 * MAIN MENU - the three original Palm menus (Command / Game / Help).
 * Returns true when the chosen command navigated to another screen.
 * --------------------------------------------------------------------- */
typedef struct { const char* label; int cmd; } MenuItem;
static const MenuItem _menu_command[] = {
    { "Buy Cargo",          MenuCommandBuyCargo },
    { "Sell Cargo",         MenuCommandSellCargo },
    { "Ship Yard",          MenuCommandShipYard },
    { "Buy Equipment",      MenuCommandBuyEquipment },
    { "Sell Equipment",     MenuCommandSellEquipment },
    { "Personnel Roster",   MenuCommandPersonnelRoster },
    { "Bank",               MenuCommandBank },
    { "System Information", MenuCommandSystemInformation },
    { "Commander Status",   MenuCommandCommanderStatus },
    { "Galactic Chart",     MenuCommandGalacticChart },
    { "Short Range Chart",  MenuCommandShortRangeChart },
    { NULL, 0 }
};
static const MenuItem _menu_game[] = {
    { "New Game",           MenuGameNewGame },
    { "Switch Game",        MenuGameSwitchGame },
    { "Retire",             MenuGameRetire },
    { "Options",            MenuGameOptions },
    { "High Scores",        MenuGameHighScores },
    { "Clear High Scores",  MenuGameClearHighScores },
    { "Snapshot",           MenuGameSnapshot },
    { NULL, 0 }
};
static const MenuItem _menu_help[] = {
    { "Help Current Screen", MenuHelpHelpCurrentScreen },
    { "Help On Menu",        MenuHelpHelpOnMenu },
    { "How To Play",         MenuHelpHowToPlay },
    { "Trading",             MenuHelpTrading },
    { "Traveling",           MenuHelpTraveling },
    { "Ship Equipment",      MenuHelpShipEquipment },
    { "Skills",              MenuHelpSkills },
    { "First Steps",         MenuHelpFirstSteps },
    { "Acknowledgements",    MenuHelpAcknowledgements },
    { "About",               MenuHelpAbout },
    { NULL, 0 }
};
static const struct { const char* name; const MenuItem* items; } _menus[3] = {
    { "Command", _menu_command }, { "Game", _menu_game }, { "Help", _menu_help }
};

/* The Palm version stored the running game whenever the application was
 * left. A console is simply switched off, so store it at safe points. */
void ui_autosave(void)
{
    extern Boolean SaveGame(int State);
    if (NameCommander[0]) SaveGame(0);
}

Boolean ui_menu(void)
{
    static int m = 0, sel = 0;
    int i, n;
    extern Boolean AppHandleEvent(EventType* ep);

    ui_autosave();

    gfx_hide();
    for (;;) {
        const MenuItem* it = _menus[m].items;
        for (n = 0; it[n].label; n++) ;
        if (sel >= n) sel = n - 1;
        ui_clear_body();
        ui_title("MENU");
        for (i = 0; i < 3; i++)
            ui_printf((u8)(2 + i * 13), UI_BODY_TOP, i == m ? PAL_HILIGHT : PAL_DIM,
                      i == m ? "[%s]" : " %s ", _menus[i].name);
        ui_hline(UI_BODY_TOP + 1);
        for (i = 0; i < n; i++)
            ui_printf(2, (u8)(UI_BODY_TOP + 3 + i), i == sel ? PAL_HILIGHT : PAL_NORMAL,
                      "%s%s", i == sel ? "> " : "  ", it[i].label);
        ui_status("L/R=menu  UP/DN=item  A=select  B=close");
        for (;;) {
            u16 joy = wait_button();
            if (joy & BTN_DOWN)  { sel = (sel + 1) % n; break; }
            if (joy & BTN_UP)    { sel = (sel + n - 1) % n; break; }
            if (joy & BTN_RIGHT) { m = (m + 1) % 3; sel = 0; break; }
            if (joy & BTN_LEFT)  { m = (m + 2) % 3; sel = 0; break; }
            if (joy & (BTN_B | BTN_START)) return false;
            if (joy & BTN_A) {
                EventType ev;
                memset(&ev, 0, sizeof(ev));
                ev.eType = menuEvent;
                ev.data.menu.itemID = (u16)it[sel].cmd;
                AppHandleEvent(&ev);
                if (_next_screen != SCREEN_NONE) return true;
                if (m != 2) return false;
                break;
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * SYSTEM INFORMATION SCREEN
 * Shows system stats. Buttons: Special, Mercenary, News, B=back
 * --------------------------------------------------------------------- */
static void screen_system_info(void)
{
    static int last_sys = -1, last_days = -1;
    /* Deliver open to let SystemInformationFormHandleEvent add news events */
    deliver_open(SystemInformationFormHandleEvent, SystemInformationForm);
    if (last_sys != COMMANDER.CurSystem || last_days != Days) {
        last_sys = COMMANDER.CurSystem; last_days = Days;
        ui_autosave();
    }

    /* Draw our own clean version on top – all data from game globals */
    ui_clear_body();
    ui_title(SolarSystemName[CURSYSTEM.NameIndex]);

    CURSYSTEM.Visited = true;

    ui_printf(0, UI_BODY_TOP + 0, PAL_NORMAL, "Tech   : %-14s", TechLevel[CURSYSTEM.TechLevel]);
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL, "Govt   : %-14s", Politics[CURSYSTEM.Politics].Name);
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL, "Police : %-14s", Activity[Politics[CURSYSTEM.Politics].StrengthPolice]);
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL, "Pirates: %-14s", Activity[Politics[CURSYSTEM.Politics].StrengthPirates]);
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL, "Size   : %-14s", SystemSize[CURSYSTEM.Size]);
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL, "Res    : %-14s", SpecialResources[CURSYSTEM.SpecialResources]);
    ui_printf(0, UI_BODY_TOP + 6, PAL_NORMAL, "Status : %-14s", Status[CURSYSTEM.Status]);
    ui_printf(0, UI_BODY_TOP + 8, PAL_NORMAL, "Fuel   : %d/%d  Credits: %ld",
              (int)Ship.Fuel, (int)Shiptype[Ship.Type].FuelTanks, Credits);

    /* Check if special event is available */
    Boolean has_special = false;
    {
        EventType ev; memset(&ev, 0, sizeof(ev));
        /* The handler already ran frmOpenEvent which set up news; 
         * we check special visibility ourselves */
        int OpenQ = OpenQuests();
        has_special = (CURSYSTEM.Special >= 0 && OpenQ <= 3);
    }
    Boolean has_merc = (GetForHire() >= 0);

    ui_printf(0, UI_BODY_TOP + 10, PAL_DIM,
        "A=Warp B=Menu C=Status S=News");
    if (has_special)
        ui_printf(0, UI_BODY_TOP + 11, PAL_HILIGHT, "  Up=Special event available!");
    if (has_merc)
        ui_printf(0, UI_BODY_TOP + 12, PAL_HILIGHT, "  Dn=Mercenary for hire!");

    ui_status("A=Warp  B=Menu  C=Status  S=News");

    for (;;) {
        u16 joy = wait_button();

        if (joy & BTN_A) {
            ui_goto_screen(SCREEN_WARP);
            break;
        }
        if (joy & BTN_B) {
            CurForm = SystemInformationForm;
            if (!ui_menu()) ui_goto_screen(SCREEN_SYSTEM_INFO);   /* redraw */
            break;
        }
        if (joy & BTN_C) {
            ui_goto_screen(SCREEN_COMMANDER_STATUS);
            break;
        }
        if (joy & BTN_START) {
            deliver_button(SystemInformationFormHandleEvent, SystemInformationNewsButton);
            if (_next_screen == SCREEN_NONE) ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
        if ((joy & BTN_UP) && has_special) {
            deliver_button(SystemInformationFormHandleEvent, SystemInformationSpecialButton);
            break;
        }
        if ((joy & BTN_DOWN) && has_merc) {
            deliver_button(SystemInformationFormHandleEvent, SystemInformationMercenaryForHireButton);
            break;
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * WARP SCREEN  (short-range chart) - see ui_charts.c
 * --------------------------------------------------------------------- */
static void screen_warp(void)
{
    screen_warp_gfx();
}

/* -----------------------------------------------------------------------
 * EXECUTE WARP SCREEN
 * Shows destination info and asks confirmation.
 * --------------------------------------------------------------------- */
static void _draw_execute_warp(void)
{
    Boolean wh = WormholeExists(COMMANDER.CurSystem, WarpSystem);
    Boolean inrange = wh || RealDistance(CURSYSTEM, SolarSystem[WarpSystem]) <= GetFuel();
    ui_clear_body();
    ui_title("TARGET SYSTEM");
    ui_printf(0, UI_BODY_TOP + 0, PAL_HILIGHT, "Name:          %s", ui_label(ExecuteWarpSystemNameLabel));
    ui_printf(0, UI_BODY_TOP + 1, PAL_NORMAL,  "Size:          %s", ui_label(ExecuteWarpSizeLabel));
    ui_printf(0, UI_BODY_TOP + 2, PAL_NORMAL,  "Tech level:    %s", ui_label(ExecuteWarpTechLevelLabel));
    ui_printf(0, UI_BODY_TOP + 3, PAL_NORMAL,  "Government:    %s", ui_label(ExecuteWarpGovernmentLabel));
    ui_printf(0, UI_BODY_TOP + 4, PAL_NORMAL,  "Distance:      %s", ui_label(ExecuteWarpRangeLabel));
    ui_printf(0, UI_BODY_TOP + 5, PAL_NORMAL,  "Police:        %s", ui_label(ExecuteWarpPoliceLabel));
    ui_printf(0, UI_BODY_TOP + 6, PAL_NORMAL,  "Pirates:       %s", ui_label(ExecuteWarpPiratesLabel));
    ui_printf(0, UI_BODY_TOP + 7, PAL_NORMAL,  "Current costs: %s", ui_label(ExecuteWarpCurrentCostsLabel));
    if (!inrange)
        ui_print(0, UI_BODY_TOP + 9, PAL_WARN, "This system is out of range.");
    ui_printf(0, UI_BODY_TOP + 11, PAL_DIM, "Fuel: %d parsecs    Cash: %ld cr.", (int)GetFuel(), Credits);
    ui_printf(0, UI_BODY_TOP + 13, PAL_DIM, "LEFT/RIGHT = other systems in range");
    ui_printf(0, UI_BODY_TOP + 14, PAL_DIM, "UP = cost specification");
    ui_status(inrange ? "A=Warp C=Avg prices B=Chart" : "B=Chart");
}

/* Drives the original ExecuteWarpFormHandleEvent, so DoWarp() runs with all
 * its checks (debt, mercenaries, insurance, wormhole tax, fuel, interest). */
static void screen_execute_warp(void)
{
    deliver_open(ExecuteWarpFormHandleEvent, ExecuteWarpForm);
    _draw_execute_warp();
    for (;;) {
        u16 joy = wait_button();
        Boolean wh = WormholeExists(COMMANDER.CurSystem, WarpSystem);
        Boolean inrange = wh || RealDistance(CURSYSTEM, SolarSystem[WarpSystem]) <= GetFuel();
        int btn = 0;
        if ((joy & BTN_A) && inrange) btn = ExecuteWarpWarpButton;
        if ((joy & BTN_C) && inrange) btn = ExecuteWarpPricesButton;
        if (joy & BTN_UP)    btn = ExecuteWarpSpecificationButton;
        /* the Palm handler would index system -1 when nothing is in range */
        if ((joy & BTN_LEFT) && NextSystemWithinRange(WarpSystem, true) >= 0)   btn = ExecuteWarpScrollBackButton;
        if ((joy & BTN_RIGHT) && NextSystemWithinRange(WarpSystem, false) >= 0) btn = ExecuteWarpScrollFwdButton;
        if (btn) {
            deliver_button(ExecuteWarpFormHandleEvent, btn);
            if (_next_screen != SCREEN_NONE) break;
            deliver_open(ExecuteWarpFormHandleEvent, ExecuteWarpForm);
            _draw_execute_warp();
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_WARP); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * GALACTIC CHART
 * Shows all known systems. Player picks with D-Pad, A=select, B=back.
 * --------------------------------------------------------------------- */
static void screen_galactic_chart(void)
{
    screen_galactic_gfx();
}

/* -----------------------------------------------------------------------
 * BUY CARGO SCREEN
 * Lists MAXTRADEITEM commodities. D-Pad selects, A=buy qty, C=buy all.
 * --------------------------------------------------------------------- */
static void draw_cargo_list(int sel, int buying)
{
    int i;
    ui_clear_body();
    ui_title(buying ? "BUY CARGO" : "SELL CARGO");
    ui_printf(0, UI_BODY_TOP, PAL_DIM, buying ? "   Item        Stock   Price"
                                               : "   Item        Hold    Price");
    for (i = 0; i < MAXTRADEITEM; i++) {
        int row = UI_BODY_TOP + 2 + i;
        const char* mark = (i == sel) ? ">" : " ";
        if (buying) {
            if (BuyPrice[i] > 0)
                ui_printf(0, row, i == sel ? PAL_HILIGHT : PAL_NORMAL, "%s  %-12s %3d   %5ld cr.",
                          mark, Tradeitem[i].Name, (int)CURSYSTEM.Qty[i], (long)BuyPrice[i]);
            else
                ui_printf(0, row, i == sel ? PAL_HILIGHT : PAL_DIM, "%s  %-12s       not sold",
                          mark, Tradeitem[i].Name);
        } else {
            /* like the Palm form: '!' marks items you would sell at a profit */
            int profit = Ship.Cargo[i] > 0 && SellPrice[i] > 0 &&
                         BuyingPrice[i] / Ship.Cargo[i] < SellPrice[i];
            if (SellPrice[i] > 0)
                ui_printf(0, row, i == sel ? PAL_HILIGHT : (Ship.Cargo[i] > 0 ? PAL_NORMAL : PAL_DIM),
                          "%s%s %-12s %3d   %5ld cr.", mark, profit ? "!" : " ",
                          Tradeitem[i].Name, (int)Ship.Cargo[i], (long)SellPrice[i]);
            else
                ui_printf(0, row, i == sel ? PAL_HILIGHT : PAL_DIM, "%s  %-12s %3d   no trade%s",
                          mark, Tradeitem[i].Name, (int)Ship.Cargo[i],
                          Ship.Cargo[i] > 0 ? " (dump)" : "");
        }
    }
    ui_printf(0, UI_BODY_TOP + MAXTRADEITEM + 4, PAL_NORMAL, "Bays: %d/%d     Cash: %ld cr.",
              FilledCargoBays(), TotalCargoBays(), Credits);
    ui_status(buying ? "A=Buy  C=Buy max  B=Back  START=Menu"
                     : "A=Sell/Dump C=Sell all B=Back START=Menu");
}

/* Both screens drive the original Cargo.c form handlers. */
static void _trade_screen(int buying)
{
    form_handler_t h = buying ? BuyCargoFormHandleEvent : SellCargoFormHandleEvent;
    int sel = 0;
    deliver_open(h, buying ? BuyCargoForm : SellCargoForm);
    draw_cargo_list(sel, buying);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { sel = (sel + 1) % MAXTRADEITEM; draw_cargo_list(sel, buying); }
        if (joy & BTN_UP)   { sel = (sel + MAXTRADEITEM - 1) % MAXTRADEITEM; draw_cargo_list(sel, buying); }
        if (joy & BTN_A) {
            if (buying) deliver_button(h, BuyCargoQty0Button + sel);
            else deliver_button(h, (SellPrice[sel] > 0 ? SellCargoQty0Button : SellCargoDump0Button) + sel);
            draw_cargo_list(sel, buying);
        }
        if (joy & BTN_C) {
            if (buying) deliver_button(h, BuyCargoAll0Button + sel);
            else if (SellPrice[sel] > 0) deliver_button(h, SellCargoAll0Button + sel);
            else deliver_button(h, SellCargoDump0Button + sel);
            draw_cargo_list(sel, buying);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (joy & BTN_START) {
            CurForm = buying ? BuyCargoForm : SellCargoForm;
            if (ui_menu()) break;
            draw_cargo_list(sel, buying);
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void screen_buy_cargo(void)  { _trade_screen(1); }
static void screen_sell_cargo(void) { _trade_screen(0); }

/* -----------------------------------------------------------------------
 * DISCARD CARGO SCREEN (used during plunder - dump your own cargo)
 * --------------------------------------------------------------------- */
static void screen_discard_cargo(void)
{
    /* the Dump Cargo form is modal (see GEN_FrmDoDialog); never a screen */
    ui_goto_screen(SCREEN_SYSTEM_INFO);
}

/* -----------------------------------------------------------------------
 * BANK SCREEN
 * --------------------------------------------------------------------- */
extern Boolean BankFormHandleEvent(EventPtr ep);
extern Boolean ShipYardFormHandleEvent(EventPtr ep);
extern Boolean ExecuteWarpFormHandleEvent(EventPtr ep);
static void draw_bank(void)
{
    ui_clear_body();
    ui_title("BANK");
    ui_print(0, UI_BODY_TOP + 0, PAL_TITLE, "Loan");
    ui_printf(1, UI_BODY_TOP + 1, PAL_NORMAL, "Current debt:    %s", ui_capture_at(32));
    ui_printf(1, UI_BODY_TOP + 2, PAL_NORMAL, "Maximum loan:    %s", ui_capture_at(46));
    ui_print(0, UI_BODY_TOP + 5, PAL_TITLE, "Insurance");
    ui_printf(1, UI_BODY_TOP + 6, PAL_NORMAL, "Ship value:      %s", ui_capture_at(100));
    ui_printf(1, UI_BODY_TOP + 7, PAL_NORMAL, "No-claim:        %s", ui_capture_at(114));
    ui_printf(1, UI_BODY_TOP + 8, PAL_NORMAL, "Costs:           %s", ui_capture_at(128));
    ui_printf(1, UI_BODY_TOP + 9, PAL_NORMAL, "Insured:         %s", Insurance ? "yes" : "no");
    ui_printf(0, UI_BODY_TOP + 12, PAL_NORMAL, "Cash: %ld cr.", Credits);
    ui_printf(0, UI_BODY_TOP + 15, PAL_DIM, "A = Get loan");
    ui_printf(0, UI_BODY_TOP + 16, PAL_DIM, "C = Pay back loan");
    ui_printf(0, UI_BODY_TOP + 17, PAL_DIM, "UP = %s insurance", Insurance ? "Stop" : "Buy");
    ui_status("A=Loan C=Pay back UP=Insur. B=Back");
}

/* Drives the original Bank.c handler (loan limits, insurance rules). */
static void screen_bank(void)
{
    ui_capture(2);
    deliver_open(BankFormHandleEvent, BankForm);
    draw_bank();
    for (;;) {
        u16 joy = wait_button();
        int btn = 0;
        if (joy & BTN_A) btn = BankGetLoanButton;
        if (joy & BTN_C) btn = BankPayBackButton;
        if (joy & BTN_UP) btn = Insurance ? BankStopInsuranceButton : BankBuyInsuranceButton;
        if (btn) {
            deliver_button(BankFormHandleEvent, btn);
            deliver_open(BankFormHandleEvent, BankForm);
            draw_bank();
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (joy & BTN_START) {
            CurForm = BankForm;
            if (ui_menu()) break;
            draw_bank();
        }
        if (_next_screen != SCREEN_NONE) break;
    }
    ui_capture(0);
}

/* -----------------------------------------------------------------------
 * SHIPYARD SCREEN - drives the original Shipyard.c handler; the four
 * status lines are the very strings ShowShipYard() draws.
 * --------------------------------------------------------------------- */
typedef struct { const char* label; int btn; ScreenID scr; } YardAct;
static YardAct _yard[8];
static int _yard_n;

static void draw_shipyard(int sel)
{
    int i;
    Boolean ships = CURSYSTEM.TechLevel >= Shiptype[0].MinTechLevel;
    _yard_n = 0;
    #define YA(l,b,sc) do { _yard[_yard_n].label=(l); _yard[_yard_n].btn=(b); _yard[_yard_n].scr=(sc); _yard_n++; } while(0)
    if (GetFuel() < GetFuelTanks()) { YA("Buy fuel", ShipYardBuyFuelButton, SCREEN_NONE); YA("Full tank", ShipYardFullTankButton, SCREEN_NONE); }
    if (Ship.Hull < GetHullStrength()) { YA("Repair", ShipYardRepairButton, SCREEN_NONE); YA("Full repairs", ShipYardFullRepairsButton, SCREEN_NONE); }
    YA(ships ? "Buy new ship" : "Ship information", ships ? ShipYardBuyNewShipButton : ShipYardShipInformationButton, SCREEN_NONE);
    if (!(EscapePod || ToSpend() < 2000 || !ships)) YA("Buy escape pod", ShipYardEscapePodButton, SCREEN_NONE);
    YA("Buy equipment", 0, SCREEN_BUY_EQUIPMENT);
    YA("Sell equipment", 0, SCREEN_SELL_EQUIPMENT);
    #undef YA

    ui_clear_body();
    ui_title("SHIP YARD");
    ui_print(0, UI_BODY_TOP + 0, PAL_NORMAL, ui_capture_at(18));
    ui_print(0, UI_BODY_TOP + 1, PAL_NORMAL, ui_capture_at(30));
    ui_print(0, UI_BODY_TOP + 3, PAL_NORMAL, ui_capture_at(60));
    ui_print(0, UI_BODY_TOP + 4, PAL_NORMAL, ui_capture_at(72));
    ui_print(0, UI_BODY_TOP + 6, PAL_NORMAL, ui_capture_at(102));
    ui_print(0, UI_BODY_TOP + 8, PAL_NORMAL, ui_capture_at(132));
    ui_printf(0, UI_BODY_TOP + 10, PAL_NORMAL, "Cash: %ld cr.", Credits);
    for (i = 0; i < _yard_n; i++)
        ui_printf(2, (u8)(UI_BODY_TOP + 13 + i), i == sel ? PAL_HILIGHT : PAL_NORMAL,
                  "%s%s", i == sel ? "> " : "  ", _yard[i].label);
    ui_status("UP/DN=choose  A=select  B=Back");
}

static void screen_shipyard(void)
{
    int sel = 0;
    ui_capture(2);
    deliver_open(ShipYardFormHandleEvent, ShipYardForm);
    draw_shipyard(sel);
    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { sel = (sel + 1) % _yard_n; draw_shipyard(sel); }
        if (joy & BTN_UP)   { sel = (sel + _yard_n - 1) % _yard_n; draw_shipyard(sel); }
        if (joy & BTN_A) {
            if (_yard[sel].btn) {
                deliver_button(ShipYardFormHandleEvent, _yard[sel].btn);
                if (_next_screen != SCREEN_NONE) break;
                deliver_open(ShipYardFormHandleEvent, ShipYardForm);
                draw_shipyard(sel);
                if (sel >= _yard_n) { sel = 0; draw_shipyard(sel); }
            } else {
                ui_goto_screen(_yard[sel].scr);
                break;
            }
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (joy & BTN_START) {
            CurForm = ShipYardForm;
            if (ui_menu()) break;
            draw_shipyard(sel);
        }
        if (_next_screen != SCREEN_NONE) break;
    }
    ui_capture(0);
}

/* -----------------------------------------------------------------------
 * ENCOUNTER SCREEN
 * The most complex screen. We drive EncounterFormHandleEvent with
 * synthetic ctlSelectEvents matching the button IDs.
 * --------------------------------------------------------------------- */
/* -----------------------------------------------------------------------
 * Encounter screen context-aware action menu
 *
 * Derives available actions from EncounterType and maps them to
 * Genesis buttons A/B/C/Start, updating the status bar each frame.
 * --------------------------------------------------------------------- */
typedef struct { u16 joy_bit; int ctrl_id; const char* label; } EncAct;

static int _enc_actions(EncAct acts[4])
{
    int n = 0;
    #define ACT(j,id,lbl) do{acts[n].joy_bit=(j);acts[n].ctrl_id=(id);acts[n].label=(lbl);n++;}while(0)

    if (EncounterType == POLICEINSPECTION) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterFleeButton,   "B=Flee");
        ACT(BTN_C, EncounterSubmitButton, "C=Submit");
        ACT(BTN_START, EncounterBribeButton, "St=Bribe");
    } else if (EncounterType == POSTMARIEPOLICEENCOUNTER) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterFleeButton,   "B=Flee");
        ACT(BTN_C, EncounterYieldButton,  "C=Yield");
        ACT(BTN_START, EncounterBribeButton, "St=Bribe");
    } else if (EncounterType == POLICEATTACK || EncounterType == PIRATEATTACK ||
               EncounterType == SCARABATTACK) {
        ACT(BTN_A, EncounterAttackButton,    "A=Attack");
        ACT(BTN_B, EncounterFleeButton,      "B=Flee");
        ACT(BTN_C, EncounterSurrenderButton, "C=Surrender");
    } else if (EncounterType == TRADERSURRENDER || EncounterType == PIRATESURRENDER) {
        ACT(BTN_A, EncounterAttackButton,  "A=Attack");
        ACT(BTN_B, EncounterPlunderButton, "B=Plunder");
    } else if (EncounterType == TRADERBUY || EncounterType == TRADERSELL) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
        ACT(BTN_C, EncounterTradeButton,  "C=Trade");
    } else if (EncounterType == MARIECELESTEENCOUNTER) {
        ACT(BTN_A, EncounterBoardButton,  "A=Board");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
    } else if (ENCOUNTERFAMOUS(EncounterType)) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
        ACT(BTN_C, EncounterMeetButton,   "C=Meet");
    } else if (EncounterType == BOTTLEOLDENCOUNTER ||
               EncounterType == BOTTLEGOODENCOUNTER) {
        ACT(BTN_A, EncounterDrinkButton,  "A=Drink");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
    } else if (EncounterType == POLICEFLEE || EncounterType == TRADERFLEE ||
               EncounterType == PIRATEFLEE || EncounterType == TRADERIGNORE ||
               EncounterType == POLICEIGNORE || EncounterType == PIRATEIGNORE ||
               EncounterType == SPACEMONSTERIGNORE || EncounterType == DRAGONFLYIGNORE ||
               EncounterType == SCARABIGNORE) {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterIgnoreButton, "B=Ignore");
    } else {
        ACT(BTN_A, EncounterAttackButton, "A=Attack");
        ACT(BTN_B, EncounterFleeButton,   "B=Flee");
    }
    if (AutoAttack || AutoFlee)
        ACT(BTN_START, EncounterInterruptButton, "St=Stop");
    #undef ACT
    return n;
}

static void _enc_status(void)
{
    EncAct acts[4];
    char buf[UI_COLS + 1];
    int n = _enc_actions(acts), i, j = 0;
    for (i = 0; i < n && j < UI_COLS - 2; i++) {
        int len = (int)strlen(acts[i].label);
        if (j + len + 2 > UI_COLS) break;
        if (i > 0) { buf[j++] = ' '; buf[j++] = ' '; }
        memcpy(buf + j, acts[i].label, (unsigned)len); j += len;
    }
    while (j < UI_COLS) buf[j++] = ' ';
    buf[UI_COLS] = '\0';
    ui_print(0, UI_STATUS_ROW, PAL_STATUS, buf);
}

static void _draw_encounter(void);

/* --- encounter graphics state ------------------------------------------ */
#define ENC_T_PLAYER   GFX_TILE_BASE
#define ENC_T_OPP      (GFX_TILE_BASE + 208)
#define ENC_T_ICON     (GFX_TILE_BASE + 416)
#define ENC_T_ATTACK   (GFX_TILE_BASE + 425)     /* 2 x 9 tiles */
#define ENC_T_TRIBBLE  (GFX_TILE_BASE + 443)     /* 4 tiles */
static long _enc_cache[2][3];                    /* type, hull, shield */
static s8   _enc_icon;

static void _enc_gfx_init(void)
{
    u32 t[32];
    u16 x, y;
    _enc_cache[0][0] = _enc_cache[1][0] = -1;
    _enc_icon = -2;
    gfx_set_pal(PAL0, gfx_icon_pal);
    gfx_icon2x(ENC_T_ATTACK, &gfx_icon[IC_ATTACK], 3, 3);
    gfx_icon2x(ENC_T_ATTACK + 9, &gfx_icon[IC_ATTACK2], 3, 3);
    /* tribble: the Palm form used 12x12 framed buttons labelled ".." */
    memset(t, 0, sizeof(t));
    for (y = 2; y < 14; y++)
        for (x = 2; x < 14; x++) {
            bool corner = (x == 2 || x == 13) && (y == 2 || y == 13);
            bool edge = x == 2 || x == 13 || y == 2 || y == 13;
            bool eye = (y == 8 || y == 9) && (x == 5 || x == 6 || x == 9 || x == 10);
            if (!corner && (edge || eye))
                t[((y >> 3) * 2 + (x >> 3)) * 8 + (y & 7)] |= 0xF0000000UL >> ((x & 7) * 4);
        }
    VDP_loadTileData(t, ENC_T_TRIBBLE, 4, CPU);
}

static void _enc_ship(int who, SHIP* sh, long hullmax, u16 tile, u16 pal, u16 col)
{
    long shmax = (sh->Shield[0] < 0) ? 0 : TotalShields(sh);
    long shcur = shmax > 0 ? TotalShieldStrength(sh) : 0;
    if (_enc_cache[who][0] != sh->Type || _enc_cache[who][1] != sh->Hull ||
        _enc_cache[who][2] != shcur) {
        gfx_set_pal(pal, gfx_ship_pal[sh->Type]);
        gfx_ship_draw(tile, sh->Type, sh->Hull, hullmax, shcur, shmax);
        _enc_cache[who][0] = sh->Type; _enc_cache[who][1] = sh->Hull; _enc_cache[who][2] = shcur;
    }
    gfx_place(pal, tile, col, 4, 16, 13);
}

static void _enc_stats(SHIP* sh, long hullmax, u8 col, u8 pal)
{
    long ts = TotalShields(sh);
    int hp = (int)(hullmax > 0 ? (sh->Hull * 100) / hullmax : 0);
    int sp = (int)(ts > 0 ? (TotalShieldStrength(sh) * 100) / ts : 0);
    if (TextualEncounters) {
        ui_printf(col, 6, pal, "%s", Shiptype[sh->Type].Name);
        ui_printf(col, 8, pal, "Hull at %d%c", hp, '%');
        if (sh->Shield[0] < 0) ui_print(col, 10, pal, "No shields");
        else ui_printf(col, 10, pal, "Shields at %d%c", sp, '%');
    } else {
        ui_printf(col, 17, pal, "%s", Shiptype[sh->Type].Name);
        if (sh->Shield[0] < 0) ui_printf(col, 18, pal, "Hull %d%c", hp, '%');
        else ui_printf(col, 18, pal, "Hull %d%c Shld %d%c", hp, '%', sp, '%');
    }
}

static void _enc_gfx(void)
{
    int icon = -1, i;

    ui_print(2, 3, PAL_HILIGHT, "You");
    ui_print(22, 3, PAL_TITLE, "Opponent");
    _enc_stats(&Ship, GetHullStrength(), 2, PAL_HILIGHT);
    _enc_stats(&Opponent, Shiptype[Opponent.Type].HullStrength, 22, PAL_TITLE);
    if (!TextualEncounters) {
        _enc_ship(0, &Ship, GetHullStrength(), ENC_T_PLAYER, PAL1, 2);
        _enc_ship(1, &Opponent, Shiptype[Opponent.Type].HullStrength, ENC_T_OPP, PAL2, 22);
    }

    if (ENCOUNTERPIRATE(EncounterType)) icon = (Opponent.Type == MANTISTYPE) ? IC_ALIEN : IC_PIRATE;
    else if (ENCOUNTERPOLICE(EncounterType)) icon = IC_POLICE;
    else if (ENCOUNTERTRADER(EncounterType)) icon = IC_TRADER;
    else if (EncounterType == CAPTAINHUIEENCOUNTER || EncounterType == MARIECELESTEENCOUNTER ||
             EncounterType == CAPTAINAHABENCOUNTER || EncounterType == CAPTAINCONRADENCOUNTER)
        icon = IC_SPECIAL;
    if (icon >= 0 && !TextualEncounters) {
        if (icon != _enc_icon) { gfx_icon2x(ENC_T_ICON, &gfx_icon[icon], 3, 3); _enc_icon = (s8)icon; }
        gfx_place(PAL0, ENC_T_ICON, 37, 1, 3, 3);
    }
    if (ui_attack_icon)
        gfx_place(PAL0, (u16)(ENC_T_ATTACK + (ui_attack_icon - 1) * 9), 37, 24, 3, 3);

    /* tribbles crawling over the screen (plane A, yellow) */
    for (i = 0; i < TRIBBLESONSCREEN; i++)
        if (ui_tribbles & (1UL << i))
            VDP_fillTileMapRectInc(BG_A, TILE_ATTR_FULL(PAL1, 1, 0, 0, ENC_T_TRIBBLE),
                                   (u16)(gfx_tribble_pos[i][0] / 4),
                                   (u16)(2 + (gfx_tribble_pos[i][1] * 3) / 20), 2, 2);
}

/* -----------------------------------------------------------------------
 * Encounter screen renderer: ships (original bitmaps with damage/shield
 * split exactly like the Palm ShowShip), encounter icon, and the narration
 * text Encounter.c draws with DrawChars (captured in ui_extra.c).
 * --------------------------------------------------------------------- */
static void _draw_encounter(void)
{
    const char* t = "ENCOUNTER";
    gfx_hide();
    ui_clear_body();
    if (ENCOUNTERPOLICE(EncounterType))          t = "POLICE";
    else if (ENCOUNTERPIRATE(EncounterType))     t = "PIRATE";
    else if (ENCOUNTERTRADER(EncounterType))     t = "TRADER";
    else if (ENCOUNTERMONSTER(EncounterType))    t = "SPACE MONSTER";
    else if (EncounterType == DRAGONFLYATTACK || EncounterType == DRAGONFLYIGNORE) t = "DRAGONFLY";
    else if (EncounterType == SCARABATTACK || EncounterType == SCARABIGNORE)       t = "SCARAB";
    else if (EncounterType == MARIECELESTEENCOUNTER) t = "MARIE CELESTE";
    else if (EncounterType == CAPTAINAHABENCOUNTER || EncounterType == CAPTAINCONRADENCOUNTER ||
             EncounterType == CAPTAINHUIEENCOUNTER)  t = "FAMOUS CAPTAIN";
    else if (EncounterType == BOTTLEOLDENCOUNTER || EncounterType == BOTTLEGOODENCOUNTER) t = "FLOATING BOTTLE";
    ui_title(t);
    _enc_gfx();
    ui_capture_print(20, PAL_NORMAL);
    if (AutoAttack || AutoFlee)
        ui_printf(1, 26, PAL_DIM, "Auto-%s active", AutoAttack ? "attack" : "flee");
}

static void screen_encounter(void)
{
    u16 wait = 0;
    ui_tribbles = 0; ui_attack_icon = 0;
    ui_capture(1);
    _enc_gfx_init();
    deliver_open(EncounterFormHandleEvent, EncounterForm);
    _draw_encounter();
    _enc_status();

    while (_next_screen == SCREEN_NONE) {
        ui_vsync();
        if (Continuous && (AutoAttack || AutoFlee) && !ui_joy_pressed) {
            /* the Palm version ran a combat round on each nilEvent tick */
            if (++wait < 30) continue;
            wait = 0;
            {
                EventType nil_ev;
                memset(&nil_ev, 0, sizeof(nil_ev));
                nil_ev.eType = nilEvent;
                EncounterFormHandleEvent(&nil_ev);
            }
            if (_next_screen == SCREEN_NONE) { _draw_encounter(); _enc_status(); }
            continue;
        }
        if (!ui_joy_pressed) continue;

        if ((ui_joy_pressed & BTN_UP) && ui_tribbles) {
            FrmAlert(SqueekAlert);
            _draw_encounter(); _enc_status();
            continue;
        }
        {
            EncAct acts[4];
            int n = _enc_actions(acts), i;
            for (i = 0; i < n; i++) {
                if (ui_joy_pressed & acts[i].joy_bit) {
                    if (acts[i].ctrl_id)
                        deliver_button(EncounterFormHandleEvent, acts[i].ctrl_id);
                    if (_next_screen == SCREEN_NONE) {
                        _draw_encounter();
                        _enc_status();
                    }
                    break;
                }
            }
        }
    }
    ui_capture(0);
}

/* -----------------------------------------------------------------------
 * PLUNDER SCREEN
 * --------------------------------------------------------------------- */
static void draw_plunder(int sel)
{
    int i;
    ui_clear_body();
    ui_title("PLUNDER CARGO");
    for (i = 0; i < MAXTRADEITEM; i++) {
        int qty = Opponent.Cargo[i];
        ui_printf(0, UI_BODY_TOP + 1 + i, i == sel ? PAL_HILIGHT : (qty > 0 ? PAL_NORMAL : PAL_DIM),
                  "%s %-12s %3d", i == sel ? ">" : " ", Tradeitem[i].Name, qty);
    }
    ui_printf(0, UI_BODY_TOP + MAXTRADEITEM + 3, PAL_NORMAL, "Bays: %d/%d",
              FilledCargoBays(), TotalCargoBays());
    ui_status("A=Qty  C=All  B=Dump  START=Done");
    ui_printf(0, UI_BODY_TOP + MAXTRADEITEM + 5, PAL_DIM, "B = dump some of your own cargo");
}

static void screen_plunder(void)
{
    int sel = 0;
    deliver_open(PlunderFormHandleEvent, PlunderForm);
    draw_plunder(sel);
    for (;;) {
        u16 joy = wait_button();
        int btn = 0;
        if (joy & BTN_DOWN) { sel = (sel + 1) % MAXTRADEITEM; draw_plunder(sel); }
        if (joy & BTN_UP)   { sel = (sel + MAXTRADEITEM - 1) % MAXTRADEITEM; draw_plunder(sel); }
        if (joy & BTN_A) btn = PlunderQty0Button + sel;
        if (joy & BTN_C) btn = PlunderAll0Button + sel;
        if (joy & BTN_B) btn = PlunderDumpButton;
        if (joy & BTN_START) btn = PlunderDoneButton;
        if (btn) {
            deliver_button(PlunderFormHandleEvent, btn);
            if (_next_screen != SCREEN_NONE) break;
            if (btn == PlunderDoneButton) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
            draw_plunder(sel);
        }
    }
}

/* -----------------------------------------------------------------------
 * COMMANDER STATUS SCREEN
 * --------------------------------------------------------------------- */

extern Boolean SpecialCargoFormHandleEvent(EventPtr ep);

/* Screens whose Palm form is plain text: run the original draw code with
 * DrawChars captured, then show exactly what it drew. */
static void _text_form(const char* title, form_handler_t h, int formID)
{
    ui_capture(2);
    deliver_open(h, formID);
    ui_clear_body();
    ui_title(title);
    ui_capture_print(UI_BODY_TOP + 1, PAL_NORMAL);
    ui_capture(0);
}

static void _status_tabs(form_handler_t h, int formID, const char* title,
                         u16 j1, int b1, u16 j2, int b2, u16 j3, int b3, const char* status)
{
    _text_form(title, h, formID);
    ui_status(status);
    for (;;) {
        u16 joy = wait_button();
        int btn = 0;
        if (joy & j1) btn = b1;
        if (joy & j2) btn = b2;
        if (joy & j3) btn = b3;
        if (btn) { deliver_button(h, btn); if (_next_screen != SCREEN_NONE) break; }
        if (joy & BTN_START) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void screen_commander_status(void)
{
    deliver_open(CommanderStatusFormHandleEvent, CommanderStatusForm);
    ui_clear_body();
    ui_title("COMMANDER STATUS");
    ui_printf(0, UI_BODY_TOP + 0,  PAL_HILIGHT, "Name:          %s", ui_label(CommanderStatusNameLabel));
    ui_printf(0, UI_BODY_TOP + 2,  PAL_NORMAL,  "Pilot:         %s", ui_label(CommanderStatusPilotLabel));
    ui_printf(0, UI_BODY_TOP + 3,  PAL_NORMAL,  "Fighter:       %s", ui_label(CommanderStatusFighterLabel));
    ui_printf(0, UI_BODY_TOP + 4,  PAL_NORMAL,  "Trader:        %s", ui_label(CommanderStatusTraderLabel));
    ui_printf(0, UI_BODY_TOP + 5,  PAL_NORMAL,  "Engineer:      %s", ui_label(CommanderStatusEngineerLabel));
    ui_printf(0, UI_BODY_TOP + 7,  PAL_NORMAL,  "Kills:         %s", ui_label(CommanderStatusKillsLabel));
    ui_printf(0, UI_BODY_TOP + 8,  PAL_NORMAL,  "Time:          %s", ui_label(CommanderStatusDaysLabel));
    ui_printf(0, UI_BODY_TOP + 9,  PAL_NORMAL,  "Cash:          %s", ui_label(CommanderStatusCreditsLabel));
    ui_printf(0, UI_BODY_TOP + 10, PAL_NORMAL,  "Debt:          %s", ui_label(CommanderStatusDebtLabel));
    ui_printf(0, UI_BODY_TOP + 11, PAL_NORMAL,  "Net worth:     %s", ui_label(CommanderStatusNetWorthLabel));
    ui_printf(0, UI_BODY_TOP + 13, PAL_NORMAL,  "Reputation:    %s", ui_label(CommanderStatusReputationLabel));
    ui_printf(0, UI_BODY_TOP + 14, PAL_NORMAL,  "Police record: %s", ui_label(CommanderStatusPoliceRecordLabel));
    ui_printf(0, UI_BODY_TOP + 15, PAL_NORMAL,  "Difficulty:    %s", ui_label(CommanderStatusLevelLabel));
    ui_status("A=Ship B=Quests C=Cargo START=Back");
    for (;;) {
        u16 joy = wait_button();
        int btn = 0;
        if (joy & BTN_A) btn = CommanderStatusShipButton;
        if (joy & BTN_B) btn = CommanderStatusQuestsButton;
        if (joy & BTN_C) btn = CommanderStatusSpecialButton;
        if (btn) { deliver_button(CommanderStatusFormHandleEvent, btn); if (_next_screen != SCREEN_NONE) break; }
        if (joy & BTN_START) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void screen_current_ship(void)
{
    _status_tabs(CurrentShipFormHandleEvent, CurrentShipForm, "CURRENT SHIP",
                 BTN_A, CurrentShipStatusButton, BTN_B, CurrentShipQuestsButton,
                 BTN_C, CurrentShipSpecialButton, "A=Status B=Quests C=Cargo START=Back");
}

static void screen_quests(void)
{
    _status_tabs(QuestsFormHandleEvent, QuestsForm, "QUESTS",
                 BTN_A, QuestsStatusButton, BTN_B, QuestsShipButton,
                 BTN_C, QuestsSpecialButton, "A=Status B=Ship C=Cargo START=Back");
}

static void screen_special_cargo(void)
{
    _status_tabs(SpecialCargoFormHandleEvent, SpecialCargoForm, "SPECIAL CARGO",
                 BTN_A, SpecialCargoStatusButton, BTN_B, SpecialCargoShipButton,
                 BTN_C, SpecialCargoQuestsButton, "A=Status B=Ship C=Quests START=Back");
}

/* -----------------------------------------------------------------------
 * AVERAGE PRICES
 * --------------------------------------------------------------------- */
static void _draw_avg_prices(void)
{
    int i;
    SOLARSYSTEM* ws = &SolarSystem[WarpSystem];

    ui_clear_body();
    ui_title("AVERAGE PRICES");

    ui_printf(0, UI_BODY_TOP, PAL_HILIGHT, "%-16s  %s",
        SolarSystemName[ws->NameIndex],
        ws->Visited ? SpecialResources[ws->SpecialResources] : "?");
    ui_printf(0, UI_BODY_TOP+1, PAL_NORMAL, "Tech: %-10s  Bays free: %d",
        TechLevel[ws->TechLevel],
        TotalCargoBays() - FilledCargoBays());

    for (i = 0; i < MAXTRADEITEM; i++) {
        long price = StandardPrice(
            (char)i,
            (char)ws->Size,
            (char)ws->TechLevel,
            (char)ws->Politics,
            ws->Visited ? (int)ws->SpecialResources : -1);
        int row = UI_BODY_TOP + 3 + i;
        if (PriceDifferences) {
            long diff = price - BuyPrice[i];
            int pal = (diff > 0 && BuyPrice[i] > 0) ? PAL_HILIGHT :
                      (diff < 0) ? PAL_DIM : PAL_NORMAL;
            if (price <= 0 || BuyPrice[i] <= 0)
                ui_printf(0, row, PAL_DIM, "%-12s  ---", Tradeitem[i].Name);
            else
                ui_printf(0, row, pal, "%-12s  %+5ldcr", Tradeitem[i].Name, diff);
        } else {
            int pal = (price > 0) ? PAL_NORMAL : PAL_DIM;
            if (price > 0)
                ui_printf(0, row, pal, "%-12s  %5ldcr", Tradeitem[i].Name, price);
            else
                ui_printf(0, row, PAL_DIM, "%-12s  ---", Tradeitem[i].Name);
        }
    }
    ui_status("A=Warp B=Chart C=Diff LR=Sys START=Info");
}

static void screen_average_prices(void)
{
    deliver_open(AveragePricesFormHandleEvent, AveragePricesForm);
    _draw_avg_prices();

    for (;;) {
        u16 joy = wait_button();
        int btn = 0;
        if (joy & BTN_A)     btn = AveragePricesWarpButton;
        if (joy & BTN_START) btn = AveragePricesSystemInfoButton;
        if (joy & BTN_C)     btn = PriceDifferences ? AveragePricesAbsolutePricesButton
                                                    : AveragePricesPriceDifferencesButton;
        if ((joy & BTN_RIGHT) && NextSystemWithinRange(WarpSystem, false) >= 0) btn = AveragePricesScrollFwdButton;
        if ((joy & BTN_LEFT) && NextSystemWithinRange(WarpSystem, true) >= 0)   btn = AveragePricesScrollBackButton;
        if (btn) {
            deliver_button(AveragePricesFormHandleEvent, btn);
            if (_next_screen != SCREEN_NONE) break;
            _draw_avg_prices();
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_WARP); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * NEWSPAPER SCREEN
 * --------------------------------------------------------------------- */
/* Newspaper screen uses a growable headline list rendered in ui_printf rows.
 * Headlines are collected into a fixed buffer then displayed with scrolling. */
#define MAX_HEADLINES 20
#define HL_LEN        40


static void screen_newspaper(void)
{
    ui_form_title[0] = '\0';
    ui_capture(2);
    deliver_open(NewspaperFormHandleEvent, NewspaperForm);
    ui_clear_body();
    ui_title(ui_form_title[0] ? ui_form_title : "NEWSPAPER");
    ui_capture_print(UI_BODY_TOP + 1, PAL_NORMAL);
    ui_capture(0);
    ui_status("A/B = Done");
    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_A | BTN_B | BTN_START)) {
            deliver_button(NewspaperFormHandleEvent, NewspaperDoneButton);
            if (_next_screen == SCREEN_NONE) ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }
    }
}

/* -----------------------------------------------------------------------
 * PERSONNEL ROSTER SCREEN
 * --------------------------------------------------------------------- */
/* Draw the personnel roster screen from globals */
static void _draw_personnel(void)
{
    ui_capture(2);
    deliver_open(PersonnelRosterFormHandleEvent, PersonnelRosterForm);
    /* the static labels of the Palm form */
    GEN_WinDrawChars("Crew 1:", 7, 0, 17);
    GEN_WinDrawChars("Crew 2:", 7, 0, 62);
    GEN_WinDrawChars("For hire:", 9, 0, 107);
    ui_clear_body();
    ui_title("PERSONNEL ROSTER");
    ui_capture_print(UI_BODY_TOP + 1, PAL_NORMAL);
    ui_capture(0);
    ui_printf(0, UI_BODY_BOT - 1, PAL_DIM, "Cash: %ld cr.", Credits);
    ui_status("A=Hire UP=Fire 1st DN=Fire 2nd B=Back");
}

static void screen_personnel(void)
{
    _draw_personnel();
    for (;;) {
        u16 joy = wait_button();
        int btn = 0;
        if ((joy & BTN_UP) && Ship.Crew[1] >= 0)   btn = PersonnelRosterFire0Button;
        if ((joy & BTN_DOWN) && Ship.Crew[2] >= 0) btn = PersonnelRosterFire0Button + 1;
        if ((joy & BTN_A) && GetForHire() >= 0)    btn = PersonnelRosterHire0Button;
        if (btn) {
            deliver_button(PersonnelRosterFormHandleEvent, btn);
            _draw_personnel();
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SYSTEM_INFO); break; }
        if (joy & BTN_START) {
            CurForm = PersonnelRosterForm;
            if (ui_menu()) break;
            _draw_personnel();
        }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * BUY SHIP / BUY EQUIPMENT / SELL EQUIPMENT - delegate to form handlers
 * --------------------------------------------------------------------- */
static void _draw_buy_ship(int sel)
{
    int i;
    DetermineShipPrices();
    ui_clear_body();
    ui_title("BUY SHIP");
    ui_printf(0, UI_BODY_TOP, PAL_NORMAL, "Credits: %ld cr   Current: %s",
              Credits, Shiptype[Ship.Type].Name);
    for (i = 0; i < MAXSHIPTYPE; i++) {
        int pal = (i == sel) ? PAL_HILIGHT :
                  (ShipPrice[i] > 0 && Ship.Type != i) ? PAL_NORMAL : PAL_DIM;
        const char* status = (Ship.Type == i) ? "owned" :
                             (ShipPrice[i] <= 0) ? "n/a" : "";
        if (ShipPrice[i] > 0 && Ship.Type != i)
            ui_printf(0, UI_BODY_TOP + 1 + i, pal,
                "%-14s %6ldcr  C%dW%dS%dG%d  %s",
                Shiptype[i].Name, ShipPrice[i],
                (int)Shiptype[i].CargoBays,
                (int)Shiptype[i].WeaponSlots,
                (int)Shiptype[i].ShieldSlots,
                (int)Shiptype[i].GadgetSlots, status);
        else
            ui_printf(0, UI_BODY_TOP + 1 + i, pal,
                "%-14s  %-8s", Shiptype[i].Name, status);
    }
    ui_status("UP/DN=select  A=buy  C=info  B=back");
}

static void screen_buy_ship(void)
{
    /* Deliver open so BuyShipFormHandleEvent can init state */
    deliver_open(BuyShipFormHandleEvent, BuyShipForm);
    int sel = (SelectedShipType >= 0 && SelectedShipType < MAXSHIPTYPE) ? SelectedShipType : 0;
    _draw_buy_ship(sel);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < MAXSHIPTYPE-1) sel++; _draw_buy_ship(sel); continue; }
        if (joy & BTN_UP)   { if (sel > 0)             sel--; _draw_buy_ship(sel); continue; }
        if (joy & BTN_C) {
            /* Info button: show ship spec via ShiptypeInfoForm */
            SelectedShipType = sel;
            ui_goto_screen(SCREEN_SHIPTYPE_INFO);
            break;
        }
        if (joy & BTN_A) {
            if (ShipPrice[sel] > 0 && Ship.Type != sel)
                deliver_button(BuyShipFormHandleEvent, BuyShipBuy0Button + sel);
            _draw_buy_ship(sel);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SHIPYARD); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void _draw_buy_equip(int sel)
{
    int i, row = UI_BODY_TOP, n = MAXWEAPONTYPE + MAXSHIELDTYPE + MAXGADGETTYPE;
    ui_clear_body();
    ui_title("BUY EQUIPMENT");
    ui_printf(0, row++, PAL_NORMAL, "Credits: %ld cr", Credits);
    for (i = 0; i < MAXWEAPONTYPE; i++, row++) {
        long p = BASEWEAPONPRICE(i);
        int pal = (i == sel) ? PAL_HILIGHT : (p > 0 ? PAL_NORMAL : PAL_DIM);
        ui_printf(0, row, pal, "%-16s %6ldcr", Weapontype[i].Name, p > 0 ? p : 0L);
    }
    for (i = 0; i < MAXSHIELDTYPE; i++, row++) {
        long p = BASESHIELDPRICE(i);
        int idx = MAXWEAPONTYPE + i;
        int pal = (idx == sel) ? PAL_HILIGHT : (p > 0 ? PAL_NORMAL : PAL_DIM);
        ui_printf(0, row, pal, "%-16s %6ldcr", Shieldtype[i].Name, p > 0 ? p : 0L);
    }
    for (i = 0; i < MAXGADGETTYPE; i++, row++) {
        long p = BASEGADGETPRICE(i);
        int idx = MAXWEAPONTYPE + MAXSHIELDTYPE + i;
        int pal = (idx == sel) ? PAL_HILIGHT : (p > 0 ? PAL_NORMAL : PAL_DIM);
        ui_printf(0, row, pal, "%-16s %6ldcr", Gadgettype[i].Name, p > 0 ? p : 0L);
    }
    (void)n;
    ui_status("UP/DN=item  A=buy  B=back");
}

static void screen_buy_equipment(void)
{
    deliver_open(BuyEquipmentFormHandleEvent, BuyEquipmentForm);
    int sel = 0, total = MAXWEAPONTYPE + MAXSHIELDTYPE + MAXGADGETTYPE;
    _draw_buy_equip(sel);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < total-1) sel++; _draw_buy_equip(sel); continue; }
        if (joy & BTN_UP)   { if (sel > 0)       sel--; _draw_buy_equip(sel); continue; }
        if (joy & BTN_A) {
            deliver_button(BuyEquipmentFormHandleEvent, BuyEquipmentBuy0Button + sel);
            _draw_buy_equip(sel);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SHIPYARD); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

static void _draw_sell_equip(int sel)
{
    int i, row = UI_BODY_TOP, slot = 0;
    ui_clear_body();
    ui_title("SELL EQUIPMENT");
    ui_printf(0, row++, PAL_NORMAL, "Credits: %ld cr", Credits);

    for (i = 0; i < MAXWEAPON; i++, slot++) {
        int pal = (slot == sel) ? PAL_HILIGHT : (Ship.Weapon[i] >= 0 ? PAL_NORMAL : PAL_DIM);
        if (Ship.Weapon[i] >= 0)
            ui_printf(0, UI_BODY_TOP + 1 + slot, pal,
                "Wpn: %-14s %6ldcr",
                Weapontype[Ship.Weapon[i]].Name, WEAPONSELLPRICE(i));
        else
            ui_printf(0, UI_BODY_TOP + 1 + slot, PAL_DIM, "Wpn slot %d: empty", i);
    }
    for (i = 0; i < MAXSHIELD; i++, slot++) {
        int pal = (slot == sel) ? PAL_HILIGHT : (Ship.Shield[i] >= 0 ? PAL_NORMAL : PAL_DIM);
        if (Ship.Shield[i] >= 0)
            ui_printf(0, UI_BODY_TOP + 1 + slot, pal,
                "Shd: %-14s %6ldcr",
                Shieldtype[Ship.Shield[i]].Name, SHIELDSELLPRICE(i));
        else
            ui_printf(0, UI_BODY_TOP + 1 + slot, PAL_DIM, "Shield slot %d: empty", i);
    }
    for (i = 0; i < MAXGADGET; i++, slot++) {
        int pal = (slot == sel) ? PAL_HILIGHT : (Ship.Gadget[i] >= 0 ? PAL_NORMAL : PAL_DIM);
        if (Ship.Gadget[i] >= 0)
            ui_printf(0, UI_BODY_TOP + 1 + slot, pal,
                "Gad: %-14s %6ldcr",
                Gadgettype[Ship.Gadget[i]].Name, GADGETSELLPRICE(i));
        else
            ui_printf(0, UI_BODY_TOP + 1 + slot, PAL_DIM, "Gadget slot %d: empty", i);
    }
    ui_status("UP/DN=item  A=sell  B=back");
}

static void screen_sell_equipment(void)
{
    deliver_open(SellEquipmentFormHandleEvent, SellEquipmentForm);
    int sel = 0, total = MAXWEAPON + MAXSHIELD + MAXGADGET;
    _draw_sell_equip(sel);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_DOWN) { if (sel < total-1) sel++; _draw_sell_equip(sel); continue; }
        if (joy & BTN_UP)   { if (sel > 0)       sel--; _draw_sell_equip(sel); continue; }
        if (joy & BTN_A) {
            /* Determine if this slot is occupied */
            int has_item = 0;
            if (sel < MAXWEAPON)                         has_item = Ship.Weapon[sel] >= 0;
            else if (sel < MAXWEAPON + MAXSHIELD)        has_item = Ship.Shield[sel - MAXWEAPON] >= 0;
            else                                          has_item = Ship.Gadget[sel - MAXWEAPON - MAXSHIELD] >= 0;
            if (has_item)
                deliver_button(SellEquipmentFormHandleEvent, SellEquipmentSell0Button + sel);
            _draw_sell_equip(sel);
        }
        if (joy & BTN_B) { ui_goto_screen(SCREEN_SHIPYARD); break; }
        if (_next_screen != SCREEN_NONE) break;
    }
}

/* -----------------------------------------------------------------------
 * SPECIAL EVENT SCREEN
 * --------------------------------------------------------------------- */
static void screen_special_event(void)
{
    int sp = CURSYSTEM.Special;
    Boolean just_msg = SpecialEvent[sp].JustAMessage;

    ui_capture(2);
    deliver_open(SpecialEventFormHandleEvent, SpecialEventForm);
    ui_clear_body();
    ui_title(ui_form_title[0] ? ui_form_title : "Special Event");
    ui_wrap(ui_page, UI_BODY_TOP + 1, PAL_NORMAL);
    ui_capture(0);
    ui_status(just_msg ? "A = OK" : "A = Yes     B = No");
    snd_play(SND_INFO);

    for (;;) {
        u16 joy = wait_button();
        if (joy & BTN_A)
            deliver_button(SpecialEventFormHandleEvent, just_msg ? SpecialEventOKButton : SpecialEventYesButton);
        else if ((joy & BTN_B) && !just_msg)
            deliver_button(SpecialEventFormHandleEvent, SpecialEventNoButton);
        else
            continue;
        if (_next_screen == SCREEN_NONE) ui_goto_screen(SCREEN_SYSTEM_INFO);
        break;
    }
}

/* -----------------------------------------------------------------------
 * SPECIAL CARGO SCREEN
 * --------------------------------------------------------------------- */

/* -----------------------------------------------------------------------
 * DESTROYED / RETIRE / UTOPIA screens
 * --------------------------------------------------------------------- */
/* Deliver a key press to a form handler and wait for it to set _next_screen */
static void _endgame_screen(const char* title, int pic,
                             form_handler_t handler, int formID)
{
    EventType ev;
    deliver_open(handler, formID);

    ui_clear_screen();
    ui_title(title);
    gfx_picture(pic, 10, 3);
    ui_printf(1, 24, PAL_NORMAL, "Commander %s", NameCommander);
    ui_printf(1, 25, PAL_NORMAL, "Days: %d   Worth: %ld cr", (int)Days, CurrentWorth());
    ui_status("A = Continue");
    snd_play(pic == PIC_DESTROYED ? SND_ERROR : SND_CONFIRM);

    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_A | BTN_START)) {
            gfx_reset();
            ui_clear_screen();
            memset(&ev, 0, sizeof(ev));
            ev.eType = keyDownEvent;
            ev.data.keyDown.chr = chrReturn;
            handler(&ev);
            if (_next_screen == SCREEN_NONE) ui_goto_screen(SCREEN_MAIN);
            break;
        }
    }
}

static void screen_destroyed(void)
{
    _endgame_screen("YOUR SHIP HAS BEEN DESTROYED", PIC_DESTROYED,
        DestroyedFormHandleEvent, DestroyedForm);
}

static void screen_retire(void)
{
    _endgame_screen("YOU HAVE RETIRED", PIC_RETIRE,
        RetireFormHandleEvent, RetireForm);
}

static void screen_utopia(void)
{
    _endgame_screen("YOU RETIRE TO YOUR OWN MOON", PIC_UTOPIA,
        UtopiaFormHandleEvent, UtopiaForm);
}

/* -----------------------------------------------------------------------
 * MAIN FORM - the title picture; shown after a game ended. Any button
 * starts a new game, exactly like the Palm MainForm.
 * --------------------------------------------------------------------- */
static void screen_main(void)
{
    extern void sram_clear_slot(int slot);
    sram_clear_slot(0);                 /* the finished game is gone */
    gfx_picture(PIC_TITLE, 10, 1);
    ui_printf(2, 23, PAL_NORMAL, "Space Trader 1.2.2 for Sega Genesis");
    ui_status("A / START = New Game");
    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_A | BTN_START)) break;
    }
    gfx_reset();
    ui_clear_screen();
    StartNewGame();
    if (_next_screen == SCREEN_NONE) ui_goto_screen(SCREEN_NEW_COMMANDER);
}

/* -----------------------------------------------------------------------
 * SHIPTYPE INFORMATION (ship specs + the original ship bitmap)
 * --------------------------------------------------------------------- */
static void screen_shiptype_info(void)
{
    int t = SelectedShipType;
    ui_title("SHIP INFORMATION");
    ui_printf(1, 3, PAL_HILIGHT, "%s", Shiptype[t].Name);
    ui_printf(1, 5,  PAL_NORMAL, "Size:           %s", SystemSize[Shiptype[t].Size]);
    ui_printf(1, 6,  PAL_NORMAL, "Cargo bays:     %d", (int)Shiptype[t].CargoBays);
    ui_printf(1, 7,  PAL_NORMAL, "Weapon slots:   %d", (int)Shiptype[t].WeaponSlots);
    ui_printf(1, 8,  PAL_NORMAL, "Shield slots:   %d", (int)Shiptype[t].ShieldSlots);
    ui_printf(1, 9,  PAL_NORMAL, "Gadget slots:   %d", (int)Shiptype[t].GadgetSlots);
    ui_printf(1, 10, PAL_NORMAL, "Crew quarters:  %d", (int)Shiptype[t].CrewQuarters);
    ui_printf(1, 11, PAL_NORMAL, "Maximum range:  %d parsecs", (int)Shiptype[t].FuelTanks);
    ui_printf(1, 12, PAL_NORMAL, "Hull strength:  %d", (int)Shiptype[t].HullStrength);
    gfx_set_pal(PAL1, gfx_ship_pal[t]);
    gfx_ship_draw(GFX_TILE_BASE, t, 1, 1, 0, 0);
    gfx_place(PAL1, GFX_TILE_BASE, 12, 14, 16, 13);
    ui_status("A/B = Back");
    for (;;) {
        u16 joy = wait_button();
        if (joy & (BTN_A | BTN_B | BTN_START)) break;
    }
    CurForm = BuyShipForm;
    ui_goto_screen(SCREEN_BUY_SHIP);
}

/* -----------------------------------------------------------------------
 * NEW COMMANDER SCREEN
 *
 * Mirrors NewCommanderFormHandleEvent logic:
 *   - B cycles through preset names
 *   - Up/Down selects which field (Difficulty, Pilot, Fighter, Trader, Engineer)
 *   - Left/Right adjusts the selected field
 *   - A confirms — only when all 2*MAXSKILL points are allocated
 *
 * On confirm: DeterminePrices, optional lottery, then SCREEN_SYSTEM_INFO.
 * --------------------------------------------------------------------- */
static void screen_new_commander(void)
{
    static const char* preset_names[] = {
        "Jameson","Shelby","Morgan","Blake","Quinn",
        "Reyes","Hayashi","Novak","Cruz","Okafor"
    };
    static const int N_NAMES = 10;

    /* Field indices */
    enum { F_DIFF=0, F_PILOT, F_FIGHT, F_TRADE, F_ENG, F_COUNT };

    int name_idx = 0;
    int field    = F_PILOT;   /* start on Pilot */

    /* Make sure name is initialised */
    if (NameCommander[0] == '\0')
        strncpy(NameCommander, preset_names[0], NAMELEN);

    /* Find which preset matches current name (if any) */
    {
        int i;
        for (i = 0; i < N_NAMES; i++)
            if (strcmp(NameCommander, preset_names[i]) == 0)
                { name_idx = i; break; }
    }

    #define _REMAINING() \
        (2*MAXSKILL - COMMANDER.Pilot - COMMANDER.Fighter - \
                      COMMANDER.Trader - COMMANDER.Engineer)

    #define _DRAW_NCMD() do { \
        int rem = _REMAINING(); \
        ui_clear_body(); \
        ui_title("NEW COMMANDER"); \
        ui_printf(0, UI_BODY_TOP+0, PAL_NORMAL,  "Name      : %-20s", NameCommander); \
        ui_printf(0, UI_BODY_TOP+1, PAL_DIM,     "(B = preset name, C = type a name)"); \
        ui_printf(0, UI_BODY_TOP+3, \
            field==F_DIFF ? PAL_HILIGHT : PAL_NORMAL, \
            "Difficulty: %-10s   %s", DifficultyLevel[Difficulty], \
            field==F_DIFF ? "<" : ""); \
        ui_printf(0, UI_BODY_TOP+4, \
            rem>0 ? PAL_WARN : PAL_NORMAL, \
            "Pts left  : %d  ", rem); \
        ui_printf(0, UI_BODY_TOP+5, \
            field==F_PILOT ? PAL_HILIGHT : PAL_NORMAL, \
            "Pilot     : %d   %s", (int)COMMANDER.Pilot, field==F_PILOT?"<":""); \
        ui_printf(0, UI_BODY_TOP+6, \
            field==F_FIGHT ? PAL_HILIGHT : PAL_NORMAL, \
            "Fighter   : %d   %s", (int)COMMANDER.Fighter, field==F_FIGHT?"<":""); \
        ui_printf(0, UI_BODY_TOP+7, \
            field==F_TRADE ? PAL_HILIGHT : PAL_NORMAL, \
            "Trader    : %d   %s", (int)COMMANDER.Trader, field==F_TRADE?"<":""); \
        ui_printf(0, UI_BODY_TOP+8, \
            field==F_ENG ? PAL_HILIGHT : PAL_NORMAL, \
            "Engineer  : %d   %s", (int)COMMANDER.Engineer, field==F_ENG?"<":""); \
        if (rem == 0) \
            ui_status("A=Start B=Name UP/DN=field LR=pts"); \
        else \
            ui_status("UP/DN=field LR=pts  B=name  A=start"); \
    } while(0)

    _DRAW_NCMD();

    for (;;) {
        u16 joy = wait_button();

        /* Cycle name with B */
        if (joy & BTN_B) {
            name_idx = (name_idx + 1) % N_NAMES;
            strncpy(NameCommander, preset_names[name_idx], NAMELEN);
            NameCommander[NAMELEN] = '\0';
            _DRAW_NCMD();
            continue;
        }

        if (joy & BTN_C) {
            char nm[NAMELEN + 1];
            strncpy(nm, NameCommander, NAMELEN); nm[NAMELEN] = '\0';
            if (ui_text_input("COMMANDER NAME", nm, NAMELEN) && nm[0]) {
                strncpy(NameCommander, nm, NAMELEN);
                NameCommander[NAMELEN] = '\0';
            }
            _DRAW_NCMD();
            continue;
        }

        /* Navigate fields */
        if (joy & BTN_DOWN) {
            if (field < F_COUNT - 1) field++;
            _DRAW_NCMD();
            continue;
        }
        if (joy & BTN_UP) {
            if (field > 0) field--;
            _DRAW_NCMD();
            continue;
        }

        /* Adjust selected field */
        if (joy & (BTN_LEFT | BTN_RIGHT)) {
            int inc = (joy & BTN_RIGHT) ? 1 : -1;
            switch (field) {
                case F_DIFF:
                    Difficulty = (Byte)(Difficulty + inc);
                    if ((int)Difficulty >= MAXDIFFICULTY) Difficulty = MAXDIFFICULTY - 1;
                    break;
                case F_PILOT:
                    if (inc < 0 && COMMANDER.Pilot > 1)    COMMANDER.Pilot--;
                    if (inc > 0 && COMMANDER.Pilot < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Pilot++;
                    break;
                case F_FIGHT:
                    if (inc < 0 && COMMANDER.Fighter > 1)  COMMANDER.Fighter--;
                    if (inc > 0 && COMMANDER.Fighter < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Fighter++;
                    break;
                case F_TRADE:
                    if (inc < 0 && COMMANDER.Trader > 1)   COMMANDER.Trader--;
                    if (inc > 0 && COMMANDER.Trader < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Trader++;
                    break;
                case F_ENG:
                    if (inc < 0 && COMMANDER.Engineer > 1) COMMANDER.Engineer--;
                    if (inc > 0 && COMMANDER.Engineer < MAXSKILL && _REMAINING() > 0)
                        COMMANDER.Engineer++;
                    break;
            }
            _DRAW_NCMD();
            continue;
        }

        /* Confirm with A */
        if (joy & BTN_A) {
            if (_REMAINING() > 0) {
                /* Flash warning — can't start until all points are spent */
                ui_printf(0, UI_BODY_TOP + 10, PAL_WARN,
                    "Spend %d more skill point(s)!", _REMAINING());
                continue;
            }
            /* Replicate NewCommanderFormHandleEvent OK logic */
                    DeterminePrices(COMMANDER.CurSystem);
            if (Difficulty < NORMAL)
                if (CURSYSTEM.Special < 0)
                    CURSYSTEM.Special = LOTTERYWINNER;
            ui_goto_screen(SCREEN_SYSTEM_INFO);
            break;
        }

        if (_next_screen != SCREEN_NONE) break;
    }

    #undef _REMAINING
    #undef _DRAW_NCMD
}

/* -----------------------------------------------------------------------
 * Palm form ID -> ScreenID converter
 * Called by the FrmGotoForm macro so existing game code works unchanged.
 * --------------------------------------------------------------------- */
void palm_form_to_screen(int formID)
{
    ScreenID sid = SCREEN_NONE;

    switch (formID) {
        case SystemInformationForm:  sid = SCREEN_SYSTEM_INFO;      break;
        case WarpForm:               sid = SCREEN_WARP;             break;
        case ExecuteWarpForm:        sid = SCREEN_EXECUTE_WARP;     break;
        case GalacticChartForm:      sid = SCREEN_GALACTIC_CHART;   break;
        case BuyCargoForm:           sid = SCREEN_BUY_CARGO;        break;
        case SellCargoForm:          sid = SCREEN_SELL_CARGO;       break;
        case DumpCargoForm:       sid = SCREEN_DUMP_CARGO;    break;
        case ShipYardForm:           sid = SCREEN_SHIPYARD;         break;
        case BuyShipForm:            sid = SCREEN_BUY_SHIP;         break;
        case ShiptypeInfoForm:       sid = SCREEN_SHIPTYPE_INFO;    break;
        case BuyEquipmentForm:       sid = SCREEN_BUY_EQUIPMENT;    break;
        case SellEquipmentForm:      sid = SCREEN_SELL_EQUIPMENT;   break;
        case PersonnelRosterForm:    sid = SCREEN_PERSONNEL;        break;
        case BankForm:               sid = SCREEN_BANK;             break;
        case CommanderStatusForm:    sid = SCREEN_COMMANDER_STATUS; break;
        case CurrentShipForm:        sid = SCREEN_CURRENT_SHIP;     break;
        case QuestsForm:             sid = SCREEN_QUESTS;           break;
        case AveragePricesForm:      sid = SCREEN_AVERAGE_PRICES;   break;
        case EncounterForm:          sid = SCREEN_ENCOUNTER;        break;
        case PlunderForm:            sid = SCREEN_PLUNDER;          break;
        case SpecialEventForm:       sid = SCREEN_SPECIAL_EVENT;    break;
        case SpecialCargoForm:       sid = SCREEN_SPECIAL_CARGO;    break;
        case NewspaperForm:          sid = SCREEN_NEWSPAPER;        break;
        case DestroyedForm:          sid = SCREEN_DESTROYED;        break;
        case RetireForm:             sid = SCREEN_RETIRE;           break;
        case UtopiaForm:             sid = SCREEN_UTOPIA;           break;
        case NewCommanderForm:       sid = SCREEN_NEW_COMMANDER;    break;
        case MainForm:               sid = SCREEN_MAIN;             break;
        default:
            kprintf("palm_form_to_screen: unknown formID=%d", formID);
            return;
    }
    kprintf("palm_form_to_screen: %d -> screen %d", formID, (int)sid);
    CurForm = formID;   /* keep CurForm in sync for AppHandleEvent menu logic */
    ui_goto_screen(sid);
}

