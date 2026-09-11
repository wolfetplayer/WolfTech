/*
===========================================================================

Return to Castle Wolfenstein single player GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Return to Castle Wolfenstein single player GPL Source Code (RTCW SP Source Code).

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

// g_intro.c - server side of the "introtitle" script action.

#include "g_local.h"
#include "g_intro.h"

typedef struct {
	qboolean configured;     // an "introtitle" call is waiting to run this level
	qboolean active;         // clock running (startTime is valid)
	int startTime;
	int fadeIn;               // ms per line fade-in, also its share of the end fade-out
	int hold, fadeOut, stagger;
	int freezeEndTime;       // freeze releases here, before endTime, while still black
	int endTime;              // whole sequence is over
	qboolean freeze;
	qboolean hasMission;
	int numLines;              // includes the mission line, if any
	char text[MAX_INTRO_LINES * MAX_INTRO_LINE_LEN]; // '\n'-joined, sent to clients as-is
} introState_t;

static introState_t intro;

// true once a real player's ClientBegin has fired this level - pers.connected alone isn't enough, since a single-player connection persists across a map change
static qboolean introAnyClientBegun;

/*
============
G_Intro_BuildCommand

  Shared by every send site, so they can't drift out of sync with each other.
============
*/
static void G_Intro_BuildCommand( char *buf, int bufSize ) {
	Com_sprintf( buf, bufSize, "introtitle %i %i %i %i %i %i %i \"%s\"\n",
				 intro.startTime, intro.fadeIn, intro.hold, intro.fadeOut, intro.stagger,
				 intro.freeze, intro.hasMission, intro.text );
}

/*
============
G_Intro_Activate

  Starts the clock. Sends nothing - callers broadcast/unicast themselves.
============
*/
static void G_Intro_Activate( void ) {
	int visiblePhaseEnd;

	intro.active = qtrue;
	intro.startTime = level.time;

	visiblePhaseEnd = ( intro.numLines - 1 ) * intro.stagger + intro.fadeIn; // last line fully faded in
	intro.freezeEndTime = intro.startTime + visiblePhaseEnd + intro.hold;
	intro.endTime = intro.freezeEndTime + intro.fadeOut;
}

/*
============
G_Intro_Start

  lines/numLines are the caption lines in order; mission (may be NULL/empty) is
  appended as the last, emphasized line. Called from G_ScriptAction_IntroTitle.
============
*/
void G_Intro_Start( char lines[][MAX_INTRO_LINE_LEN], int numLines, const char *mission,
					 int fadeIn, int hold, int fadeOut, int stagger, qboolean freeze ) {
	int i;

	if ( !g_introTitles.integer ) {
		return; // server-wide kill switch
	}
	if ( numLines <= 0 && ( !mission || !mission[0] ) ) {
		return; // nothing to show
	}
	if ( numLines > MAX_INTRO_LINES ) {
		numLines = MAX_INTRO_LINES;
	}

	memset( &intro, 0, sizeof( intro ) );
	intro.configured = qtrue;
	intro.fadeIn = fadeIn < 0 ? 0 : fadeIn;
	intro.hold = hold < 0 ? 0 : hold;
	intro.fadeOut = fadeOut < 0 ? 0 : fadeOut;
	intro.stagger = stagger < 0 ? 0 : stagger;
	intro.freeze = freeze;
	intro.hasMission = ( mission && mission[0] ) ? qtrue : qfalse;
	intro.numLines = numLines + ( intro.hasMission ? 1 : 0 );

	for ( i = 0; i < numLines; i++ ) {
		if ( i ) {
			Q_strcat( intro.text, sizeof( intro.text ), "\n" );
		}
		Q_strcat( intro.text, sizeof( intro.text ), lines[i] );
	}
	if ( intro.hasMission ) {
		if ( numLines ) {
			Q_strcat( intro.text, sizeof( intro.text ), "\n" );
		}
		Q_strcat( intro.text, sizeof( intro.text ), mission );
	}

	// only start right away if a player is already loaded and playing - otherwise G_Intro_ClientBegin starts it
	if ( introAnyClientBegun ) {
		char cmd[sizeof( intro.text ) + 128];

		G_Intro_Activate();
		G_Intro_BuildCommand( cmd, sizeof( cmd ) );
		trap_SendServerCommand( -1, cmd );
	}
}

/*
============
G_Intro_ClientBegin

  Called from ClientBegin for every real (non-AI) client. Starts the clock on
  the first player to begin, or catches a later joiner up to the sequence.
============
*/
void G_Intro_ClientBegin( int clientNum ) {
	char cmd[sizeof( intro.text ) + 128];

	introAnyClientBegun = qtrue;

	if ( !intro.configured ) {
		return;
	}
	if ( !intro.active ) {
		G_Intro_Activate();
	} else if ( level.time >= intro.endTime ) {
		return; // already finished, don't replay it for a later joiner
	}

	G_Intro_BuildCommand( cmd, sizeof( cmd ) );
	trap_SendServerCommand( clientNum, cmd );
}

/*
============
G_Intro_Tick

  Called once per server frame from G_RunFrame.
============
*/
void G_Intro_Tick( void ) {
	if ( !intro.active ) {
		return;
	}
	if ( level.time >= intro.endTime ) {
		intro.active = qfalse;
	}
}

/*
============
G_Intro_Active

  True while players should be held in PM_FREEZE for the titles.
============
*/
qboolean G_Intro_Active( void ) {
	return ( intro.active && intro.freeze && level.time < intro.freezeEndTime );
}
