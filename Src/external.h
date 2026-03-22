/**
 * external.h  – Global variable declarations (ported from Palm OS version)
 * Replaces Palm OS includes with our Genesis compatibility layer.
 */
#ifndef EXTERNAL_H
#define EXTERNAL_H

#include "palmcompat.h"
#include "MerchantRsc.h"
#include "MerchantGraphics.h"
#include "spacetrader.h"

/* Global Variables (defined in Global.c) */
extern long Credits;
extern long Debt;
extern long PoliceRecordScore;
extern long ReputationScore;
extern long PoliceKills;
extern long TraderKills;
extern long PirateKills;
extern long SellPrice[];
extern long BuyPrice[];
extern long BuyingPrice[];
extern long ShipPrice[];
extern long MonsterHull;
extern unsigned long GalacticChartUpdateTicks;

extern int CheatCounter;
extern int Days;
extern int CurForm;
extern int EncounterType;
extern int WarpSystem;
extern int NoClaim;
extern int SelectedShipType;
extern int LeaveEmpty;
extern int GalacticChartSystem;
extern int NewsSpecialEventCount;
extern int TrackedSystem;

extern Boolean AutoFuel;
extern Boolean AutoRepair;
extern Boolean Clicks;
extern Boolean Raided;
extern Boolean MoonBought;
extern Boolean EscapePod;
extern Boolean Insurance;
extern Boolean Inspected;
extern Boolean AlwaysIgnoreTraders;
extern Boolean AlwaysIgnorePolice;
extern Boolean AlwaysIgnorePirates;
extern Boolean AlwaysIgnoreTradeInOrbit;
extern Boolean AlreadyPaidForNewspaper;
extern Boolean ArtifactOnBoard;
extern Boolean ReserveMoney;
extern Boolean PriceDifferences;
extern Boolean APLscreen;
extern Boolean TribbleMessage;
extern Boolean AlwaysInfo;
extern Boolean Continuous;
extern Boolean AttackFleeing;
extern Boolean LitterWarning;
extern Boolean SharePreferences;
extern Boolean IdentifyStartup;
extern Boolean RectangularButtonsOn;
extern Boolean GameLoaded;
extern Boolean AutoAttack;
extern Boolean AutoFlee;
extern Boolean AttackIconStatus;

extern Byte Difficulty;
extern Byte MonsterStatus;
extern Byte DragonflyStatus;
extern Byte JaporiDiseaseStatus;
extern Byte JarekStatus;
extern Byte InvasionStatus;
extern Byte ExperimentAndWildStatus;
extern Byte FabricRipProbability;
extern Byte VeryRareEncounter;
extern Byte BooleanCollection;
extern Byte ReactorStatus;
extern Byte ScarabStatus;
extern Byte VersionMajor;
extern Byte VersionMinor;

extern char NameCommander[];

extern SHIP Ship;
extern SHIP Opponent;
extern CREWMEMBER Mercenary[];
extern SOLARSYSTEM SolarSystem[];
extern Byte Wormhole[];
extern HIGHSCORE Hscores[];

extern int Shortcut1, Shortcut2, Shortcut3, Shortcut4;

extern char SBuf[];
extern char SBuf2[];

extern uint32_t romVersion;

/* MERCENARYHIREPRICE already defined in spacetrader.h */


/* -----------------------------------------------------------------------
 * Additional globals defined in Global.c – missing from original external.h
 * --------------------------------------------------------------------- */

/* Constant game data arrays (const, defined in Global.c) */
extern const POLICERECORD  PoliceRecord[MAXPOLICERECORD];
extern const REPUTATION    Reputation[MAXREPUTATION];
extern const char*         DifficultyLevel[MAXDIFFICULTY];
extern const char*         SpecialResources[MAXRESOURCES];
extern const POLITICS      Politics[MAXPOLITICS];
extern const TRADEITEM     Tradeitem[MAXTRADEITEM];
extern const WEAPON        Weapontype[MAXWEAPONTYPE+EXTRAWEAPONS];
extern const SHIELD        Shieldtype[MAXSHIELDTYPE+EXTRASHIELDS];
extern const GADGET        Gadgettype[MAXGADGETTYPE+EXTRAGADGETS];
extern const SHIPTYPE      Shiptype[MAXSHIPTYPE+EXTRASHIPS];
extern char*               MercenaryName[MAXCREWMEMBER];
extern const char*         Shortcuts[11];
extern       int           ShortcutTarget[];

/* Extra boolean globals referenced across files */
extern Boolean TextualEncounters;
extern Boolean UseHWButtons;
extern Boolean NewsAutoPay;
extern Boolean ShowTrackedRange;
extern Boolean JustLootedMarie;
extern Boolean ArrivedViaWormhole;
extern Boolean TrackAutoOff;
extern Boolean RemindLoans;
extern Boolean CanSuperWarp;
extern Boolean PossibleToGoThroughRip;

/* Extra byte globals */
extern Byte ExperimentStatus;
extern Byte WildStatus;

/* Chance globals */
extern int ChanceOfVeryRareEncounter;
extern int ChanceOfTradeInOrbit;

/* Bitmap pointers (all NULL on Genesis – bitmaps not used) */
extern Handle    SystemBmp;
extern Handle    CurrentSystemBmp;
extern Handle    ShortRangeSystemBmp;
extern Handle    WormholeBmp;
extern Handle    SmallWormholeBmp;
extern Handle    VisitedSystemBmp;
extern Handle    CurrentVisitedSystemBmp;
extern Handle    VisitedShortRangeSystemBmp;
extern Handle    ShipBmp[];
extern Handle    DamagedShipBmp[];
extern Handle    ShieldedShipBmp[];
extern Handle    DamagedShieldedShipBmp[];
extern Handle    IconBmp[];

extern BitmapPtr SystemBmpPtr;
extern BitmapPtr CurrentSystemBmpPtr;
extern BitmapPtr ShortRangeSystemBmpPtr;
extern BitmapPtr WormholeBmpPtr;
extern BitmapPtr SmallWormholeBmpPtr;
extern BitmapPtr VisitedSystemBmpPtr;
extern BitmapPtr CurrentVisitedSystemBmpPtr;
extern BitmapPtr VisitedShortRangeSystemBmpPtr;
extern BitmapPtr ShipBmpPtr[];
extern BitmapPtr DamagedShipBmpPtr[];
extern BitmapPtr ShieldedShipBmpPtr[];
extern BitmapPtr DamagedShieldedShipBmpPtr[];
extern BitmapPtr IconBmpPtr[];

/* Name handle (used in NewCommanderForm – stub on Genesis) */
extern Handle NameH;


/* -----------------------------------------------------------------------
 * Additional global arrays and variables defined in Global.c
 * (declared extern so all translation units can access them)
 * --------------------------------------------------------------------- */
extern const POLICERECORD PoliceRecord[];
extern const REPUTATION   Reputation[];
extern const SHIPTYPE     Shiptype[];
extern const TRADEITEM    Tradeitem[];
extern const WEAPON       Weapontype[];
extern const SHIELD       Shieldtype[];
extern const GADGET       Gadgettype[];
extern const POLITICS     Politics[];

extern char* MercenaryName[];   /* non-const: Global.c defines char* array */
extern const char* DifficultyLevel[];
extern const char* SpecialResources[];
extern const char* Shortcuts[];
extern int         ShortcutTarget[];

/* Bitmap handles (NULL on Genesis – text mode only) */
extern Handle    SystemBmp;
extern Handle    CurrentSystemBmp;
extern Handle    ShortRangeSystemBmp;
extern Handle    WormholeBmp;
extern Handle    SmallWormholeBmp;
extern Handle    VisitedSystemBmp;
extern Handle    CurrentVisitedSystemBmp;
extern Handle    VisitedShortRangeSystemBmp;
extern Handle    ShipBmp[];
extern Handle    DamagedShipBmp[];
extern Handle    ShieldedShipBmp[];
extern Handle    DamagedShieldedShipBmp[];
extern Handle    IconBmp[];
extern Handle    NameH;

extern BitmapPtr SystemBmpPtr;
extern BitmapPtr CurrentSystemBmpPtr;
extern BitmapPtr ShortRangeSystemBmpPtr;
extern BitmapPtr WormholeBmpPtr;
extern BitmapPtr SmallWormholeBmpPtr;
extern BitmapPtr VisitedSystemBmpPtr;
extern BitmapPtr CurrentVisitedSystemBmpPtr;
extern BitmapPtr VisitedShortRangeSystemBmpPtr;
extern BitmapPtr ShipBmpPtr[];
extern BitmapPtr DamagedShipBmpPtr[];
extern BitmapPtr ShieldedShipBmpPtr[];
extern BitmapPtr DamagedShieldedShipBmpPtr[];
extern BitmapPtr IconBmpPtr[];

/* Additional boolean globals defined in Global.c */
extern Boolean TextualEncounters;
extern Boolean AutoAttack;
extern Boolean AutoFlee;
extern Boolean AttackIconStatus;
extern Boolean AttackFleeing;
extern Boolean Continuous;
extern Boolean PossibleToGoThroughRip;
extern Boolean UseHWButtons;
extern Boolean NewsAutoPay;
extern Boolean ShowTrackedRange;
extern Boolean JustLootedMarie;
extern Boolean ArrivedViaWormhole;
extern Boolean AlreadyPaidForNewspaper;
extern Boolean TrackAutoOff;
extern Boolean RemindLoans;
extern Boolean CanSuperWarp;

/* Additional Byte globals */
extern Byte ExperimentStatus;
extern Byte WildStatus;

/* Additional int globals */
extern int ChanceOfVeryRareEncounter;
extern int ChanceOfTradeInOrbit;


extern SPECIALEVENT SpecialEvent[];  /* non-const: Traveler.c assigns Occurrence */

/* Global string arrays for system info display (defined in Global.c) */
extern const char* TechLevel[];
extern const char* Activity[];
extern const char* Status[];
extern const char* SystemSize[];

/* SpaceMonster – special enemy SHIP instance */
extern SHIP SpaceMonster;
extern SHIP Scarab;
extern SHIP Dragonfly;

/* SRAM full save/load – defined in ui.c, SAVEGAMETYPE needed */
extern void    sram_save_full(SAVEGAMETYPE* sg);
extern void    sram_load_full(SAVEGAMETYPE* sg);

#endif /* EXTERNAL_H */

/* -----------------------------------------------------------------------
 * Additional global arrays & variables defined in Global.c
 * (not declared in the original external.h – added for Genesis port)
 * --------------------------------------------------------------------- */

/* Game data tables (const arrays defined in Global.c) */
extern const POLICERECORD  PoliceRecord[MAXPOLICERECORD];
extern const REPUTATION    Reputation[MAXREPUTATION];
extern const char*         DifficultyLevel[MAXDIFFICULTY];
extern const char*         SpecialResources[MAXRESOURCES];
extern const char*         SizeName[6];
extern const char*         TechLevelName[MAXTECHLEVEL+1];
extern const char*         PoliticsName[MAXPOLITICS];
extern const POLITICS      Politics[MAXPOLITICS];
extern const TRADEITEM     Tradeitem[MAXTRADEITEM];
extern const SHIPTYPE      Shiptype[MAXSHIPTYPE+EXTRASHIPS];
extern const WEAPON        Weapontype[MAXWEAPONTYPE+EXTRAWEAPONS];
extern const SHIELD        Shieldtype[MAXSHIELDTYPE+EXTRASHIELDS];
extern const GADGET        Gadgettype[MAXGADGETTYPE+EXTRAGADGETS];
extern char*               MercenaryName[MAXCREWMEMBER];
extern const char*         Shortcuts[11];
extern int                 ShortcutTarget[11];


/* Bitmap pointers (all NULL on Genesis – graphics are tile-based) */
extern void* SystemBmp;
extern void* CurrentSystemBmp;
extern void* ShortRangeSystemBmp;
extern void* WormholeBmp;
extern void* SmallWormholeBmp;
extern void* VisitedSystemBmp;
extern void* CurrentVisitedSystemBmp;
extern void* VisitedShortRangeSystemBmp;
extern void* SystemBmpPtr;
extern void* CurrentSystemBmpPtr;
extern void* ShortRangeSystemBmpPtr;
extern void* WormholeBmpPtr;
extern void* SmallWormholeBmpPtr;
extern void* VisitedSystemBmpPtr;
extern void* CurrentVisitedSystemBmpPtr;
extern void* VisitedShortRangeSystemBmpPtr;
extern void* ShipBmp[MAXSHIPTYPE+EXTRASHIPS];
extern void* DamagedShipBmp[MAXSHIPTYPE+EXTRASHIPS];
extern void* ShieldedShipBmp[MAXSHIPTYPE+EXTRASHIPS];
extern void* DamagedShieldedShipBmp[MAXSHIPTYPE+EXTRASHIPS];
extern void* IconBmp[5];
extern void* ShipBmpPtr[MAXSHIPTYPE+EXTRASHIPS];
extern void* DamagedShipBmpPtr[MAXSHIPTYPE+EXTRASHIPS];
extern void* ShieldedShipBmpPtr[MAXSHIPTYPE+EXTRASHIPS];
extern void* DamagedShieldedShipBmpPtr[MAXSHIPTYPE+EXTRASHIPS];
extern void* IconBmpPtr[5];
extern void* NameH;

/* Additional boolean globals defined in Global.c */
extern Boolean TextualEncounters;
extern Boolean UseHWButtons;
extern Boolean NewsAutoPay;
extern Boolean ShowTrackedRange;
extern Boolean JustLootedMarie;
extern Boolean ArrivedViaWormhole;
extern Boolean TrackAutoOff;
extern Boolean RemindLoans;
extern Boolean CanSuperWarp;
extern Boolean PossibleToGoThroughRip;

/* Additional byte globals */
extern Byte ExperimentStatus;
extern Byte WildStatus;

/* Encounter-related globals */
extern int ChanceOfVeryRareEncounter;
extern int ChanceOfTradeInOrbit;

/* Additional const arrays defined in Global.c */
extern const char* SolarSystemName[MAXSOLARSYSTEM];
extern const char* SystemSize[MAXSIZE];
extern int         ChanceOfVeryRareEncounter;
extern int         ChanceOfTradeInOrbit;
