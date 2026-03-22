/***********************************************************************
 *
 * SPACE TRADER 1.2.0
 *
 * Merchant.c
 *
 * Copyright (C) 2000-2002 Pieter Spronck, All Rights Reserved
 *
 * Additional coding by Sam Anderson (rulez2@home.com)
 * Additional coding by Samuel Goldstein (palm@fogbound.net)
 *
 * Some code of Matt Lee's Dope Wars program has been used.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 * You can contact the author at space_trader@hotmail.com
 *
 * For those who are familiar with the classic game Elite: many of the
 * ideas in Space Trader are heavily inspired by Elite.
 *
 **********************************************************************/

/* Genesis port: bitmap internals access not needed (BELOW40 is always false) */
#include "external.h"
#include "compat.h"
#include "ui.h"

#define ourMinVersion	sysMakeROMVersion(2,0,0,sysROMStageRelease,0)
void UnpackBooleans(Byte _val, Boolean *_a, Boolean *_b, Boolean *_c, Boolean *_d,
				  Boolean *_e, Boolean *_f, Boolean *_g, Boolean *_h);
Byte PackBooleans(Boolean _a, Boolean _b, Boolean _c, Boolean _d,
				  Boolean _e, Boolean _f, Boolean _g, Boolean _h);
Byte PackBytes(Byte _a, Byte _b);
void UnpackBytes(Byte _val, Byte *_a, Byte *_b);

#define BELOW35				(romVersion < sysMakeROMVersion( 3, 5, 0, sysROMStageRelease, 0 ))				
#define BELOW40				(romVersion < sysMakeROMVersion( 4, 0, 0, sysROMStageRelease, 0 ))				

/* SAVEGAMENAME not needed on Genesis (single SRAM slot) */
/* SWITCHGAMENAME not needed on Genesis */

// *************************************************************************
// Get width of bitmap
// *************************************************************************
int GetBitmapWidth( BitmapPtr BmpPtr )
{
	Coord W;

	if (BELOW40)
		return ((BitmapHeaderType*)BmpPtr)->width;
	else
	{
		BmpGetDimensions( BmpPtr, &W, 0, 0 );
		return W;
	}
}

// *************************************************************************
// Get heigth of bitmap
// *************************************************************************
int GetBitmapHeight( BitmapPtr BmpPtr )
{
	Coord H;

	if (BELOW40)
		return ((BitmapHeaderType*)BmpPtr)->height;
	else
	{
		BmpGetDimensions( BmpPtr, 0, &H, 0 );
		return H;
	}
}
// *************************************************************************
/* Genesis port: graphics support check replaced by no-op stubs */
static Err GraphicsSupport( void )   { return 0; }
static void EndGraphicsSupport( void ) { }


// *************************************************************************
// Determines if the ROM version is compatible
// *************************************************************************
/* Genesis port: ROM version check always passes */
Err RomVersionCompatible(DWord requiredVersion, Word launchFlags)
{
    (void)requiredVersion; (void)launchFlags;
    return 0;
}


// *************************************************************************
// Determines if the software is outdated
// *************************************************************************
/* Genesis port: no beta expiry check */
Err OutdatedSoftware( void ) { return 0; }

// Fills global variables
static void FillGlobals( SAVEGAMETYPE* sg, int State )
{
  	int i;
	Boolean OldVersion = false;
	Boolean Trash;

	if (sg->VersionMinor < 4)
		OldVersion = true;
		
	Credits = sg->Credits;
	Debt = sg->Debt;
	Days = sg->Days;
	WarpSystem = sg->WarpSystem;
	SelectedShipType = sg->SelectedShipType;
	for (i=0; i<MAXTRADEITEM; ++i)
	{
		BuyPrice[i] = sg->BuyPrice[i];
		SellPrice[i] = sg->SellPrice[i];
	}
	for (i=0; i<MAXSHIPTYPE; ++i)
		ShipPrice[i] = sg->ShipPrice[i];
	GalacticChartSystem = sg->GalacticChartSystem;
	PoliceKills = sg->PoliceKills;
	TraderKills = sg->TraderKills;
	PirateKills = sg->PirateKills;
	PoliceRecordScore = sg->PoliceRecordScore;
	ReputationScore = sg->ReputationScore;
	Clicks = sg->Clicks;
	EncounterType = sg->EncounterType;
	Raided = sg->Raided;
	MonsterStatus = sg->MonsterStatus;
	DragonflyStatus = sg->DragonflyStatus;
	JaporiDiseaseStatus = sg->JaporiDiseaseStatus;
	MoonBought = sg->MoonBought;
	MonsterHull = sg->MonsterHull;
	StrCopy( NameCommander, sg->NameCommander );
	CurForm = sg->CurForm;
	MemMove( &Ship, &(sg->Ship), sizeof( Ship ) );
	MemMove( &Opponent, &(sg->Opponent), sizeof( Opponent) );
	for (i=0; i<MAXCREWMEMBER+1; ++i)
		MemMove( &Mercenary[i], &(sg->Mercenary[i]), sizeof( Mercenary[i] ) );
	for (i=0; i<MAXSOLARSYSTEM; ++i)
		MemMove( &SolarSystem[i], &(sg->SolarSystem[i]), sizeof( SolarSystem[i] ) );
	EscapePod = sg->EscapePod;
	Insurance = sg->Insurance;
	NoClaim = sg->NoClaim;
	Inspected = sg->Inspected;
	LitterWarning = sg->LitterWarning;
	Difficulty = sg->Difficulty;
	for (i=0; i<MAXWORMHOLE; ++i)
		Wormhole[i] = sg->Wormhole[i];
	for (i=0; i<MAXTRADEITEM; ++i)
		if (Ship.Cargo[i] <= 0)
			BuyingPrice[i] = 0;
		else
			BuyingPrice[i] = sg->BuyingPrice[i];
	ArtifactOnBoard = sg->ArtifactOnBoard;
	JarekStatus = sg->JarekStatus;
	InvasionStatus = sg->InvasionStatus;
	
	if (OldVersion)
	{
		ExperimentStatus = 0;
		FabricRipProbability = 0;
		VeryRareEncounter = 0;
		WildStatus = 0;
		ReactorStatus = 0;
		TrackedSystem = -1;
		JustLootedMarie = false;
		AlreadyPaidForNewspaper = false;
		GameLoaded = false;
		// if Nix already has a special, we have to get rid of the
		// spurious GETREACTOR quest.
		if (SolarSystem[NIXSYSTEM].Special == -1)
			SolarSystem[NIXSYSTEM].Special = REACTORDELIVERED;
		else
		{
			for (i=0;i<MAXSOLARSYSTEM;i++)
				if (SolarSystem[i].Special == GETREACTOR)
					SolarSystem[i].Special = -1;
		}

		// To make sure 1.1.2 is upgradeable to 1.2.0
		if (SolarSystem[GEMULONSYSTEM].Special == 27)
			SolarSystem[GEMULONSYSTEM].Special = GEMULONRESCUED;
		if (SolarSystem[DEVIDIASYSTEM].Special == 26)
			SolarSystem[DEVIDIASYSTEM].Special = JAREKGETSOUT;

		ScarabStatus = 0;
		ArrivedViaWormhole = false;
		CanSuperWarp = false;
		SharePreferences = true;
		
		if (State != 2 || !SharePreferences)
		{
			Shortcut1 = 0;
			Shortcut2 = 1;
			Shortcut3 = 2;
			Shortcut4 = 10;
			UseHWButtons = false;
			NewsAutoPay = false;
			AlwaysIgnoreTradeInOrbit = false;
			ShowTrackedRange = true;
			TrackAutoOff = true;
			RemindLoans = true;
			IdentifyStartup = false;
		}
	}
	else
	{
		UnpackBytes(sg->ExperimentAndWildStatus, &ExperimentStatus,&WildStatus);
		FabricRipProbability = sg->FabricRipProbability;
		VeryRareEncounter = sg->VeryRareEncounter;
		SharePreferences = sg->SharePreferences;
		
		if (State != 2 || !SharePreferences)
			UnpackBooleans(sg->BooleanCollection, &UseHWButtons, &NewsAutoPay,
				&ShowTrackedRange, &JustLootedMarie, &ArrivedViaWormhole, &TrackAutoOff,
				&RemindLoans, &CanSuperWarp);
		else
			UnpackBooleans(sg->BooleanCollection, &Trash, &Trash,
				&Trash, &JustLootedMarie, &ArrivedViaWormhole, &Trash,
				&Trash, &CanSuperWarp);
		
		ReactorStatus = sg-> ReactorStatus;
		TrackedSystem = sg->TrackedSystem;
		ScarabStatus = sg->ScarabStatus;
		AlreadyPaidForNewspaper = sg->AlreadyPaidForNewspaper;
		GameLoaded = sg->GameLoaded;

		if (State != 2 || !SharePreferences)
		{
			AlwaysIgnoreTradeInOrbit = sg->AlwaysIgnoreTradeInOrbit;
			Shortcut1 = sg->Shortcut1;
			Shortcut2 = sg->Shortcut2;
			Shortcut3 = sg->Shortcut3;
			Shortcut4 = sg->Shortcut4;
			IdentifyStartup = sg->IdentifyStartup;
			RectangularButtonsOn = sg->RectangularButtonsOn;
		}
	}

	if (State != 2 || !SharePreferences)
	{
		ReserveMoney = sg->ReserveMoney;
		PriceDifferences = sg->PriceDifferences;
		APLscreen = sg->APLscreen;
		TribbleMessage = sg->TribbleMessage;
		AlwaysInfo = sg->AlwaysInfo;
		LeaveEmpty = sg->LeaveEmpty;
		TextualEncounters = sg->TextualEncounters;
		Continuous = sg->Continuous;
		AttackFleeing = sg->AttackFleeing;
		AutoFuel = sg->AutoFuel;
		AutoRepair = sg->AutoRepair;
		AlwaysIgnoreTraders = sg->AlwaysIgnoreTraders;
		AlwaysIgnorePolice = sg->AlwaysIgnorePolice;
		AlwaysIgnorePirates = sg->AlwaysIgnorePirates;
	}
	
	SolarSystem[UTOPIASYSTEM].Special = MOONBOUGHT;
}

// Load game and returns true if OK
Boolean LoadGame( int State )
{
    /* Genesis port: load from SRAM battery backup */
    SAVEGAMETYPE sg_buf;
    SAVEGAMETYPE* sg = &sg_buf;

    if (State != 0)
    {
        /* Slot switching (State 1/2) not supported on Genesis – single save only */
        FrmAlert( CannotLoadAlert );
        return false;
    }

    /* Check for SRAM magic */
    extern Boolean sram_has_save(void);
    extern void sram_load_full(SAVEGAMETYPE* sg);
    if (!sram_has_save())
    {
        CurForm = MainForm;
        return true; /* no save – start fresh */
    }

    sram_load_full(sg);
    FillGlobals( sg, 0 );
    return true;
}

// *************************************************************************
// Get the current application's preferences.
// *************************************************************************
static Err AppStart(void)
{
   	FtrGet( sysFtrCreator, sysFtrNumROMVersion, &romVersion );

	LoadGame( 0 );

	ChanceOfVeryRareEncounter = CHANCEOFVERYRAREENCOUNTER;
	ChanceOfTradeInOrbit = CHANCEOFTRADEINORBIT;

   	return 0;
}


// *************************************************************************
// Pack two limited range bytes into a two nibbles (a single byte) for storage.
// Use this with caution -- it doesn't check that the values are really
// within the range of 0-15
// *************************************************************************
Byte PackBytes(Byte a, Byte b)
{
	return (Byte)(a<<4 | (b & (Byte)0x0F));
}

// *************************************************************************
// Unpack two limited range bytes from a single byte from storage.
// High order 4 bits become Byte a, low order 4 bits become Byte b
// *************************************************************************
void UnpackBytes(Byte val, Byte *a, Byte *b)
{
	*a = val >> 4;
	*b = val & (Byte) 15; // 0x0F
}


// *************************************************************************
// Pack eight booleans into a single byte for storage.
// *************************************************************************
Byte PackBooleans(Boolean a, Boolean b, Boolean c, Boolean d,
				  Boolean e, Boolean f, Boolean g, Boolean h)
{
	Byte retByte = 0;
	if (a) retByte |= (Byte)128; // 0x80
	if (b) retByte |= (Byte) 64; // 0x40
	if (c) retByte |= (Byte) 32; // 0x20
	if (d) retByte |= (Byte) 16; // etc.
	if (e) retByte |= (Byte)  8;
	if (f) retByte |= (Byte)  4;
	if (g) retByte |= (Byte)  2;
	if (h) retByte |= (Byte)  1;
	return retByte;
}

// *************************************************************************
// unpack eight booleans from a single byte out of storage.
// *************************************************************************
void UnpackBooleans(Byte val, Boolean *a, Boolean *b, Boolean *c, Boolean *d,
				  Boolean *e, Boolean *f, Boolean *g, Boolean *h)
{

	if ( val & (Byte)128 )
		*a = true;
	else
		*a = false;	
	if (val & (Byte) 64 )
		*b = true;
	else
		*b = false;
	if (val & (Byte) 32 )
		*c = true;
	else
		*c = false;
	if (val & (Byte) 16 )
		*d = true;
	else
		*d = false;
	if (val & (Byte)  8 )
		*e = true;
	else
		*e = false;
	if (val & (Byte)  4 )
		*f = true;
	else
		*f = false;
	if (val & (Byte)  2 )
		*g = true;
	else
		*g = false;
	if (val & (Byte)  1 )
		*h = true;
	else
		*h = false;

}

// Save the game, either in the Statesave or in a file
Boolean SaveGame( int State )
{
    int i;
    SAVEGAMETYPE sg_buf;
    SAVEGAMETYPE* sg = &sg_buf;
    extern void sram_save_full(SAVEGAMETYPE* sg);

    /* Pack all game state into sg */
    sg->Credits = Credits;
    sg->Debt = Debt;
    sg->Days = Days;
    sg->WarpSystem = WarpSystem;
    sg->SelectedShipType = SelectedShipType;
    for (i=0; i<MAXTRADEITEM; ++i)
    {
        sg->BuyPrice[i] = BuyPrice[i];
        sg->SellPrice[i] = SellPrice[i];
    }
    for (i=0; i<MAXSHIPTYPE; ++i)
        sg->ShipPrice[i] = ShipPrice[i];
    sg->GalacticChartSystem = GalacticChartSystem;
    sg->PoliceKills = PoliceKills;
    sg->TraderKills = TraderKills;
    sg->PirateKills = PirateKills;
    sg->PoliceRecordScore = PoliceRecordScore;
    sg->ReputationScore = ReputationScore;
    sg->AutoFuel = AutoFuel;
    sg->AutoRepair = AutoRepair;
    sg->Clicks = Clicks;
    sg->EncounterType = EncounterType;
    sg->Raided = Raided;
    sg->MonsterStatus = MonsterStatus;
    sg->DragonflyStatus = DragonflyStatus;
    sg->JaporiDiseaseStatus = JaporiDiseaseStatus;
    sg->MoonBought = MoonBought;
    sg->MonsterHull = MonsterHull;
    StrCopy( sg->NameCommander, NameCommander );
    sg->CurForm = CurForm;
    MemMove( &(sg->Ship), &Ship, sizeof( sg->Ship ) );
    MemMove( &(sg->Opponent), &Opponent, sizeof( sg->Opponent ) );
    for (i=0; i<MAXCREWMEMBER+1; ++i)
        MemMove( &(sg->Mercenary[i]), &Mercenary[i], sizeof( sg->Mercenary[i] ) );
    for (i=0; i<MAXSOLARSYSTEM; ++i)
        MemMove( &(sg->SolarSystem[i]), &SolarSystem[i], sizeof( sg->SolarSystem[i] ) );
    for (i=0; i<MAXFORFUTUREUSE; ++i)
        sg->ForFutureUse[i] = 0;
    sg->EscapePod = EscapePod;
    sg->Insurance = Insurance;
    sg->NoClaim = NoClaim;
    sg->Inspected = Inspected;
    sg->LitterWarning = LitterWarning;
    sg->AlwaysIgnoreTraders = AlwaysIgnoreTraders;
    sg->AlwaysIgnorePolice = AlwaysIgnorePolice;
    sg->AlwaysIgnorePirates = AlwaysIgnorePirates;
    sg->Difficulty = Difficulty;
    sg->VersionMajor = 1;
    sg->VersionMinor = 4;
    for (i=0; i<MAXWORMHOLE; ++i)
        sg->Wormhole[i] = Wormhole[i];
    for (i=0; i<MAXTRADEITEM; ++i)
        sg->BuyingPrice[i] = BuyingPrice[i];
    sg->ArtifactOnBoard = ArtifactOnBoard;
    sg->ReserveMoney = ReserveMoney;
    sg->PriceDifferences = PriceDifferences;
    sg->APLscreen = APLscreen;
    sg->TribbleMessage = TribbleMessage;
    sg->AlwaysInfo = AlwaysInfo;
    sg->LeaveEmpty = LeaveEmpty;
    sg->TextualEncounters = TextualEncounters;
    sg->JarekStatus = JarekStatus;
    sg->InvasionStatus = InvasionStatus;
    sg->Continuous = Continuous;
    sg->AttackFleeing = AttackFleeing;
    sg->ExperimentAndWildStatus = PackBytes(ExperimentStatus, WildStatus);
    sg->FabricRipProbability = FabricRipProbability;
    sg->VeryRareEncounter = VeryRareEncounter;
    sg->BooleanCollection = PackBooleans(UseHWButtons, NewsAutoPay,
        ShowTrackedRange, JustLootedMarie, ArrivedViaWormhole, TrackAutoOff,
        RemindLoans, CanSuperWarp);
    sg->ReactorStatus = ReactorStatus;
    sg->TrackedSystem = TrackedSystem;
    sg->ScarabStatus = ScarabStatus;
    sg->AlwaysIgnoreTradeInOrbit = AlwaysIgnoreTradeInOrbit;
    sg->AlreadyPaidForNewspaper = AlreadyPaidForNewspaper;
    sg->GameLoaded = GameLoaded;
    sg->SharePreferences = SharePreferences;
    sg->IdentifyStartup = IdentifyStartup;
    sg->Shortcut1 = Shortcut1;
    sg->Shortcut2 = Shortcut2;
    sg->Shortcut3 = Shortcut3;
    sg->Shortcut4 = Shortcut4;
    sg->RectangularButtonsOn = RectangularButtonsOn;

    /* Genesis port: write to SRAM (all States map to single slot) */
    (void)State;
    sram_save_full(sg);
    return true;
}
// *************************************************************************
// Save the current state of the application.
// *************************************************************************
static void AppStop(void)
{
    SaveGame( 0 );
    /* FrmCloseAllForms – no-op on Genesis */
}

// *************************************************************************
// This routine is the event loop for the application. 
// ************************************************************************* 
static void AppEventLoop(void)
{
    /* Genesis port: main game loop.
     * We poll joypad via ui_vsync() (called inside GEN_EvtGetEvent),
     * synthesize Palm-style events, then dispatch through the original
     * AppHandleEvent / form handler chain exactly as the Palm version did.
     * The only difference: on Continuous auto-attack we fire every 1 second
     * (60 frames) instead of using SysTicksPerSecond timing. */
    Word error;
    EventType event;
    uint32_t last_auto_tick = 0;

    do
    {
        if (Continuous && (AutoAttack || AutoFlee) && (CurForm == EncounterForm))
        {
            GEN_EvtGetEvent(&event, SysTicksPerSecond());
            if (event.eType == nilEvent &&
                (ui_frame_count - last_auto_tick) >= (uint32_t)SysTicksPerSecond())
            {
                last_auto_tick = ui_frame_count;
                EncounterFormHandleEvent(&event);
                continue;
            }
        }
        else
        {
            GEN_EvtGetEvent(&event, -1);
        }

        if (event.eType != nilEvent)
            kprintf("AppEventLoop: dispatching eType=%d CurForm=%d",
                    (int)event.eType, (int)CurForm);
        if (!AppHandleEvent(&event))
            FrmDispatchEvent(&event);

        if (CurForm != EncounterForm)
        {
            AutoAttack = false;
            AutoFlee   = false;
        }

    } while (event.eType != appStopEvent);
}
// *************************************************************************
// This routine is the event handler for the Start screen.
// *************************************************************************
Boolean MainFormHandleEvent(EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP = FrmGetActiveForm();

	switch (eventP->eType) 
	{
		case frmOpenEvent:
			FrmDrawForm ( frmP);
			handled = true;
			break;
			
		case frmUpdateEvent:
			FrmDrawForm ( frmP );
			handled = true;
			break;

		// Start new game
		case penDownEvent:
		case keyDownEvent:
			StartNewGame();
			handled = true;
			break;

		default:
			break;
	}
	
	return handled;
}


// *************************************************************************
// Handling of the events of several forms.
// *************************************************************************
Boolean DefaultFormHandleEvent(EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP = FrmGetActiveForm();

	switch (eventP->eType) 
	{
		case frmOpenEvent:
			FrmDrawForm ( frmP);
			handled = true;
			break;
		case frmUpdateEvent:
			FrmDrawForm ( frmP );
			handled = true;
			break;
	}
	
	return handled;
}


// *************************************************************************
// This is the main application.
// *************************************************************************
/* Genesis port: MerchantPilotMain / PilotMain are called from main.c's
 * int main() entry point. We expose the three lifecycle functions directly. */
Err GenAppStart(void)
{
    return AppStart();
}
void GenAppLoop(void)
{
    AppEventLoop();
}
void GenAppStop(void)
{
    AppStop();
}
