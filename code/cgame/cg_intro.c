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

// cg_intro.c - client side of the "introtitle" server command (see g_intro.c).

#include "cg_local.h"
#include "../ui/ui_shared.h"

/*
============
CG_Intro_Parse

  Handles the "introtitle <start> <fadein> <hold> <fadeout> <stagger> <freeze>
  <hasMission> "<'\n'-joined lines>"" server command.
============
*/
void CG_Intro_Parse( void ) {
	char buf[MAX_INTRO_LINES * MAX_INTRO_LINE_LEN];
	char *line;
	int visiblePhaseEnd;

	if ( trap_Argc() < 9 ) {
		return;
	}

	cg.introStartTime  = atoi( CG_Argv( 1 ) );
	cg.introFadeIn     = atoi( CG_Argv( 2 ) );
	cg.introHold       = atoi( CG_Argv( 3 ) );
	cg.introFadeOut    = atoi( CG_Argv( 4 ) );
	cg.introStagger    = atoi( CG_Argv( 5 ) );
	cg.introFreeze     = atoi( CG_Argv( 6 ) ) ? qtrue : qfalse;
	cg.introHasMission = atoi( CG_Argv( 7 ) ) ? qtrue : qfalse;

	Q_strncpyz( buf, CG_Argv( 8 ), sizeof( buf ) );

	cg.introNumLines = 0;
	memset( cg.introLines, 0, sizeof( cg.introLines ) );

	line = buf;
	while ( line && cg.introNumLines < MAX_INTRO_LINES ) {
		char *nl = strchr( line, '\n' );
		if ( nl ) {
			*nl = '\0';
		}
		// translated immediately, same as the "cp"/"cpst" handlers in cg_servercmds.c
		Q_strncpyz( cg.introLines[cg.introNumLines], CG_TranslateString( line ), MAX_INTRO_LINE_LEN );
		cg.introNumLines++;
		line = nl ? nl + 1 : NULL;
	}

	// keep in sync with G_Intro_Activate's endTime math in g_intro.c
	visiblePhaseEnd = ( cg.introNumLines - 1 ) * cg.introStagger + cg.introFadeIn;
	cg.introFadeOutStart = cg.introStartTime + visiblePhaseEnd + cg.introHold;
	cg.introEndTime = cg.introFadeOutStart + cg.introFadeOut;

	cg.introActive = qtrue;
	cg.introFadeOutSent = qfalse;

	// screen goes black immediately; duration 1 not 0 since CG_DrawFlashFade divides by it
	CG_Fade( 0, 0, 0, 255, cg.introStartTime, 1 );
}

/*
============
CG_Intro_Active

  True while the titles (and the black screen) should still be showing.
============
*/
qboolean CG_Intro_Active( void ) {
	return ( cg.introActive && cg.time < cg.introEndTime );
}

/*
============
CG_DrawIntroTitles
============
*/
void CG_DrawIntroTitles( void ) {
	int i;
	int elapsed;

	if ( !cg.introActive ) {
		return;
	}
	if ( cg.time >= cg.introEndTime ) {
		cg.introActive = qfalse;
		return;
	}

	// fade back to gameplay exactly once, right as the hold phase ends
	if ( !cg.introFadeOutSent && cg.time >= cg.introFadeOutStart ) {
		CG_Fade( 0, 0, 0, 0, cg.introFadeOutStart, cg.introFadeOut > 0 ? cg.introFadeOut : 1 );
		cg.introFadeOutSent = qtrue;
	}

	elapsed = cg.time - cg.introStartTime;

	if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement( PLACE_LEFT, PLACE_BOTTOM );
	}

	for ( i = 0; i < cg.introNumLines; i++ ) {
		qboolean isMission = ( cg.introHasMission && i == cg.introNumLines - 1 );
		int lineStart = i * cg.introStagger;
		float alphaIn, alphaOut, alpha, scale;
		vec4_t color;
		int y;

		if ( elapsed < lineStart ) {
			continue;
		}

		if ( cg.introFadeIn > 0 ) {
			alphaIn = Com_Clamp( 0.0f, 1.0f, (float)( elapsed - lineStart ) / cg.introFadeIn );
		} else {
			alphaIn = 1.0f;
		}

		if ( cg.time < cg.introFadeOutStart ) {
			alphaOut = 1.0f;
		} else if ( cg.introFadeOut > 0 ) {
			alphaOut = Com_Clamp( 0.0f, 1.0f, (float)( cg.introEndTime - cg.time ) / cg.introFadeOut );
		} else {
			alphaOut = 0.0f;
		}

		alpha = alphaIn * alphaOut;
		if ( alpha <= 0.0f ) {
			continue;
		}

		scale = isMission ? 0.55f : 0.30f;
		if ( isMission ) {
			color[0] = 1.0f; color[1] = 0.82f; color[2] = 0.30f;
		} else {
			color[0] = color[1] = color[2] = 0.85f;
		}
		color[3] = alpha;

		// stack upward from the lower-left corner; the mission line (if any) sits lowest
		y = 420 - ( cg.introNumLines - 1 - i ) * 30;

		CG_Text_Paint( 56, y, UI_FONT_DEFAULT, scale, color, cg.introLines[i], 0, 0, ITEM_TEXTSTYLE_SHADOWED );
	}

	if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement( PLACE_CENTER, PLACE_CENTER );
	}
}
