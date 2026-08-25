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

// cg_scoreboard -- draw the scoreboard on top of the game screen
#include "cg_local.h"
#include "../ui/ui_shared.h"

#define SCOREBOARD_WIDTH    ( 31 * BIGCHAR_WIDTH )

// row/column text - scale derived from CG_CHAT_TEXT_SCALE (cg_local.h) doubled for SMALLCHAR_HEIGHT vs TINYCHAR_HEIGHT
#define SCOREBOARD_FONT        UI_FONT_COURBD_12
#define SCOREBOARD_SCALE       0.38f
// section titles / "Killed by" banner - reuses the proven HUD numeric readout pairing
#define SCOREBOARD_TITLE_FONT  UI_FONT_COURBD_24
#define SCOREBOARD_TITLE_SCALE 0.32f

// CG_Text_Paint multiplies color[3] by cg_hudAlpha in place; copy first so repeated calls with a shared array don't compound
static void CG_ScoreboardText( float x, float y, int font, float scale, const float *color, const char *text, int limit, int style ) {
	vec4_t c;
	Vector4Copy( color, c );
	CG_Text_Paint( x, y, font, scale, c, text, 0, limit, style );
}

void CG_DrawCoopScoreboard( void ) {
	int i, place = 0, w;
	float *color;       // faded color based on cursor hint drawing
	float color2[4] = {0, 0, 0, 1};
	float color3[4] = {1, 1, 1, 1};
	float black[4] = {0, 0, 0, 1};
	clientInfo_t *ci;
	long score;
	int ping;
#ifdef MONEY
	int highest_score;
	float green[4] = {0, 1, 0, 1};
#endif
	const char *s;
	const char *className;
	static const char *classNames[4] = { "Soldier", "Medic", "Engineer", "Lieutenant" };
	int msec, mins, seconds, tens;     // JPW NERVE
	qboolean maxlives = qfalse;

		#define MAX_STATS_VARS  64

	if ( cg_paused.integer ) {
		// no draw if any menu's are up  (or otherwise paused)
		return;
	}

	color = colorWhite;

	if ( !color ) {     // currently faded out, don't draw
		return;
	}

	color2[3] = color[3];

	// patched for MAX_COOP_PLAYERS (4) players.
	// background
	color2[3] *= 0.6f;
	CG_FilledBar( 80, 104, 480, 168, color2, NULL, NULL, 1.0f, 0 );

	color2[0] = color2[1] = color2[2] = 0.3f;
	color2[3] *= 0.6f;

	// border
	CG_FilledBar( 78, 104, 2, 168, color2, NULL, NULL, 1.0f, 0 );        // left
	CG_FilledBar( 560, 104, 2, 168, color2, NULL, NULL, 1.0f, 0 );        // right
	CG_FilledBar( 78, 102, 484, 2, color2, NULL, NULL, 1.0f, 0 );        // top
	CG_FilledBar( 80, 124, 482, 2, color2, NULL, NULL, 1.0f, 0 );        // under green bar
	CG_FilledBar( 78, 272, 484, 2, color2, NULL, NULL, 1.0f, 0 );        // bot


	// text boxes
	color2[0] = color2[1] = color2[2] = 0.4f;
#ifdef LOCALISATION
	CG_ScoreboardText( 105, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, CG_TranslateString( "Name" ), 25, ITEM_TEXTSTYLE_SHADOWED );
#else
	CG_ScoreboardText( 105, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, "Name", 25, ITEM_TEXTSTYLE_SHADOWED );
#endif
#ifdef LOCALISATION
	CG_ScoreboardText( 280, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, CG_TranslateString( "Class" ), 25, ITEM_TEXTSTYLE_SHADOWED );
#else
	CG_ScoreboardText( 280, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, "Class", 25, ITEM_TEXTSTYLE_SHADOWED );
#endif
	w = CG_Text_Width( "Kills", SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
#ifdef LOCALISATION
	CG_ScoreboardText( 100 + 320 - w, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, CG_TranslateString( "Kills" ), 25, ITEM_TEXTSTYLE_SHADOWED );
#else
	CG_ScoreboardText( 100 + 320 - w, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, "Kills", 25, ITEM_TEXTSTYLE_SHADOWED );
#endif
	w = CG_Text_Width( "Score", SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
#ifdef LOCALISATION
	CG_ScoreboardText( 100 + 375 - w, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, CG_TranslateString( "Score" ), 25, ITEM_TEXTSTYLE_SHADOWED );
#else
	CG_ScoreboardText( 100 + 375 - w, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, "Score", 25, ITEM_TEXTSTYLE_SHADOWED );
#endif
	w = CG_Text_Width( "Ping", SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
#ifdef LOCALISATION
	CG_ScoreboardText( 100 + 420 - w, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, CG_TranslateString( "Ping" ), 25, ITEM_TEXTSTYLE_SHADOWED );
#else
	CG_ScoreboardText( 100 + 420 - w, 130, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, "Ping", 25, ITEM_TEXTSTYLE_SHADOWED );
#endif
	//if (cg.scores[0].respawnsLeft != -1)
	//        maxlives = qtrue;

	// first get highest values

#ifdef MONEY
	highest_score = 0;

	for ( i = 0; i < MAX_COOP_PLAYERS; i++ ) {
		if ( cg.scores[i].score > highest_score ) {
			highest_score = cg.scores[i].score;
		}
	}
#endif

	for ( i = 0; i < MAX_COOP_PLAYERS; i++ ) {
		if ( cg.clientNum == cg.scores[i].client && i < cg.numScores ) {
			CG_FilledBar( 100, 154 + ( 28 * i ), 440, 20, black, NULL, NULL, 1.0f, 0 );
		} else {
			CG_FilledBar( 100, 154 + ( 28 * i ), 440, 20, color2, NULL, NULL, 1.0f, 0 );
		}

		ci = &cgs.clientinfo[cg.scores[i].client];

		if ( i < cg.numScores ) {
			ping = cg.scores[i].ping;
			score = cg.scores[i].score;

			place++;
			if ( ci->team == TEAM_SPECTATOR ) {
#ifdef LOCALISATION
				CG_ScoreboardText( 105, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( CG_TranslateString( "[SPECTATOR] %s" ), ci->name ), 20, ITEM_TEXTSTYLE_SHADOWED );
#else
				CG_ScoreboardText( 105, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "[SPECTATOR] %s", ci->name ), 20, ITEM_TEXTSTYLE_SHADOWED );
#endif
			} else if ( ci->team == TEAM_RED ) {
				CG_ScoreboardText( 105, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "^1%i^7. %s", place, ci->name ), 20, ITEM_TEXTSTYLE_SHADOWED );
			} else {
				CG_ScoreboardText( 105, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "%i. %s", place, ci->name ), 20, ITEM_TEXTSTYLE_SHADOWED );
			}

			if ( ci->team != TEAM_SPECTATOR ) {
				className = ( cg.scores[i].playerClass >= 0 && cg.scores[i].playerClass < 4 ) ?
					classNames[cg.scores[i].playerClass] : "";
				CG_ScoreboardText( 280, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, className, 10, ITEM_TEXTSTYLE_SHADOWED );

				w = CG_Text_Width( va( "%d", cg.scores[i].kills ), SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
				CG_ScoreboardText( 100 + 320 - w, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "%d", cg.scores[i].kills ), 25, ITEM_TEXTSTYLE_SHADOWED );
			}

			if ( maxlives )
			{
				w = CG_Text_Width( va( "%ld (%d)", score, cg.scores[i].respawnsLeft ), SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
				if ( ci->team != TEAM_SPECTATOR )
				{
#ifdef MONEY
					if ( cg.scores[i].score == highest_score )
					{
						CG_ScoreboardText( 100 + 375 - w, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, green, va( "%d (%d)", cg.scores[i].score, cg.scores[i].respawnsLeft ), 25, ITEM_TEXTSTYLE_SHADOWED );
					}
					else
#endif
					{
						CG_ScoreboardText( 100 + 375 - w, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "%d (%d)", cg.scores[i].score, cg.scores[i].respawnsLeft ), 25, ITEM_TEXTSTYLE_SHADOWED );
					}
				}
			}
			else
			{
				w = CG_Text_Width( va( "%ld", score ), SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
				if ( ci->team != TEAM_SPECTATOR )
				{
#ifdef MONEY
					if ( cg.scores[i].score == highest_score )
					{
						CG_ScoreboardText( 100 + 375 - w, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, green, va( "%d", cg.scores[i].score ), 25, ITEM_TEXTSTYLE_SHADOWED );
					}
					else
#endif
					{
						CG_ScoreboardText( 100 + 375 - w, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "%d", cg.scores[i].score ), 25, ITEM_TEXTSTYLE_SHADOWED );
					}
				}
			}

			w = CG_Text_Width( va( "%d", ping ), SCOREBOARD_FONT, SCOREBOARD_SCALE, 0 );
			CG_ScoreboardText( 100 + 420 - w, 154 + ( 28 * i ) + 1, SCOREBOARD_FONT, SCOREBOARD_SCALE, color3, va( "%d", cg.scores[i].ping ), 25, ITEM_TEXTSTYLE_SHADOWED );
		}
	}


	// green title
	color2[0] = color2[2] = 0;
	color2[1] = 0.3f;
	CG_FilledBar( 80, 104, 480, 20, color2, NULL, NULL, 1.0f, 0 );

	color2[0] = color2[1] = color2[2] = 0.2f;

	// title
	color2[0] = color2[1] = color2[2] = 1;
	color2[3] = color[3];
	if ( cgs.gametype == GT_COOP ) {
#ifdef LOCALISATION
		w = CG_Text_Width( CG_TranslateString( "Score" ), SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, 0 );
		CG_ScoreboardText( 100 + ( 484 / 2 ) - ( w - 2 ), 105, SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, color3, CG_TranslateString( "Score" ), 25, ITEM_TEXTSTYLE_NORMAL );
#else
		w = CG_Text_Width( "scores", SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, 0 );
		CG_ScoreboardText( 100 + ( 484 / 2 ) - ( w - 2 ), 105, SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, color3, "scores", 25, ITEM_TEXTSTYLE_NORMAL );
#endif
	}



	msec = ( cg.time - cgs.levelStartTime );

	seconds = msec / 1000;
	mins = seconds / 60;
	seconds -= mins * 60;
	tens = seconds / 10;
	seconds -= tens * 10;

	s = va( "%2.0f:%i%i", (float)mins, tens, seconds );

	CG_ScoreboardText( 100, 105, SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, color3, s, 25, ITEM_TEXTSTYLE_NORMAL );

	color2[0] = color2[1] = color2[2] = 1;
}


/*
=================
CG_DrawScoreboard

Draw the normal in-game scoreboard
=================
*/
qboolean CG_DrawScoreboard( void ) {
	int x = 0, y = 0, w;
	float fade;
	float   *fadeColor;
	char    *s;

	// don't draw anything if the menu or console is up
	if ( cg_paused.integer ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	// game-over sequence: CG_Draw2D already force-draws this; bail so TAB doesn't stack a second copy on top
	if ( cgs.gametype == GT_COOP_SURVIVAL && cg.predictedPlayerState.stats[STAT_GAMEOVER] ) {
		return qfalse;
	}

	// still need to see 'mission failed' message in SP
	if ( cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_DEAD ) {
		return qfalse;
	}

	if ( cgs.gametype == GT_SINGLE_PLAYER && cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		cg.deferredPlayerLoading = 0;
		return qfalse;
	}

	// don't draw scoreboard during death while warmup up
	if ( cg.warmup && !cg.showScores && cg.predictedPlayerState.pm_type != PM_INTERMISSION ) {
		return qfalse;
	}

	if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement(PLACE_CENTER, PLACE_CENTER);
	}

	// bleeding out (GT_COOP_SURVIVAL) is PM_DEAD but still revivable -- don't auto-pop the scoreboard for that
	if ( cg.showScores ||
		 ( cg.predictedPlayerState.pm_type == PM_DEAD && cg.predictedPlayerState.stats[STAT_REVIVE_TIME] <= 0 ) ||
		 cg.predictedPlayerState.pm_type == PM_INTERMISSION ) {
		fade = 1.0;
	} else {
		fadeColor = CG_FadeColor( cg.scoreFadeTime, FADE_TIME );

		if ( !fadeColor ) {
			// next time scoreboard comes up, don't print killer
			cg.deferredPlayerLoading = 0;
			cg.killerName[0] = 0;
			return qfalse;
		}
		fade = *fadeColor;
	}


	// fragged by ... line
	if ( cg.killerName[0] ) {
		vec4_t killedByColor = { 1, 1, 1, fade };

		s = va( "Killed by %s", cg.killerName );
		w = CG_Text_Width( s, SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, 0 );
		x = ( SCREEN_WIDTH - w ) / 2;
		y = 40;
		CG_ScoreboardText( x, y, SCOREBOARD_TITLE_FONT, SCOREBOARD_TITLE_SCALE, killedByColor, s, 0, ITEM_TEXTSTYLE_SHADOWED );
	}

	// current rank

	if ( cgs.gametype <= GT_COOP ) {   //----(SA) modified
		/*cg.cursorHintTime = cg.time;
		cg.cursorHintFade = cg_hintFadeTime.integer;
		cg.exitStatsFade = 250;     // fade /up/ time
		if ( !cg.exitStatsTime ) {
		        cg.exitStatsTime = cg.time;
		}
		*/
		CG_DrawCoopScoreboard();
		//CG_DrawBigString( 30, 50, "COOP SCORES", 1.0 );
		//y = CG_TeamScoreboard( x, y, TEAM_FREE, fade );

		/*		s = va( "%s place with %i",
		                CG_PlaceString( cg.snap->ps.persistant[PERS_RANK] + 1 ),
		                cg.snap->ps.persistant[PERS_SCORE] );
		        w = CG_DrawStrlen( s ) * BIGCHAR_WIDTH;
		        x = ( SCREEN_WIDTH - w ) / 2;
		        y = 60;
		        CG_DrawBigString( x, y, s, fade );*/
	}

	// load any models that have been deferred
	if ( ++cg.deferredPlayerLoading > 1 ) {
		CG_LoadDeferredPlayers();
	}

	return qtrue;
}
