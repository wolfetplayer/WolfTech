/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).  

RTCW SP Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RTCW SP Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RTCW SP Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the RTCW SP Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the RTCW SP Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

/*
 * name:		cg_consolecmds.c
 *
 * desc:		text commands typed in at the local console, or executed by a key binding
 *
*/


#include "cg_local.h"
#include "../ui/ui_shared.h"

menuDef_t *menuScoreboard = NULL;

void CG_TargetCommand_f( void ) {
	int targetNum;
	char test[4];

	targetNum = CG_CrosshairPlayer();
	if ( targetNum == -1 ) {
		return;
	}

	trap_Argv( 1, test, 4 );
	trap_SendClientCommand( va( "gc %i %i", targetNum, atoi( test ) ) );
}



/*
=================
CG_SizeUp_f

Keybinding command
=================
*/
static void CG_SizeUp_f( void ) {
	trap_Cvar_Set( "cg_viewsize", va( "%i",(int)( cg_viewsize.integer + 10 ) ) );
}


/*
=================
CG_SizeDown_f

Keybinding command
=================
*/
static void CG_SizeDown_f( void ) {
	trap_Cvar_Set( "cg_viewsize", va( "%i",(int)( cg_viewsize.integer - 10 ) ) );
}


/*
=============
CG_Viewpos_f

Debugging command to print the current position
=============
*/
static void CG_Viewpos_f( void ) {
	CG_Printf( "(%i %i %i) : %i\n", (int)cg.refdef.vieworg[0],
			   (int)cg.refdef.vieworg[1], (int)cg.refdef.vieworg[2],
			   (int)cg.refdefViewAngles[YAW] );
}


static void CG_ScoresDown_f( void ) {
	// game-over sequence: CG_Draw2D already force-draws the scoreboard; the numScores=0 reset below would just blank it for a frame
	if ( cgs.gametype == GT_COOP_SURVIVAL && cg.predictedPlayerState.stats[STAT_GAMEOVER] ) {
		return;
	}

	if ( cg.scoresRequestTime + 2000 < cg.time ) {
		// the scores are more than two seconds out of data,
		// so request new ones
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );

		// leave the current scores up if they were already
		// displayed, but if this is the first hit, clear them out
		if ( !cg.showScores ) {
			cg.showScores = qtrue;
			cg.numScores = 0;
		}
	} else {
		// show the cached contents even if they just pressed if it
		// is within two seconds
		cg.showScores = qtrue;
	}
}

static void CG_ScoresUp_f( void ) {
	if ( cg.showScores ) {
		cg.showScores = qfalse;
		cg.scoreFadeTime = cg.time;
	}
}


static void CG_LoadHud_f( void ) {
	char buff[1024];
	const char *hudSet;
	memset( buff, 0, sizeof( buff ) );

	String_Init();
	Menu_Reset();

	trap_Cvar_VariableStringBuffer( "cg_hudFiles", buff, sizeof( buff ) );
	hudSet = buff;
	if ( hudSet[0] == '\0' ) {
		hudSet = "ui/hud.txt";
	}

	CG_LoadMenus( hudSet );
	menuScoreboard = NULL;
}


//----(SA)	item (key/pickup) drawing
static void CG_InventoryDown_f( void ) {
	cg.showItems = qtrue;
}

static void CG_InventoryUp_f( void ) {
	cg.showItems = qfalse;
	cg.itemFadeTime = cg.time;
}

//----(SA)	end

static void CG_TellTarget_f( void ) {
	int clientNum;
	char command[128];
	char message[128];

	clientNum = CG_CrosshairPlayer();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

static void CG_TellAttacker_f( void ) {
	int clientNum;
	char command[128];
	char message[128];

	clientNum = CG_LastAttacker();
	if ( clientNum == -1 ) {
		return;
	}

	trap_Args( message, 128 );
	Com_sprintf( command, 128, "tell %i %s", clientNum, message );
	trap_SendClientCommand( command );
}

/////////// cameras

#define MAX_CAMERAS 64  // matches define in splines.cpp
qboolean cameraInuse[MAX_CAMERAS];

int CG_LoadCamera( const char *name ) {
	int i;
	for ( i = 1; i < MAX_CAMERAS; i++ ) {    // start at '1' since '0' is always taken by the cutscene camera
		if ( !cameraInuse[i] ) {
			if ( trap_loadCamera( i, name ) ) {
				cameraInuse[i] = qtrue;
				return i;
			}
		}
	}
	return -1;
}

void CG_FreeCamera( int camNum ) {
	cameraInuse[camNum] = qfalse;
}

/*
==============
CG_StartCamera
==============
*/
void CG_StartCamera( const char *name, qboolean startBlack ) {
	char lname[MAX_QPATH];

	//if ( cg.predictedPlayerState.stats[STAT_HEALTH] <= 0 )	// don't allow camera to start if you're dead
	//	return;

	COM_StripExtension( name, lname, sizeof( lname ) );    //----(SA)	added
	Q_strcat( lname, sizeof( lname ), ".camera" );

	if ( trap_loadCamera( CAM_PRIMARY, va( "cameras/%s", lname ) ) ) {
		cg.cameraMode = qtrue;				// camera on in cgame
		if ( startBlack ) {
			CG_Fade( 0, 0, 0, 255, cg.time, 0 );		// go black
		}
		trap_Cvar_Set( "cg_letterbox", "1" ); // go letterbox
		trap_SendClientCommand( "startCamera" );	// camera on in game
		trap_startCamera( CAM_PRIMARY, cg.time );	// camera on in client
	} else {
		//----(SA)	removed check for cams in main dir
		cg.cameraMode = qfalse;                 // camera off in cgame
		trap_SendClientCommand( "stopCamera" );    // camera off in game
		trap_stopCamera( CAM_PRIMARY );           // camera off in client
		CG_Fade( 0, 0, 0, 0, cg.time, 0 );        // ensure fadeup
		trap_Cvar_Set( "cg_letterbox", "0" );
		CG_Printf( "Unable to load camera %s\n",lname );
	}
}

/*
==============
CG_StopCamera
==============
*/
void CG_StopCamera( void ) {
	cg.cameraMode = qfalse;                 // camera off in cgame
	trap_SendClientCommand( "stopCamera" );    // camera off in game
	trap_stopCamera( CAM_PRIMARY );           // camera off in client
	trap_Cvar_Set( "cg_letterbox", "0" );

	// fade back into world
	CG_Fade( 0, 0, 0, 255, 0, 0 );
	CG_Fade( 0, 0, 0, 0, cg.time + 500, 2000 );

}

static void CG_Camera_f( void ) {
	char name[MAX_QPATH];

	trap_Argv( 1, name, sizeof( name ) );

	CG_StartCamera( name, qfalse );
}

static void CG_Fade_f( void ) {
	int r, g, b, a;
	float duration;

	if ( trap_Argc() < 6 ) {
		return;
	}

	r = atof( CG_Argv( 1 ) );
	g = atof( CG_Argv( 2 ) );
	b = atof( CG_Argv( 3 ) );
	a = atof( CG_Argv( 4 ) );

	duration = atof( CG_Argv( 5 ) ) * 1000;

	CG_Fade( r, g, b, a, cg.time, duration );
}

static void CG_PlayerStart_f( void ) {
	trap_SendClientCommand( "playerstart" );
	trap_Cvar_Set( "cg_norender", "0" );
}

static void CG_VoiceChat_f( void ) {
	char chatCmd[64];
 
	if ( cgs.gametype == GT_SINGLE_PLAYER || trap_Argc() != 2 ) {
		return;
	}

	// NERVE - SMF - don't let spectators voice chat
	// NOTE - This cg.snap will be the person you are following, but its just for intermission test
	if ( cg.snap && ( cg.snap->ps.pm_type != PM_INTERMISSION ) ) {
		//if ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.clientNum].team == TEAM_FREE ) {
		if ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR ) {
			CG_Printf( "Can't voice chat as a spectator.\n" );
			return;
		}
	}
 
	trap_Argv( 1, chatCmd, 64 );
 
	trap_SendConsoleCommand( va( "cmd vsay %s\n", chatCmd ) );
}

static void CG_TeamVoiceChat_f( void ) {
	char chatCmd[64];
 
	if ( cgs.gametype == GT_SINGLE_PLAYER || trap_Argc() != 2 ) {
		return;
	}
 
	// NERVE - SMF - don't let spectators voice chat
	// NOTE - This cg.snap will be the person you are following, but its just for intermission test
	if ( cg.snap && ( cg.snap->ps.pm_type != PM_INTERMISSION ) ) {
		//if ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR || cgs.clientinfo[cg.clientNum].team == TEAM_FREE ) {
		if ( cgs.clientinfo[cg.clientNum].team == TEAM_SPECTATOR ) {
			CG_Printf( "Can't team voice chat as a spectator.\n" );
			return;
		}
	}
 
	trap_Argv( 1, chatCmd, 64 );
 
	trap_SendConsoleCommand( va( "cmd vsay_team %s\n", chatCmd ) );
}

static void CG_QuickMessage_f( void ) {
	if ( cgs.gametype == GT_SINGLE_PLAYER  ) {
		return;
	}
	if ( cg_quickMessageAlt.integer ) {
		trap_UI_Popup( "UIMENU_WM_QUICKMESSAGEALT" );
	} else {
		trap_UI_Popup( "UIMENU_WM_QUICKMESSAGE" );
	}
}

static void CG_OpenLimbo_f( void ) {
	if ( cgs.gametype <= GT_SINGLE_PLAYER ) {
		return;
	}
	trap_UI_Popup( "UIMENU_WM_LIMBO" );
}

static void CG_CloseLimbo_f( void ) {
	if ( cgs.gametype <= GT_SINGLE_PLAYER ) {
		return;
	}
	trap_UI_ClosePopup( "UIMENU_WM_LIMBO" );
}

static void CG_LimboMessage_f( void ) {
	char teamStr[80], classStr[80], weapStr[80];

	if ( cgs.gametype <= GT_SINGLE_PLAYER ) {
		return;
	}

	Q_strncpyz( teamStr, CG_TranslateString( CG_Argv( 1 ) ), 80 );
	Q_strncpyz( classStr, CG_TranslateString( CG_Argv( 2 ) ), 80 );
	Q_strncpyz( weapStr, CG_TranslateString( CG_Argv( 3 ) ), 80 );

	CG_PriorityCenterPrint( va( "%s %s %s %s %s.", CG_TranslateString( "You will spawn as an" ),
								teamStr, classStr, CG_TranslateString( "with a" ), weapStr ), SCREEN_HEIGHT - ( SCREEN_HEIGHT * 0.25 ), SMALLCHAR_WIDTH, -1 );
}
// -NERVE - SMF

/*
===================
CG_DumpCastAi_f

Dump a ai_zombie definition to the ents file
===================
*/
static void CG_DumpCastAi_f( void ) {
	char aicastfilename[MAX_QPATH];
	char ainame[MAX_STRING_CHARS];
	char aitype[MAX_STRING_CHARS];
	char *aiautoname;
	char *extptr, *buffptr;
	char buff[1024];
	fileHandle_t f;
	int autonumber = 0;


	trap_Cvar_VariableStringBuffer( "cg_entityEditCounter", buff, sizeof( buff ) );
	autonumber = atoi( buff );

	// Check for argument
	if ( trap_Argc() < 2 ) {
		CG_Printf( "Usage: dumpcastai <type> [name]\n" );
		return;
	}
	trap_Argv( 1, aitype, sizeof( aitype ) );

	if ( strcmp( "ai_soldier", aitype ) &&
		 strcmp( "ai_american", aitype ) &&
		 strcmp( "ai_zombie", aitype ) &&
		 strcmp( "ai_warzombie", aitype ) &&
		 strcmp( "ai_zombie_surv", aitype ) &&
		 strcmp( "ai_venom", aitype ) &&
		 strcmp( "ai_loper", aitype ) &&
		 strcmp( "ai_loper_special", aitype ) &&
		 strcmp( "ai_boss_helga", aitype ) &&
		 strcmp( "ai_boss_heinrich", aitype ) &&
		 strcmp( "ai_eliteguard", aitype ) &&
		 strcmp( "ai_supersoldier", aitype ) &&
		 strcmp( "ai_protosoldier", aitype ) &&
		 strcmp( "ai_blackguard", aitype ) &&
		 strcmp( "ai_partisan", aitype ) &&
		 strcmp( "ai_civilian", aitype ) ) {

		CG_Printf( "Wrong type\n" );
		CG_Printf( "Usage: dumpcastai <type> [name]\n" );
		return;
	}

	if ( trap_Argc() == 3 ) {
		trap_Argv( 2, ainame, sizeof( ainame ) );
	} else
	{
		aiautoname = va( "coop_%s_%d", aitype, autonumber++ );
		Q_strncpyz( ainame, aiautoname, strlen( aiautoname ) + 1 );
	}

	trap_Cvar_Set( "cg_entityEditCounter", va( "%i",autonumber ) );

	// Open aicast file
	Q_strncpyz( aicastfilename, cgs.mapname, sizeof( aicastfilename ) );
	extptr = aicastfilename + strlen( aicastfilename ) - 4;
	if ( extptr < aicastfilename || Q_stricmp( extptr, ".bsp" ) ) {
		CG_Printf( "Unable to dump, unknown map name?\n" );
		return;
	}
	Q_strncpyz( extptr, ".ents", 6 );
	trap_FS_FOpenFile( aicastfilename, &f, FS_APPEND_SYNC );
	if ( !f ) {
		CG_Printf( "Failed to open '%s' for writing.\n", aicastfilename );
		return;
	}

	// Strip bad characters out
	for ( buffptr = ainame; *buffptr; buffptr++ )
	{
		if ( *buffptr == '\n' ) {
			*buffptr = ' ';
		} else if ( *buffptr == '"' ) {
			*buffptr = '\'';
		}
	}
	// Kill any trailing space as well
	if ( *( buffptr - 1 ) == ' ' ) {
		*( buffptr - 1 ) = 0;
	}

	// Strip bad characters out
	for ( buffptr = aitype; *buffptr; buffptr++ )
	{
		if ( *buffptr == '\n' ) {
			*buffptr = ' ';
		} else if ( *buffptr == '"' ) {
			*buffptr = '\'';
		}
	}
	// Kill any trailing space as well
	if ( *( buffptr - 1 ) == ' ' ) {
		*( buffptr - 1 ) = 0;
	}

	// Build the entity definition
	buffptr = va(   "{\n\"classname\" \"%s\"\n\"origin\" \"%i %i %i\"\n\"ainame\" \"%s\"\n\"angle\" \"%i\"\n\"spawnflags\" \"1\"\n}\n", aitype, (int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2], ainame, (int)cg.refdefViewAngles[YAW] );

	// And write out/acknowledge
	trap_FS_Write( buffptr, strlen( buffptr ), f );
	trap_FS_FCloseFile( f );
	CG_Printf( "%s (%s) entity dumped to '%s' (%i %i %i).\n", aitype, ainame, aicastfilename,
			   (int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2] );
}

/*
===================
CG_DumpLocation_f

Dump a target_location definition to a file
===================
*/
static void CG_DumpLocation_f( void ) {
	char locfilename[MAX_QPATH];
	char locname[MAX_STRING_CHARS];
	char *extptr, *buffptr;
	fileHandle_t f;

	// Check for argument
	if ( trap_Argc() < 2 ) {
		CG_Printf( "Usage: dumploc <locationname>\n" );
		return;
	}
	trap_Args( locname, sizeof( locname ) );

	// Open locations file
	Q_strncpyz( locfilename, cgs.mapname, sizeof( locfilename ) );
	extptr = locfilename + strlen( locfilename ) - 4;
	if ( extptr < locfilename || Q_stricmp( extptr, ".bsp" ) ) {
		CG_Printf( "Unable to dump, unknown map name?\n" );
		return;
	}
	Q_strncpyz( extptr, ".ents", 6 );
	trap_FS_FOpenFile( locfilename, &f, FS_APPEND_SYNC );
	if ( !f ) {
		CG_Printf( "Failed to open '%s' for writing.\n", locfilename );
		return;
	}

	// Strip bad characters out
	for ( buffptr = locname; *buffptr; buffptr++ )
	{
		if ( *buffptr == '\n' ) {
			*buffptr = ' ';
		} else if ( *buffptr == '"' ) {
			*buffptr = '\'';
		}
	}
	// Kill any trailing space as well
	if ( *( buffptr - 1 ) == ' ' ) {
		*( buffptr - 1 ) = 0;
	}

	// Build the entity definition
	buffptr = va(   "{\n\"classname\" \"target_location\"\n\"origin\" \"%i %i %i\"\n\"message\" \"%s\"\n}\n\n",
					(int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2], locname );

	// And write out/acknowledge
	trap_FS_Write( buffptr, strlen( buffptr ), f );
	trap_FS_FCloseFile( f );
	CG_Printf( "Entity dumped to '%s' (%i %i %i).\n", locfilename,
			   (int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2] );
}

/*
===================
CG_DumpCoopSpawnpoint_f

Dump a coop_spawnpoint definition to a file
===================
*/
static void CG_DumpCoopSpawnpoint_f( void ) {
	char entsfilename[MAX_QPATH];
	char *extptr, *buffptr;
	fileHandle_t f;
	char buf[64];
	int flagpolenumber = 0;

	trap_Cvar_VariableStringBuffer( "__flagpolenumber", buf, sizeof( buf ) );

	flagpolenumber = atoi( buf );

	trap_Cvar_VariableStringBuffer( "mapname", buf, sizeof( buf ) );



	// Open ents file
	Q_strncpyz( entsfilename, cgs.mapname, sizeof( entsfilename ) );
	extptr = entsfilename + strlen( entsfilename ) - 4;
	if ( extptr < entsfilename || Q_stricmp( extptr, ".bsp" ) ) {
		CG_Printf( "Unable to dump, unknown map name?\n" );
		return;
	}
	Q_strncpyz( extptr, ".ents", 6 );
	trap_FS_FOpenFile( entsfilename, &f, FS_APPEND_SYNC );
	if ( !f ) {
		CG_Printf( "Failed to open '%s' for writing.\n", entsfilename );
		return;
	}

	// Build the entity definition
	if ( flagpolenumber - 1 >= 0 ) {
		buffptr = va(   "{\n\"classname\" \"coop_spawnpoint\"\n\"spawnflags\" \"2\"\n\"origin\" \"%i %i %i\"\n\"angle\" \"%d\"\n\"targetname\" \"%s\"\n}\n\n",
						(int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2], (int)cg.refdefViewAngles[YAW], va( "%s%d", buf, flagpolenumber - 1 ) );
	} else {
		buffptr = va(   "{\n\"classname\" \"coop_spawnpoint\"\n\"spawnflags\" \"3\"\n\"origin\" \"%i %i %i\"\n\"angle\" \"%d\"\n}\n\n",
						(int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2], (int)cg.refdefViewAngles[YAW] );
	}

	// And write out/acknowledge
	trap_FS_Write( buffptr, strlen( buffptr ), f );
	trap_FS_FCloseFile( f );
	CG_Printf( "coop_spawnpoint dumped to '%s' (%i %i %i).\n", entsfilename,
			   (int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2] );

	// draw the new spawnpoint
	DrawDebugAABB( cg.snap->ps.origin, cg.snap->ps.mins, cg.snap->ps.maxs, 999999, colorBlue, 6 );
	if ( flagpolenumber - 1 >= 0 ) {
		DrawDebugText( cg.snap->ps.origin, va( "coop_spawnpoint: \ntargetname: %s%d", buf, flagpolenumber - 1 ), 999999, colorWhite );
	} else {
		DrawDebugText( cg.snap->ps.origin, "coop_spawnpoint", 999999, colorWhite );
	}
}

/*
===================
CG_DumpFlagPole_f

Dump a coop_spawnpoint_trigger definition to a file
===================
*/
static void CG_DumpFlagPole_f( void ) {
	char entsfilename[MAX_QPATH];
	char *extptr, *buffptr;
	fileHandle_t f;
	char buf[64];
	int flagpolenumber = 0;

	trap_Cvar_VariableStringBuffer( "__flagpolenumber", buf, sizeof( buf ) );

	flagpolenumber = atoi( buf );

	trap_Cvar_VariableStringBuffer( "mapname", buf, sizeof( buf ) );

	// Open ents file
	Q_strncpyz( entsfilename, cgs.mapname, sizeof( entsfilename ) );
	extptr = entsfilename + strlen( entsfilename ) - 4;
	if ( extptr < entsfilename || Q_stricmp( extptr, ".bsp" ) ) {
		CG_Printf( "Unable to dump, unknown map name?\n" );
		return;
	}
	Q_strncpyz( extptr, ".ents", 6 );
	trap_FS_FOpenFile( entsfilename, &f, FS_APPEND_SYNC );
	if ( !f ) {
		CG_Printf( "Failed to open '%s' for writing.\n", entsfilename );
		return;
	}

	// Build the entity definition
	buffptr = va(   "{\n\"classname\" \"coop_spawnpoint_trigger\"\n\"origin\" \"%i %i %i\"\n\"angle\" \"%d\"\n\"model\" \"models/multiplayer/flagpole/flagpole_reinforce.md3\"\n\"target\" \"%s\"\n}\n\n",
					(int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2] - 60, (int)cg.refdefViewAngles[YAW], va( "%s%d", buf, flagpolenumber++ ) );

	// And write out/acknowledge
	trap_FS_Write( buffptr, strlen( buffptr ), f );
	trap_FS_FCloseFile( f );
	CG_Printf( "coop_spawnpoint dumped to '%s' (%i %i %i).\n", entsfilename,
			   (int) cg.snap->ps.origin[0], (int) cg.snap->ps.origin[1], (int) cg.snap->ps.origin[2] - 60 );

	// draw the new spawnpoint
	DrawDebugAABB( cg.snap->ps.origin, cg.snap->ps.mins, cg.snap->ps.maxs, 999999, colorGreen, 6 );
	DrawDebugText( cg.snap->ps.origin, va( "coop_spawnpoint_trigger: \ntarget: %s%d", buf, flagpolenumber - 1 ), 999999, colorWhite );

	trap_Cvar_Set( "__flagpolenumber", va( "%d", flagpolenumber ) );
}

/*
===================
CG_DumpSound_f

Places a new script speaker at the player's current origin, appends it to
sound/maps/<mapname>.sps, and starts it playing immediately in this session.

Usage: dumpsound <soundfile> [wait=N] [random=N] [volume=N] [range=N]
                  [looped=no|on|off] [broadcast=no|global|nopvs] [targetname=name]
===================
*/
// TEMP: injects hardcoded UTF-8 literals into the notify feed to test the Unicode font path
static void CG_TestUTF8_f( void ) {
	CG_AddToNotify( "UTF8 test: Cyrillic \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 (Privet)" );
	CG_AddToNotify( "UTF8 test: accented caf\xC3\xA9 m\xC3\xB6tley na\xC3\xAFve" );
	CG_AddToNotify( "UTF8 test: Greek \xCE\xB3\xCE\xB5\xCE\xB9\xCE\xB1 (geia)" );
}

static void CG_DumpSound_f( void ) {
	scriptSpeaker_t speaker;
	int             argc;
	int             i;
	char            arg[MAX_QPATH];
	char            *value;

	argc = trap_Argc();

	if ( argc < 2 ) {
		CG_Printf( "Usage: dumpsound <soundfile> [wait=N] [random=N] [volume=N] [range=N] [looped=no|on|off] [broadcast=no|global|nopvs] [targetname=name]\n" );
		return;
	}

	Com_Memset( &speaker, 0, sizeof( speaker ) );
	speaker.volume = 127;
	speaker.range  = 1250;
	VectorCopy( cg.snap->ps.origin, speaker.origin );

	Q_strncpyz( speaker.filename, CG_Argv( 1 ), sizeof( speaker.filename ) );

	for ( i = 2; i < argc; i++ ) {
		trap_Argv( i, arg, sizeof( arg ) );

		value = strchr( arg, '=' );
		if ( !value ) {
			CG_Printf( S_COLOR_YELLOW "WARNING: dumpsound ignoring malformed argument '%s' (expected key=value)\n", arg );
			continue;
		}
		*value++ = '\0';

		if ( !Q_stricmp( arg, "wait" ) ) {
			speaker.wait = atoi( value );
		} else if ( !Q_stricmp( arg, "random" ) ) {
			speaker.random = atoi( value );
		} else if ( !Q_stricmp( arg, "volume" ) ) {
			speaker.volume = atoi( value );
		} else if ( !Q_stricmp( arg, "range" ) ) {
			speaker.range = atoi( value );
		} else if ( !Q_stricmp( arg, "targetname" ) ) {
			Q_strncpyz( speaker.targetname, value, sizeof( speaker.targetname ) );
		} else if ( !Q_stricmp( arg, "looped" ) ) {
			if ( !Q_stricmp( value, "on" ) ) {
				speaker.loop      = SPKR_LOOPED_ON;
				speaker.activated = qtrue;
			} else if ( !Q_stricmp( value, "off" ) ) {
				speaker.loop = SPKR_LOOPED_OFF;
			} else if ( !Q_stricmp( value, "no" ) ) {
				speaker.loop = SPKR_NOT_LOOPED;
			} else {
				CG_Printf( S_COLOR_YELLOW "WARNING: dumpsound unknown looped value '%s' (expected no|on|off)\n", value );
			}
		} else if ( !Q_stricmp( arg, "broadcast" ) ) {
			if ( !Q_stricmp( value, "global" ) ) {
				speaker.broadcast = SPKR_GLOBAL;
			} else if ( !Q_stricmp( value, "nopvs" ) ) {
				speaker.broadcast = SPKR_NOPVS;
			} else if ( !Q_stricmp( value, "no" ) ) {
				speaker.broadcast = SPKR_LOCAL;
			} else {
				CG_Printf( S_COLOR_YELLOW "WARNING: dumpsound unknown broadcast value '%s' (expected no|global|nopvs)\n", value );
			}
		} else {
			CG_Printf( S_COLOR_YELLOW "WARNING: dumpsound ignoring unknown argument '%s'\n", arg );
		}
	}

	if ( speaker.volume < 0 ) {
		speaker.volume = 0;
	} else if ( speaker.volume > 255 ) {
		speaker.volume = 255;
	}
	if ( speaker.range < 0 ) {
		speaker.range = 0;
	}
	if ( speaker.wait < 0 ) {
		speaker.wait = 0;
	}
	if ( speaker.random < 0 ) {
		speaker.random = 0;
	}

	if ( !CG_AddScriptSpeaker( &speaker ) ) {
		return;
	}

	CG_SaveSpeakerScript();

	CG_Printf( "Placed speaker #%i '%s' at %.0f %.0f %.0f\n",
			   numScriptSpeakers - 1, speaker.filename,
			   (double) speaker.origin[0], (double) speaker.origin[1], (double) speaker.origin[2] );
}

/*
===================
CG_ListSounds_f

Lists all script speakers currently loaded for this map.
===================
*/
static void CG_ListSounds_f( void ) {
	static const char *loopedStr[]    = { "no", "on", "off" };
	static const char *broadcastStr[] = { "no", "global", "nopvs" };
	int             i;
	scriptSpeaker_t *speaker;

	if ( !numScriptSpeakers ) {
		CG_Printf( "No script speakers loaded for this map.\n" );
		return;
	}

	for ( i = 0; i < numScriptSpeakers; i++ ) {
		speaker = &scriptSpeakers[i];

		CG_Printf( "#%i: '%s' at %.0f %.0f %.0f  looped=%s broadcast=%s wait=%i random=%i volume=%i range=%i%s%s\n",
				   i, speaker->filename,
				   (double) speaker->origin[0], (double) speaker->origin[1], (double) speaker->origin[2],
				   loopedStr[speaker->loop], broadcastStr[speaker->broadcast],
				   speaker->wait, speaker->random, speaker->volume, speaker->range,
				   *speaker->targetname ? " targetname=" : "", speaker->targetname );
	}
}

/*
===================
CG_DeleteSound_f

Removes a script speaker by index (see listsounds) and re-saves the .sps file.
===================
*/
static void CG_DeleteSound_f( void ) {
	int index;

	if ( trap_Argc() != 2 ) {
		CG_Printf( "Usage: deletesound <index>  (see listsounds)\n" );
		return;
	}

	index = atoi( CG_Argv( 1 ) );

	if ( !CG_DeleteScriptSpeaker( index ) ) {
		CG_Printf( S_COLOR_RED "ERROR: no script speaker #%i\n", index );
		return;
	}

	CG_SaveSpeakerScript();

	CG_Printf( "Deleted speaker #%i\n", index );
}

static void CG_DisableDebugLines_f( void ) {
	OmnibotDisableDrawing();
	CG_ClearWorldText();
}

/*
===================
L0 - OSPx port

+vstr
===================
*/
void CG_vstrDown_f(void) {
	if (trap_Argc() == 5) {
		trap_SendConsoleCommand(va("vstr %s;", CG_Argv(1)));
	}
	else { CG_Printf("[cgnotify]^3Usage: ^7+vstr [down_vstr] [up_vstr]\n"); }
}

/*
===================
L0 - OSPx port

-vstr
===================
*/
void CG_vstrUp_f(void) {
	if (trap_Argc() == 5) {
		trap_SendConsoleCommand(va("vstr %s;", CG_Argv(2)));
	}
	else { CG_Printf("[cgnotify]^3Usage: ^7+vstr [down_vstr] [up_vstr]\n"); }
}

typedef struct {
	char    *cmd;
	void ( *function )( void );
} consoleCommand_t;

static consoleCommand_t commands[] = {
	{ "testgun", CG_TestGun_f },
	{ "testmodel", CG_TestModel_f },
	{ "nextframe", CG_TestModelNextFrame_f },
	{ "prevframe", CG_TestModelPrevFrame_f },
	{ "nextskin", CG_TestModelNextSkin_f },
	{ "prevskin", CG_TestModelPrevSkin_f },
	{ "viewpos", CG_Viewpos_f },
	{ "+scores", CG_ScoresDown_f },
	{ "-scores", CG_ScoresUp_f },
	{ "+inventory", CG_InventoryDown_f },
	{ "-inventory", CG_InventoryUp_f },
	{ "zoomin", CG_ZoomIn_f },
	{ "zoomout", CG_ZoomOut_f },
	{ "sizeup", CG_SizeUp_f },
	{ "sizedown", CG_SizeDown_f },
	{ "weaplastused", CG_LastWeaponUsed_f },
	{ "weapnextinbank", CG_NextWeaponInBank_f },
	{ "weapprevinbank", CG_PrevWeaponInBank_f },
	{ "weapnext", CG_NextWeapon_f },
	{ "weapprev", CG_PrevWeapon_f },
	{ "weapalt", CG_AltWeapon_f },
	{ "weapon", CG_Weapon_f },
	{ "weaponbank", CG_WeaponBank_f },
	{ "itemnext", CG_NextItem_f },
	{ "itemprev", CG_PrevItem_f },
	{ "item", CG_Item_f },
	{ "tell_target", CG_TellTarget_f },
	{ "tell_attacker", CG_TellAttacker_f },
	{ "tcmd", CG_TargetCommand_f },
	{ "loadhud", CG_LoadHud_f },
	{ "loaddeferred", CG_LoadDeferredPlayers },  // spelling fixed (SA)
	{ "camera", CG_Camera_f },   // duffy
	{ "fade", CG_Fade_f },   // duffy
	{ "playerstart", CG_PlayerStart_f },

	// NERVE - SMF
	{ "mp_QuickMessage", CG_QuickMessage_f },
	{ "quickmessage", CG_QuickMessage_f },
	{ "OpenLimboMenu", CG_OpenLimbo_f },
	{ "CloseLimboMenu", CG_CloseLimbo_f },
	{ "LimboMessage", CG_LimboMessage_f },
	{ "VoiceChat", CG_VoiceChat_f },
	{ "VoiceTeamChat", CG_TeamVoiceChat_f },
 	// -NERVE - SMF
	{ "dumploc", CG_DumpLocation_f },
	{ "dumpcastai", CG_DumpCastAi_f },
	{ "dumpcoopspawnpoint", CG_DumpCoopSpawnpoint_f },
	{ "dumpflagpole", CG_DumpFlagPole_f },
	{ "dumpsound", CG_DumpSound_f },
	{ "listsounds", CG_ListSounds_f },
	{ "deletesound", CG_DeleteSound_f },
	{ "nodebuglines", CG_DisableDebugLines_f },
	// -NERVE - SMF

	// Coop
	{ "+vstr", CG_vstrDown_f },
	{ "-vstr", CG_vstrUp_f },
	// ~Coop

	{ "utf8test", CG_TestUTF8_f }  // TEMP: tests the Unicode font path
};


/*
=================
CG_ConsoleCommand

The string has been tokenized and can be retrieved with
Cmd_Argc() / Cmd_Argv()
=================
*/
qboolean CG_ConsoleCommand( void ) {
	const char  *cmd;
	int i;

	cmd = CG_Argv( 0 );

	for ( i = 0 ; i < ARRAY_LEN( commands ) ; i++ ) {
		if ( !Q_stricmp( cmd, commands[i].cmd ) ) {
			commands[i].function();
			return qtrue;
		}
	}

	return qfalse;
}


/*
=================
CG_InitConsoleCommands

Let the client system know about all of our commands
so it can perform tab completion
=================
*/
void CG_InitConsoleCommands( void ) {
	int i;

	for ( i = 0 ; i < ARRAY_LEN( commands ) ; i++ ) {
		trap_AddCommand( commands[i].cmd );
	}

	//
	// the game server will interpret these commands, which will be automatically
	// forwarded to the server after they are not recognized locally
	//
	trap_AddCommand( "kill" );
	trap_AddCommand( "say" );
	trap_AddCommand( "say_team" );
	trap_AddCommand( "say_limbo" );           // NERVE - SMF
	trap_AddCommand( "tell" );
	trap_AddCommand( "vsay" );
	trap_AddCommand( "vsay_team" );
//	trap_AddCommand( "vtell" );
//	trap_AddCommand( "vtaunt" );
//	trap_AddCommand( "vosay" );
//	trap_AddCommand( "vosay_team" );
//	trap_AddCommand( "votell" );
	trap_AddCommand( "give" );
	trap_AddCommand( "god" );
	trap_AddCommand( "notarget" );
	trap_AddCommand( "noclip" );
	trap_AddCommand( "dropammo" );
	trap_AddCommand( "where" );
	trap_AddCommand( "team" );
	trap_AddCommand( "follow" );
	trap_AddCommand( "follownext" );
	trap_AddCommand( "followprev" );
	trap_AddCommand( "levelshot" );
	trap_AddCommand( "addbot" );
	trap_AddCommand( "setviewpos" );
	trap_AddCommand( "callvote" );
	trap_AddCommand( "vote" );
//	trap_AddCommand( "callteamvote" );
//	trap_AddCommand( "teamvote" );
	trap_AddCommand( "stats" );
//	trap_AddCommand( "teamtask" );
	trap_AddCommand( "loaddeferred" );        // spelling fixed (SA)

	trap_AddCommand( "startCamera" );
	trap_AddCommand( "stopCamera" );
	trap_AddCommand( "setCameraOrigin" );

	// Rafael
	trap_AddCommand( "nofatigue" );

	// NERVE - SMF
	trap_AddCommand( "setspawnpt" );
	// NERVE - SMF

	// coop
	trap_AddCommand( "spawnpoint" );
	trap_AddCommand( "teleport" );

	// New stuff
	trap_AddCommand( "login" );
	trap_AddCommand( "@login" );
	trap_AddCommand( "logout" );
	trap_AddCommand( "incognito" );
	trap_AddCommand( "getstatus" );
	// End

#ifdef MONEY
	trap_AddCommand( "buy" );
#endif
}
