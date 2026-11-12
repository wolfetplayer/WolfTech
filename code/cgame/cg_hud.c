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



#include "cg_local.h"
#include "../ui/ui_shared.h"
#include "../qcommon/q_unicode.h"

extern displayContextDef_t cgDC;

// set in CG_ParseTeamInfo

int drawTeamOverlayModificationCount = -1;

// floating "+N"/"-N" popup beside the crosshair; rapid awards merge into one running total instead of stacking lines
#define SCOREPOP_DURATION   1200    // msec, counted from the last merge
#define SCOREPOP_RISE       24      // pixels risen once the popup goes cold

typedef struct {
	qboolean active;
	int amount;       // running total for the current burst
	int startTime;    // refreshed on every merge; anchors the rise/fade
} scoreBurst_t;

static scoreBurst_t scoreBurst;

// cg_drawScorePop values
#define SCOREPOP_OFF        0
#define SCOREPOP_ALL        1
#define SCOREPOP_NO_HITS    2

/*
============
CG_AddScorePop

Merges a score award into the current burst. isHit marks per-hit awards
(dropped when cg_drawScorePop is 2); a sign change starts a fresh burst
instead of merging a gain with a spend.
============
*/
void CG_AddScorePop( int amount, qboolean isHit ) {
	if ( !amount ) {
		return;
	}

	if ( cg_drawScorePop.integer <= SCOREPOP_OFF ) {
		return;
	}

	if ( cg_drawScorePop.integer >= SCOREPOP_NO_HITS && isHit ) {
		return;
	}

	if ( scoreBurst.active && ( ( scoreBurst.amount > 0 ) == ( amount > 0 ) ) ) {
		scoreBurst.amount += amount;
	} else {
		scoreBurst.active = qtrue;
		scoreBurst.amount = amount;
	}
	scoreBurst.startTime = trap_Milliseconds();
}

/*
============
CG_DrawScorePops

Draws the current score burst at the given anchor (left-aligned baseline),
fading/rising once it's gone cold. Gated by cg_drawScorePop: 0 off, 1 show
all, 2 hide hit-only (+1) spam (filtered earlier, in CG_AddScorePop).
============
*/
static void CG_DrawScorePops( rectDef_t *rect, int font, float scale ) {
	int currentTime;
	float progress, alpha, y;
	vec4_t color;
	char str[16];

	if ( cg_drawScorePop.integer <= SCOREPOP_OFF || !scoreBurst.active ) {
		return;
	}

	currentTime = trap_Milliseconds();

	progress = (float)( currentTime - scoreBurst.startTime ) / (float)SCOREPOP_DURATION;
	if ( progress >= 1.0f ) {
		scoreBurst.active = qfalse;
		return;
	}

	alpha = ( progress < 0.6f ) ? 1.0f : 1.0f - ( progress - 0.6f ) / 0.4f;

	if ( scoreBurst.amount > 0 ) {
		color[0] = 0.2f; color[1] = 1.0f; color[2] = 0.2f;
	} else {
		color[0] = 1.0f; color[1] = 0.2f; color[2] = 0.2f;
	}
	color[3] = alpha;

	Com_sprintf( str, sizeof( str ), "%s%i", scoreBurst.amount > 0 ? "+" : "", scoreBurst.amount );

	y = rect->y - SCOREPOP_RISE * progress;

	CG_Text_Paint( rect->x, y, font, scale, color, str, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE );
}

/*
============
CG_DrawScorePopsCrosshair

Draws the local player's score burst beside the crosshair. Drawn once here
rather than per row in CG_DrawPlayerInfo, since the server only notifies the
scoring client - per-row drawing made teammates' screens show points they
never earned.
============
*/
#define SCOREPOP_CROSSHAIR_X 320
#define SCOREPOP_CROSSHAIR_Y 244
#define SCOREPOP_SIDE_GAP    26     // gap from screen center to the popup's left edge

void CG_DrawScorePopsCrosshair( void ) {
	rectDef_t rect;

	if ( cgs.gametype != GT_COOP_SURVIVAL ) {
		return;
	}

	rect.x = SCOREPOP_CROSSHAIR_X + SCOREPOP_SIDE_GAP;
	rect.y = SCOREPOP_CROSSHAIR_Y;
	rect.w = 0;
	rect.h = 0;

	CG_DrawScorePops( &rect, UI_FONT_NORMAL, 0.26f );
}

static void CG_DrawPlayerArmorValue( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	char num[16];
	int value;
	playerState_t   *ps;

	ps = &cg.snap->ps;

	value = ps->stats[STAT_ARMOR];

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_LEFT, PLACE_BOTTOM);
	}

	if ( shader ) {
		trap_R_SetColor( color );
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
		trap_R_SetColor( NULL );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", value );
		value = CG_Text_Width( num, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
	}
}


static void CG_DrawPlayerArmorValueBar( rectDef_t *rect, vec4_t color, int align ) {
	float frac; 
	int flags = 0;

	playerState_t   *ps;

	ps = &cg.snap->ps;

	//color[3] = 0.5f;

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_LEFT, PLACE_BOTTOM);
	}

	if ( align != HUD_HORIZONTAL ) {
		flags |= 4;   // BAR_VERT
		flags |= 1;   // BAR_LEFT (left, when vertical means grow 'up')
	}
	frac = ps->stats[STAT_ARMOR] / (float) 100;

	if ( frac > 1.0 ) 
	{
	frac = 1.0;
	}

	CG_FilledBar( rect->x, rect->y, rect->w, rect->h, color, NULL, NULL, frac, flags );

	trap_R_SetColor( NULL );
// jpw
}

/*
==============
weapIconDrawSize
==============
*/
static int weapIconDrawSize( int weap ) {

	return GetWeaponTableData( weap )->iconDrawSize;
}


/*
==============
CG_DrawPlayerWeaponIcon
==============
*/
static void CG_DrawPlayerWeaponIcon( rectDef_t *rect, qboolean drawHighlighted, int align ) {
	int size;
	int realweap;                   // DHM - Nerve
	qhandle_t icon;
	float scale,halfScale;

	if ( !cg_drawIcons.integer ) {
		return;
	}

	if ( !cg_drawWeaponIcon.integer ) {
		return;
	}

	// special equipment already has its own HUD icon - don't duplicate it here
	switch ( cg.predictedPlayerState.weapon ) {
	case WP_DYNAMITE:
	case WP_LANDMINE:
	case WP_MEDCRATE:
	case WP_AIRSTRIKE:
	case WP_M7:
		return;
	default:
		break;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_RIGHT, PLACE_BOTTOM);
	}

	realweap = cg.predictedPlayerState.weapon;

	if ( cg.snap->ps.persistant[PERS_HWEAPON_USE] ) {
		icon = cgs.media.mg42WeaponIcon;
		size = WEAPON_ICON_NORMAL;
	} else {
		size = weapIconDrawSize( realweap );

		if ( !size ) {
			return;
		}

		if ( drawHighlighted ) {
			icon = cg_weapons[ realweap ].weaponIcon[1];
		} else {
			icon = cg_weapons[ realweap ].weaponIcon[0];
		}
	}



	// pulsing grenade icon to help the player 'count' in their head
	if ( cg.predictedPlayerState.grenadeTimeLeft ) {   // grenades and dynamite set this

		// tick sound plays regardless of which weapon icon is shown - still relevant while quick-cooking
		if ( realweap == WP_DYNAMITE ) {
			if ( ( ( cg.grenLastTime ) % 1000 ) > ( ( cg.predictedPlayerState.grenadeTimeLeft ) % 1000 ) ) {
				trap_S_StartLocalSound( cgs.media.grenadePulseSound4, CHAN_LOCAL_SOUND );
			}
		} else {
			if ( ( ( cg.grenLastTime ) % 1000 ) < ( ( cg.predictedPlayerState.grenadeTimeLeft ) % 1000 ) ) {
				switch ( cg.predictedPlayerState.grenadeTimeLeft / 1000 ) {
				case 3:
					trap_S_StartLocalSound( cgs.media.grenadePulseSound4, CHAN_LOCAL_SOUND );
					break;
				case 2:
					trap_S_StartLocalSound( cgs.media.grenadePulseSound3, CHAN_LOCAL_SOUND );
					break;
				case 1:
					trap_S_StartLocalSound( cgs.media.grenadePulseSound2, CHAN_LOCAL_SOUND );
					break;
				case 0:
					trap_S_StartLocalSound( cgs.media.grenadePulseSound1, CHAN_LOCAL_SOUND );
					break;
				}
			}
		}

		// but only grow the icon when it's actually the grenade being cooked, not the equipped weapon
		if ( realweap == WP_GRENADE_LAUNCHER || realweap == WP_GRENADE_PINEAPPLE || realweap == WP_DYNAMITE ) {
			scale = (float)( ( cg.predictedPlayerState.grenadeTimeLeft ) % 1000 ) / 100.0f;
			halfScale = scale * 0.5f;
		} else {
			scale = halfScale = 0;
		}

		cg.grenLastTime = cg.predictedPlayerState.grenadeTimeLeft;
	} else if ( cg.predictedPlayerState.specialGrenadeTimeLeft ) {
		// special quickgrenade cooking - same tick cadence, but never grows this icon
		if ( ( ( cg.specialGrenLastTime ) % 1000 ) < ( ( cg.predictedPlayerState.specialGrenadeTimeLeft ) % 1000 ) ) {
			switch ( cg.predictedPlayerState.specialGrenadeTimeLeft / 1000 ) {
			case 3:
				trap_S_StartLocalSound( cgs.media.grenadePulseSound4, CHAN_LOCAL_SOUND );
				break;
			case 2:
				trap_S_StartLocalSound( cgs.media.grenadePulseSound3, CHAN_LOCAL_SOUND );
				break;
			case 1:
				trap_S_StartLocalSound( cgs.media.grenadePulseSound2, CHAN_LOCAL_SOUND );
				break;
			case 0:
				trap_S_StartLocalSound( cgs.media.grenadePulseSound1, CHAN_LOCAL_SOUND );
				break;
			}
		}

		cg.specialGrenLastTime = cg.predictedPlayerState.specialGrenadeTimeLeft;
		scale = halfScale = 0;
	} else {
		scale = halfScale = 0;
	}


	if ( icon ) {
		float x, y, w, h;

		if ( size == 1 ) { // draw half width to match the icon asset

			// start at left
			x = rect->x - halfScale;
			y = rect->y - halfScale;
			w = rect->w / 2 + scale;
			h = rect->h + scale;

			switch ( align ) {
			case ITEM_ALIGN_CENTER:
				x += rect->w / 4;
				break;
			case ITEM_ALIGN_RIGHT:
				x += rect->w / 2;
				break;
			case ITEM_ALIGN_LEFT:
			default:
				break;
			}

		} else {
			x = rect->x - halfScale;
			y = rect->y - halfScale;
			w = rect->w + scale;
			h = rect->h + scale;
		}


		CG_DrawPic( x, y, w, h, icon );

		// upgrade level badge - small roman numeral in the icon's corner, hidden while manning a mounted gun
		if ( !cg.snap->ps.persistant[PERS_HWEAPON_USE] && CG_WeaponIsUpgraded( realweap ) ) {
			static const char *upgradeNumerals[4] = { "", "I", "II", "III" };
			int upgradeLevel = cg.snap->ps.weaponUpgraded[ realweap ];

			if ( upgradeLevel < 0 ) {
				upgradeLevel = 0;
			} else if ( upgradeLevel > 3 ) {
				upgradeLevel = 3;
			}

			if ( upgradeLevel > 0 ) {
				const char *numeral = upgradeNumerals[ upgradeLevel ];
				int badgeW = CG_Text_Width_Ext( numeral, CG_CHAT_TEXT_SCALE, 0, CG_CHAT_FONT );
				float badgeX = x + w - badgeW - 2;
				float badgeY = y + h - TINYCHAR_HEIGHT - 2;

				CG_Text_Paint_Ext( badgeX, badgeY, CG_CHAT_TEXT_SCALE, CG_CHAT_TEXT_SCALE, colorWhite, numeral, 0, 0, ITEM_TEXTSTYLE_SHADOWED, CG_CHAT_FONT );
			}
		}
	}
}


/*
==============
CG_DrawPlayerWeaponName

Weapon name text, with upgrade level appended (e.g. "Thompson II").
==============
*/
static void CG_DrawPlayerWeaponName( rectDef_t *rect, int font, float scale, vec4_t color, int textStyle ) {
	int realweap;
	const char *baseName;
	char name[64];
	float w;

	if ( !cg_drawIcons.integer ) {
		return;
	}

	// equip HUD icon already shows these
	switch ( cg.predictedPlayerState.weapon ) {
	case WP_DYNAMITE:
	case WP_LANDMINE:
	case WP_MEDCRATE:
	case WP_AIRSTRIKE:
	case WP_M7:
		return;
	default:
		break;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement( PLACE_RIGHT, PLACE_BOTTOM );
	}

	realweap = cg.predictedPlayerState.weapon;

	if ( cg.snap->ps.persistant[PERS_HWEAPON_USE] ) {
		baseName = "MG 42";
	} else {
		if ( !weapIconDrawSize( realweap ) || !cg_weapons[realweap].item ) {
			return;
		}
		baseName = cg_weapons[realweap].item->pickup_name;
	}

	if ( !baseName || !baseName[0] ) {
		return;
	}

	Q_strncpyz( name, baseName, sizeof( name ) );

	if ( !cg.snap->ps.persistant[PERS_HWEAPON_USE] && CG_WeaponIsUpgraded( realweap ) ) {
		static const char *upgradeNumerals[4] = { "", "I", "II", "III" };
		int upgradeLevel = cg.snap->ps.weaponUpgraded[ realweap ];

		if ( upgradeLevel < 0 ) {
			upgradeLevel = 0;
		} else if ( upgradeLevel > 3 ) {
			upgradeLevel = 3;
		}

		if ( upgradeLevel > 0 ) {
			Com_sprintf( name, sizeof( name ), "%s %s", baseName, upgradeNumerals[upgradeLevel] );
		}
	}

	w = CG_Text_Width( name, font, scale, 0 );
	CG_Text_Paint( rect->x + ( rect->w - w ) / 2, rect->y + rect->h, font, scale, color, name, 0, 0, textStyle );
}

/*
==============
CG_DrawPlayerAmmoIcon
==============
*/
static void CG_DrawPlayerAmmoIcon( rectDef_t *rect, qboolean draw2D ) {
	centity_t   *cent;
	vec3_t angles;
	vec3_t origin;

	cent = &cg_entities[cg.snap->ps.clientNum];

	// TTimo: gcc: suggests () around && within ||
	if ( draw2D || ( !cg_draw3dIcons.integer && cg_drawIcons.integer ) ) {
		qhandle_t icon;
		icon = cg_weapons[ cg.predictedPlayerState.weapon ].ammoIcon;
		if ( icon ) {
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, icon );
		}
	} else if ( cg_draw3dIcons.integer ) {
		if ( cent->currentState.weapon && cg_weapons[ cent->currentState.weapon ].ammoModel ) {
			VectorClear( angles );
			origin[0] = 70;
			origin[1] = 0;
			origin[2] = 0;
			angles[YAW] = 90 + 20 * sin( cg.time / 1000.0 );
			CG_Draw3DModel( rect->x, rect->y, rect->w, rect->h, cg_weapons[ cent->currentState.weapon ].ammoModel, 0, origin, angles );
		}
	}
}

#define CURSORHINT_SCALE    10

/*
==============
CG_DrawCursorHints

  cg_cursorHints.integer ==
    0:	no hints
    1:	sin size pulse
    2:	one way size pulse
    3:	alpha pulse
    4+:	static image

==============
*/

static void CG_DrawCursorhint( rectDef_t *rect ) {
	float       *color;
	qhandle_t icon, icon2 = 0;
	float scale, halfscale;
	qboolean redbar = qfalse;

	if ( !cg_cursorHints.integer ) {
		return;
	}

	CG_CheckForCursorHints();

	icon = cgs.media.hintShaders[cg.cursorHintIcon];

	if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement(PLACE_CENTER, PLACE_CENTER);
	}

	switch ( cg.cursorHintIcon ) {
	case HINT_NONE:
	case HINT_FORCENONE:
		icon = 0;
		break;
	//case HINT_KNIFE:
	//        icon = cgs.media.hintShaders[HINT_KNIFE];
	//        break;
	case HINT_BREAKABLE_DYNAMITE:
		icon = cgs.media.hintShaders[HINT_BREAKABLE];
		break;
	case HINT_CHAIR:
		// only show 'pickupable' if you're not armed, or are armed with a single handed weapon
		if ( cg.predictedPlayerState.weapon && !( WEAPS_ONE_HANDED & ( 1LL << ( cg.predictedPlayerState.weapon ) ) ) ) { // (SA) this was backwards
			icon = cgs.media.hintShaders[HINT_NOACTIVATE];
		}
		break;

	case HINT_PLAYER:
		icon = cgs.media.hintShaders[HINT_NOACTIVATE];
		break;

	case HINT_PLYR_FRIEND:
		break;

	case HINT_NOEXIT_FAR:
		redbar = qtrue;     // draw the status bar in red to show that you can't exit yet
	case HINT_EXIT_FAR:
		break;

	case HINT_NOEXIT:
		redbar = qtrue;     // draw the status bar in red to show that you can't exit yet
	case HINT_EXIT:
		cg.exitStatsFade = 250;     // fade /up/ time
		if ( !cg.exitStatsTime ) {
			cg.exitStatsTime = cg.time;
		}
		CG_DrawExitStats();
		break;

	default:
		break;
	}

	if ( !icon ) {
		return;
	}

	// color
	color = CG_FadeColor( cg.cursorHintTime, cg.cursorHintFade );
	if ( !color ) {
		trap_R_SetColor( NULL );
		cg.exitStatsTime = 0;   // exit stats will fade up next time they're hit
		cg.cursorHintIcon = HINT_NONE;  // clear the hint
		return;
	}

	if ( cg_cursorHints.integer == 3 ) {
		color[3] *= 0.5 + 0.5 * sin( (float)cg.time / 150.0 );
	}

	// size
	if ( cg_cursorHints.integer >= 3 ) {   // no size pulsing
		scale = halfscale = 0;
	} else {
		if ( cg_cursorHints.integer == 2 ) {
			scale = (float)( ( cg.cursorHintTime ) % 1000 ) / 100.0f; // one way size pulse
		} else {
			scale = CURSORHINT_SCALE * ( 0.5 + 0.5 * sin( (float)cg.time / 150.0 ) ); // sin pulse

		}
		halfscale = scale * 0.5f;
	}

	// set color and draw the hint
	trap_R_SetColor( color );
	CG_DrawPic( rect->x - halfscale, rect->y - halfscale, rect->w + scale, rect->h + scale, icon );

	if ( icon2 ) {
		CG_DrawPic( rect->x - halfscale, rect->y - halfscale, rect->w + scale, rect->h + scale, icon2 );
	}

	trap_R_SetColor( NULL );

	// draw status bar under the cursor hint (hintVal on HINT_REVIVE/HINT_BUILD is an entity/clientNum, not a 0-255 fraction)
	if ( cg.cursorHintIcon != HINT_REVIVE && cg.cursorHintIcon != HINT_BUILD && cg.cursorHintValue && ( !( cg.time - cg.cursorHintTime ) ) ) {    // don't fade bar out w/ hint icon
		if ( redbar ) {
			Vector4Set( color, 1, 0, 0, 0.5f );
		} else {
			Vector4Set( color, 0, 0, 1, 0.5f );
		}
		CG_FilledBar( rect->x, rect->y + rect->h + 4, rect->w, 8, color, NULL, NULL, (float)cg.cursorHintValue / 255.0f, 0 );
	}

}

/*
==============
CG_DrawMessageIcon
==============
*/
//----(SA)	added

static void CG_DrawMessageIcon( rectDef_t *rect ) {
	int icon;

	if ( !cg_youGotMail.integer ) {
		return;
	}

	if ( cg_youGotMail.integer == 2 ) {
		icon = cgs.media.youGotObjectiveShader;
	} else {
		icon = cgs.media.youGotMailShader;
	}

	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, icon );
}



/*
==============
CG_DrawPlayerAmmoValue
    0 - ammo
    1 - clip
==============
*/
static void CG_DrawPlayerAmmoValue( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle, int type ) {
	char num[16];
	int value;
	centity_t   *cent;
	playerState_t   *ps;
	int weap;

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	weap = cent->currentState.weapon;

	if ( !weap ) {
		return;
	}

	if ( ps->weaponstate == WEAPON_RELOADING && type != 0 ) {
		return;
	}

	// venom powerup grants unlimited ammo - nothing meaningful to show
	if ( weap == WP_VENOM && ps->powerups[PW_VENOM] ) {
		return;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_RIGHT, PLACE_BOTTOM);
	}

	switch ( weap ) {      // some weapons don't draw ammo count text
	case WP_KNIFE:
		if ( type == 1 ) {  // don't draw reserve value, just clip (since these weapons have all their ammo in the clip)
			return;
		}
		break;

	case WP_DYNAMITE:      // charge-based (cooldown-gated), no meaningful clip/reserve count
	case WP_AIRSTRIKE:
	case WP_MEDCRATE:
	case WP_LANDMINE:      // real clip count, but already shown by the equipment HUD icon
	case WP_M7:            // reserve grenade count, already shown by the equipment HUD icon
		return;

	case WP_GRENADE_LAUNCHER:
	case WP_GRENADE_PINEAPPLE:
	case WP_POISONGAS:
	case WP_SMOKEBOMB:
	case WP_TESLA:
	case WP_FLAMETHROWER:
		if ( type == 0 ) {  // don't draw reserve value, just clip (since these weapons have all their ammo in the clip)
			return;
		}
		break;

	default:
		break;
	}


	if ( type == 0 ) { // ammo
		value = cg.snap->ps.ammo[BG_FindAmmoForWeapon( weap )];
	} else {        // clip
		value = ps->ammoclip[BG_FindClipForWeapon( weap )];
	}

	if ( value > -1 ) {
		if ( shader ) {
			trap_R_SetColor( color );
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
			trap_R_SetColor( NULL );
		} else {
			Com_sprintf( num, sizeof( num ), "%i", value );
			value = CG_Text_Width( num, font, scale, 0 );
			// clip and reserve share one baseline now
			CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
		}
	}
}

/*
==============
CG_DrawPlayerAmmoCombined

Single "clip | reserve" readout (e.g. "9 | 83") instead of two numbers.
==============
*/
static void CG_DrawPlayerAmmoCombined( rectDef_t *rect, int font, float scale, vec4_t color, int textStyle ) {
	char text[32];
	centity_t   *cent;
	playerState_t   *ps;
	int weap;
	int clip, ammo;

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;
	weap = cent->currentState.weapon;

	if ( !weap ) {
		return;
	}

	if ( ps->weaponstate == WEAPON_RELOADING ) {
		return;
	}

	// venom powerup grants unlimited ammo - nothing meaningful to show
	if ( weap == WP_VENOM && ps->powerups[PW_VENOM] ) {
		return;
	}

	switch ( weap ) {
	case WP_DYNAMITE:      // cooldown-based, no clip/reserve to show
	case WP_AIRSTRIKE:
	case WP_MEDCRATE:
	case WP_LANDMINE:      // equip HUD icon shows this already
	case WP_M7:            // equip HUD icon shows this already
		return;
	default:
		break;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement( PLACE_RIGHT, PLACE_BOTTOM );
	}

	clip = ps->ammoclip[ BG_FindClipForWeapon( weap ) ];
	ammo = ps->ammo[ BG_FindAmmoForWeapon( weap ) ];

	switch ( weap ) {
	case WP_KNIFE:
		Com_sprintf( text, sizeof( text ), "%i", ammo );
		break;
	case WP_GRENADE_LAUNCHER:
	case WP_GRENADE_PINEAPPLE:
	case WP_POISONGAS:
	case WP_SMOKEBOMB:
	case WP_TESLA:
	case WP_FLAMETHROWER:
		Com_sprintf( text, sizeof( text ), "%i", clip );
		break;

	default:
		Com_sprintf( text, sizeof( text ), "%i | %i", clip, ammo );
		break;
	}

	// centered, matching the grenade rows and weapon name below
	{
		float w = CG_Text_Width( text, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - w ) / 2, rect->y + rect->h, font, scale, color, text, 0, 0, textStyle );
	}
}

/*
==============
CG_GrenadeShortLabel

Short label for a grenade readout row - item->pickup_name is too wide.
==============
*/
static const char *CG_GrenadeShortLabel( int wp ) {
	switch ( wp ) {
	case WP_GRENADE_LAUNCHER:
		return "Grenade";
	case WP_GRENADE_PINEAPPLE:
		return "Pineapple";
	case WP_POISONGAS:
		return "Gas";
	case WP_SMOKEBOMB:
		return "Smoke";
	default:
		return "";
	}
}

/*
==============
CG_DrawGrenadeReadoutRow

Count then short label - shared by CG_DrawGrenadeCount and
CG_DrawSpecialGrenadeCount below. No icon: the count itself pulses while
cooking instead.
==============
*/
static void CG_DrawGrenadeReadoutRow( rectDef_t *rect, int font, float scale, vec4_t color, int textStyle, int wp, int value, float pulse ) {
	char num[16];
	const char *name;
	float x, numW, numWPulsed, numScale, gap, nameW, nameScale, totalW;

	name = CG_GrenadeShortLabel( wp );

	gap = rect->h * 0.3f;
	nameScale = scale * 0.55f;
	numScale = scale * ( 1.0f + 0.3f * pulse );

	Com_sprintf( num, sizeof( num ), "%i", value );
	numW = CG_Text_Width( num, font, scale, 0 );            // nominal width - keeps the row layout stable while pulsing
	numWPulsed = CG_Text_Width( num, font, numScale, 0 );
	nameW = ( name && name[0] ) ? CG_Text_Width( name, font, nameScale, 0 ) : 0.0f;

	// center the whole row, matching CG_DrawPlayerAmmoCombined below it
	totalW = numW + gap + nameW;
	x = rect->x + ( rect->w - totalW ) / 2;

	// grows symmetrically around its own slot instead of shifting the label next to it
	CG_Text_Paint( x - 0.5f * ( numWPulsed - numW ), rect->y + rect->h, font, numScale, color, num, 0, 0, textStyle );
	x += numW + gap;

	if ( name && name[0] ) {
		CG_Text_Paint( x, rect->y + rect->h, font, nameScale, color, name, 0, 0, textStyle );
	}
}

/*
==============
CG_DrawGrenadeCount

Always-visible grenade ammo readout.
==============
*/
static void CG_DrawGrenadeCount( rectDef_t *rect, int font, float scale, vec4_t color, int textStyle ) {
	playerState_t *ps;
	int wp;
	int value;
	float pulse;

	ps = &cg.snap->ps;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	if ( COM_BitCheck( ps->weapons, WP_GRENADE_LAUNCHER ) ) {
		wp = WP_GRENADE_LAUNCHER;
	} else if ( COM_BitCheck( ps->weapons, WP_GRENADE_PINEAPPLE ) ) {
		wp = WP_GRENADE_PINEAPPLE;
	} else {
		return;
	}

	if ( cgs.dmflags & DF_NO_WEAPRELOAD ) {
		value = ps->ammo[ BG_FindAmmoForWeapon( wp ) ];
	} else {
		value = ps->ammoclip[ BG_FindClipForWeapon( wp ) ];
	}

	if ( value <= 0 ) {
		return;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement( PLACE_RIGHT, PLACE_BOTTOM );
	}

	// icons are normally only registered lazily when their bank shows in the weapon-select strip
	CG_RegisterWeapon( wp );

	// pulse while actually cooking (quick-grenade or held grenade weapon)
	pulse = 0.0f;
	if ( cg.predictedPlayerState.grenadeTimeLeft > 0 ) {
		pulse = 1.0f - ( (float)( cg.predictedPlayerState.grenadeTimeLeft % 1000 ) / 1000.0f );
	}

	CG_DrawGrenadeReadoutRow( rect, font, scale, color, textStyle, wp, value, pulse );
}

/*
==============
CG_DrawSpecialGrenadeCount

Same as CG_DrawGrenadeCount, for the held special grenade - gas or smoke,
whichever the player actually has (never both).
==============
*/
static void CG_DrawSpecialGrenadeCount( rectDef_t *rect, int font, float scale, vec4_t color, int textStyle ) {
	playerState_t *ps;
	int wp;
	int value;
	float pulse;

	ps = &cg.snap->ps;

	if ( ps->stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	if ( COM_BitCheck( ps->weapons, WP_POISONGAS ) ) {
		wp = WP_POISONGAS;
	} else if ( COM_BitCheck( ps->weapons, WP_SMOKEBOMB ) ) {
		wp = WP_SMOKEBOMB;
	} else {
		return;
	}

	if ( cgs.dmflags & DF_NO_WEAPRELOAD ) {
		value = ps->ammo[ BG_FindAmmoForWeapon( wp ) ];
	} else {
		value = ps->ammoclip[ BG_FindClipForWeapon( wp ) ];
	}

	if ( value <= 0 ) {
		return;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement( PLACE_RIGHT, PLACE_BOTTOM );
	}

	CG_RegisterWeapon( wp );

	// pulse while cooking
	pulse = 0.0f;
	if ( cg.predictedPlayerState.specialGrenadeTimeLeft > 0 ) {
		pulse = 1.0f - ( (float)( cg.predictedPlayerState.specialGrenadeTimeLeft % 1000 ) / 1000.0f );
	}

	CG_DrawGrenadeReadoutRow( rect, font, scale, color, textStyle, wp, value, pulse );
}

static void CG_DrawPlayerHead( rectDef_t *rect, qboolean draw2D ) {
	vec3_t angles;
	float size, stretch;
	float frac;
	float x = rect->x;

	VectorClear( angles );

	if ( cg.damageTime && cg.time - cg.damageTime < DAMAGE_TIME ) {
		frac = (float)( cg.time - cg.damageTime ) / DAMAGE_TIME;
		size = rect->w * 1.25 * ( 1.5 - frac * 0.5 );

		stretch = size - rect->w * 1.25;
		// kick in the direction of damage
		x -= stretch * 0.5 + cg.damageX * stretch * 0.5;

		cg.headStartYaw = 180 + cg.damageX * 45;

		cg.headEndYaw = 180 + 20 * cos( crandom() * M_PI );
		cg.headEndPitch = 5 * cos( crandom() * M_PI );

		cg.headStartTime = cg.time;
		cg.headEndTime = cg.time + 100 + random() * 2000;
	} else {
		if ( cg.time >= cg.headEndTime ) {
			// select a new head angle
			cg.headStartYaw = cg.headEndYaw;
			cg.headStartPitch = cg.headEndPitch;
			cg.headStartTime = cg.headEndTime;
			cg.headEndTime = cg.time + 100 + random() * 2000;

			cg.headEndYaw = 180 + 20 * cos( crandom() * M_PI );
			cg.headEndPitch = 5 * cos( crandom() * M_PI );
		}
	}

	// if the server was frozen for a while we may have a bad head start time
	if ( cg.headStartTime > cg.time ) {
		cg.headStartTime = cg.time;
	}

	frac = ( cg.time - cg.headStartTime ) / (float)( cg.headEndTime - cg.headStartTime );
	frac = frac * frac * ( 3 - 2 * frac );
	angles[YAW] = cg.headStartYaw + ( cg.headEndYaw - cg.headStartYaw ) * frac;
	angles[PITCH] = cg.headStartPitch + ( cg.headEndPitch - cg.headStartPitch ) * frac;

	CG_DrawHead( x, rect->y, rect->w, rect->h, cg.snap->ps.clientNum, angles );
}

static void CG_DrawPlayerLocation( rectDef_t *rect, int font, float scale, vec4_t color, int textStyle  ) {
	clientInfo_t *ci = &cgs.clientinfo[cg.snap->ps.clientNum];
	if ( ci ) {
		const char *p = CG_ConfigString( CS_LOCATIONS + ci->location );
		if ( !p || !*p ) {
			p = "unknown";
		}
		CG_Text_Paint( rect->x, rect->y + rect->h, font, scale, color, p, 0, 0, textStyle );
	}
}

/*
===============
CG_PlayerInfoPerkLevel

Returns a player's perk level (0/1/2); self reads playerState_t directly,
others decode it from coopinfo's perkBits.
===============
*/
static int CG_PlayerInfoPerkLevel( clientInfo_t *ci, int perk ) {
	if ( ci->clientNum == cg.snap->ps.clientNum ) {
		return cg.snap->ps.perks[perk];
	}
	return ( ci->perkBits >> ( ( perk - 1 ) * 2 ) ) & 3;
}

/*
===============
CG_PlayerInfoClass

Returns a player's class; self reads playerState_t directly, others come
from coopinfo.
===============
*/
static int CG_PlayerInfoClass( clientInfo_t *ci ) {
	if ( ci->clientNum == cg.snap->ps.clientNum ) {
		return cg.snap->ps.stats[STAT_PLAYER_CLASS];
	}
	return ci->playerClass;
}

/*
===============
CG_PlayerInfoHealthColor

White-yellow-red ramp, kept separate from CG_ColorForHealthbar so enemy
health bars (which still use that one) aren't affected.
===============
*/
static void CG_PlayerInfoHealthColor( float frac, vec4_t color ) {
	if ( frac < 0.0f ) {
		frac = 0.0f;
	} else if ( frac > 1.0f ) {
		frac = 1.0f;
	}

	if ( frac < 0.5f ) {
		color[0] = 1.0f;
		color[1] = frac / 0.5f;
		color[2] = 0.0f;
	} else {
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = ( frac - 0.5f ) / 0.5f;
	}
	color[3] = 1.0f;
}

/*
===============
CG_DrawPlayerInfoRow

Draws one entry of the player-info stack: points on top (Survival only),
health bar in the middle, nickname + talk icon on the bottom line.
===============
*/
#define PLAYERINFO_ROW_W     150
#define PLAYERINFO_ROW_H     42
#define PLAYERINFO_ROW_GAP   8
#define PLAYERINFO_PANEL_RAD 6

#define PLAYERINFO_ICON_W    10
#define PLAYERINFO_ICON_H    20

#define PLAYERINFO_PERK_START 45
#define PLAYERINFO_PERK_Y_OFS -4
#define PLAYERINFO_PERK_ICON  14
#define PLAYERINFO_PERK_GAP   3

#define PLAYERINFO_BAR_W     100
#define PLAYERINFO_BAR_H     7

#define PLAYERINFO_CLASS_ICON 10
#define PLAYERINFO_CLASS_GAP  3

static void CG_DrawPlayerInfoRow( float x, float y, clientInfo_t *ci, int score, int health, int maxHealth, int armor ) {
	char nameClean[MAX_QPATH];
	char num[16];
	vec4_t bgcolor, nameColor, barColor, armorColor, textColor;
	float frac, armorFrac, panelY, pointsY, barY, nameY, nameX, nameW;

	if ( !ci->infoValid ) {
		return;
	}

	// fixed to y so adjusting pointsY alone can't move the rest
	panelY = y + 20;
	pointsY = y + 25;
	barY = y + 38;

	// larger, darker rect behind gives a cheap 1px border effect
	bgcolor[0] = bgcolor[1] = bgcolor[2] = 0.0f;
	bgcolor[3] = 0.6f * cg_hudAlpha.value;
	CG_FillRectRounded( x - 7, panelY - 1, PLAYERINFO_ROW_W + 2, PLAYERINFO_ROW_H + 2, PLAYERINFO_PANEL_RAD + 1, bgcolor );

	bgcolor[3] = 0.35f * cg_hudAlpha.value;
	CG_FillRectRounded( x - 6, panelY, PLAYERINFO_ROW_W, PLAYERINFO_ROW_H, PLAYERINFO_PANEL_RAD, bgcolor );

	textColor[0] = textColor[1] = textColor[2] = 1.0f;
	textColor[3] = cg_hudAlpha.value;

	// top line: points (Survival only)
	if ( cgs.gametype == GT_COOP_SURVIVAL ) {
		trap_R_SetColor( textColor );
		CG_DrawPic( x, pointsY, PLAYERINFO_ICON_W, PLAYERINFO_ICON_H, cgs.media.scoreIconShader );
		trap_R_SetColor( NULL );

		Com_sprintf( num, sizeof( num ), "%i", score );
		CG_Text_Paint( x + PLAYERINFO_ICON_W + 4, pointsY + 9, UI_FONT_NORMAL, 0.22f, textColor, num, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

		// perk icons; teammates' levels come from coopinfo's perkBits
		{
			float perkX = x + PLAYERINFO_PERK_START;
			int p;

			for ( p = PERK_RESILIENCE; p < NUM_PERKS; p++ ) {
				int level = CG_PlayerInfoPerkLevel( ci, p );

				if ( level > 0 ) {
					gitem_t *item = BG_FindItemForPerk( p );
					qhandle_t icon;

					if ( !item ) {
						continue;
					}

					CG_RegisterItemVisuals( item - bg_itemlist );
					icon = cg_items[item - bg_itemlist].icons[0];
					if ( level >= 2 && cgs.media.perkProIcons[p] ) {
						icon = cgs.media.perkProIcons[p];
					}

					trap_R_SetColor( textColor );
					CG_DrawPic( perkX, pointsY + PLAYERINFO_PERK_Y_OFS, PLAYERINFO_PERK_ICON, PLAYERINFO_PERK_ICON, icon );
					trap_R_SetColor( NULL );

					perkX += PLAYERINFO_PERK_ICON + PLAYERINFO_PERK_GAP;
				}
			}
		}
	}

	// middle line: health bar (+ gray armor overlay), value to its right
	frac = ( maxHealth > 0 ) ? ( (float)health / (float)maxHealth ) : 0.0f;
	if ( frac < 0.0f ) {
		frac = 0.0f;
	} else if ( frac > 1.0f ) {
		frac = 1.0f;
	}

	// 1px dark frame behind both bars, so the fill reads as inset rather than floating
	{
		vec4_t barFrame;
		barFrame[0] = barFrame[1] = barFrame[2] = 0.0f;
		barFrame[3] = 0.5f * cg_hudAlpha.value;
		CG_FillRect( x - 1, barY - 1, PLAYERINFO_BAR_W + 2, PLAYERINFO_BAR_H + 2, barFrame );
	}

	CG_PlayerInfoHealthColor( frac, barColor );
	CG_FilledBar( x, barY, PLAYERINFO_BAR_W, PLAYERINFO_BAR_H, barColor, NULL, NULL, frac, 0 );

	if ( armor > 0 ) {
		armorFrac = ( maxHealth > 0 ) ? ( (float)armor / (float)maxHealth ) : 0.0f;
		if ( armorFrac > 1.0f ) {
			armorFrac = 1.0f;
		}
		armorColor[0] = 0.45f;
		armorColor[1] = 0.45f;
		armorColor[2] = 0.5f;
		armorColor[3] = 0.8f * cg_hudAlpha.value;
		CG_FilledBar( x, barY, PLAYERINFO_BAR_W, PLAYERINFO_BAR_H, armorColor, NULL, NULL, armorFrac, 0 );
	}

	Com_sprintf( num, sizeof( num ), "%i", ( health > 0 ) ? health : 0 );
	CG_Text_Paint( x + PLAYERINFO_BAR_W + 5, barY + PLAYERINFO_BAR_H, UI_FONT_NORMAL, 0.22f, textColor, num, 0, 0, ITEM_TEXTSTYLE_SHADOWED );

	// bottom line: class icon, nickname, then talk icon
	nameY = barY + PLAYERINFO_BAR_H + 12;
	nameX = x;

	{
		int pc = CG_PlayerInfoClass( ci );

		if ( pc >= 0 && pc < 4 && cgs.media.classIcons[pc] ) {
			trap_R_SetColor( textColor );
			CG_DrawPic( x, nameY - 9, PLAYERINFO_CLASS_ICON, PLAYERINFO_CLASS_ICON, cgs.media.classIcons[pc] );
			trap_R_SetColor( NULL );
			nameX = x + PLAYERINFO_CLASS_ICON + PLAYERINFO_CLASS_GAP;
		}
	}

	nameColor[0] = ci->c1RGBA[0] / 255.0f;
	nameColor[1] = ci->c1RGBA[1] / 255.0f;
	nameColor[2] = ci->c1RGBA[2] / 255.0f;
	nameColor[3] = cg_hudAlpha.value;
	Q_strncpyz( nameClean, ci->name, sizeof( nameClean ) );
	Q_StripColorCodes( nameClean );
	CG_Text_Paint( nameX, nameY, UI_FONT_NORMAL, 0.18f, nameColor, nameClean, 0, 14, ITEM_TEXTSTYLE_SHADOWED );

	if ( cg_entities[ci->clientNum].voipTalking ) {
		nameW = CG_Text_Width( nameClean, UI_FONT_NORMAL, 0.18f, 14 );
		trap_R_SetColor( textColor );
		CG_DrawPic( nameX + nameW + 4, nameY - 6, 8, 8, cgs.media.speakingShader );
		trap_R_SetColor( NULL );
	}
}

/*
===============
CG_DrawTeammateInfoRow

Compact entry for another player's row: points, class icon, nickname, no
health bar or perk icons. A downed teammate (health <= 0) shows the "needs
heal" icon in place of the talk icon.
===============
*/
#define PLAYERINFO_ROW_W_COMPACT  100
#define PLAYERINFO_ROW_H_COMPACT  34
#define PLAYERINFO_WOUNDED_ICON   12

static void CG_DrawTeammateInfoRow( float x, float y, clientInfo_t *ci, int score, int health ) {
	char nameClean[MAX_QPATH];
	char num[16];
	vec4_t bgcolor, nameColor, textColor;
	float panelY, pointsY, nameY, nameX, nameW;

	if ( !ci->infoValid ) {
		return;
	}

	panelY = y;
	pointsY = y + 5;
	nameY = y + 27;

	// larger, darker rect behind gives a cheap 1px border effect
	bgcolor[0] = bgcolor[1] = bgcolor[2] = 0.0f;
	bgcolor[3] = 0.6f * cg_hudAlpha.value;
	CG_FillRectRounded( x - 7, panelY - 1, PLAYERINFO_ROW_W_COMPACT + 2, PLAYERINFO_ROW_H_COMPACT + 2, PLAYERINFO_PANEL_RAD + 1, bgcolor );

	bgcolor[3] = 0.35f * cg_hudAlpha.value;
	CG_FillRectRounded( x - 6, panelY, PLAYERINFO_ROW_W_COMPACT, PLAYERINFO_ROW_H_COMPACT, PLAYERINFO_PANEL_RAD, bgcolor );

	textColor[0] = textColor[1] = textColor[2] = 1.0f;
	textColor[3] = cg_hudAlpha.value;

	// top line: points only (Survival only) - no perk icons for teammates
	if ( cgs.gametype == GT_COOP_SURVIVAL ) {
		trap_R_SetColor( textColor );
		CG_DrawPic( x, pointsY, PLAYERINFO_ICON_W, PLAYERINFO_ICON_H, cgs.media.scoreIconShader );
		trap_R_SetColor( NULL );

		Com_sprintf( num, sizeof( num ), "%i", score );
		CG_Text_Paint( x + PLAYERINFO_ICON_W + 4, pointsY + 9, UI_FONT_NORMAL, 0.22f, textColor, num, 0, 0, ITEM_TEXTSTYLE_SHADOWED );
	}

	// bottom line: class icon, nickname, then wounded icon (or talk icon)
	nameX = x;

	{
		int pc = CG_PlayerInfoClass( ci );

		if ( pc >= 0 && pc < 4 && cgs.media.classIcons[pc] ) {
			trap_R_SetColor( textColor );
			CG_DrawPic( x, nameY - 9, PLAYERINFO_CLASS_ICON, PLAYERINFO_CLASS_ICON, cgs.media.classIcons[pc] );
			trap_R_SetColor( NULL );
			nameX = x + PLAYERINFO_CLASS_ICON + PLAYERINFO_CLASS_GAP;
		}
	}

	nameColor[0] = ci->c1RGBA[0] / 255.0f;
	nameColor[1] = ci->c1RGBA[1] / 255.0f;
	nameColor[2] = ci->c1RGBA[2] / 255.0f;
	nameColor[3] = cg_hudAlpha.value;
	Q_strncpyz( nameClean, ci->name, sizeof( nameClean ) );
	Q_StripColorCodes( nameClean );
	CG_Text_Paint( nameX, nameY, UI_FONT_NORMAL, 0.18f, nameColor, nameClean, 0, 10, ITEM_TEXTSTYLE_SHADOWED );

	nameW = CG_Text_Width( nameClean, UI_FONT_NORMAL, 0.18f, 10 );

	if ( health <= 0 && cgs.media.hintShaders[HINT_REVIVE] ) {
		trap_R_SetColor( textColor );
		CG_DrawPic( nameX + nameW + 4, nameY - 9, PLAYERINFO_WOUNDED_ICON, PLAYERINFO_WOUNDED_ICON, cgs.media.hintShaders[HINT_REVIVE] );
		trap_R_SetColor( NULL );
	} else if ( cg_entities[ci->clientNum].voipTalking ) {
		trap_R_SetColor( textColor );
		CG_DrawPic( nameX + nameW + 4, nameY - 6, 8, 8, cgs.media.speakingShader );
		trap_R_SetColor( NULL );
	}
}

/*
===============
CG_DrawPlayerInfo

Bottom-left stack of player-info entries; local player anchors at the
bottom (full row, with health bar and perks) and others stack upward above
it as compact rows (name, class icon, points, no health bar/perks).
===============
*/
#define PLAYERINFO_BASE_X  20
#define PLAYERINFO_BASE_Y  408

void CG_DrawPlayerInfo( void ) {
	clientInfo_t *ci;
	int i, row;

	if ( cgs.gametype > GT_COOP ) {
		return;
	}

	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_SPECTATOR ) {
		return;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement( PLACE_LEFT, PLACE_BOTTOM );
	}

	// score only updates when the server is asked (normally the scoreboard); keep it fresh ourselves
	if ( cgs.gametype == GT_COOP_SURVIVAL && cg.scoresRequestTime + 2000 < cg.time ) {
		cg.scoresRequestTime = cg.time;
		trap_SendClientCommand( "score" );
	}

	row = 0;
	for ( i = 0; i < numSortedTeamPlayers; i++ ) {
		if ( sortedTeamPlayers[i] == cg.snap->ps.clientNum ) {
			continue;
		}
		ci = cgs.clientinfo + sortedTeamPlayers[i];
		if ( !ci->infoValid || ci->team != cg.snap->ps.persistant[PERS_TEAM] ) {
			continue;
		}
		row++;
		// +20 lines this up with the self row's panel top, which sits 20px below its own anchor y
		CG_DrawTeammateInfoRow( PLAYERINFO_BASE_X, PLAYERINFO_BASE_Y + 20 - row * ( PLAYERINFO_ROW_H_COMPACT + PLAYERINFO_ROW_GAP ),
								 ci, ci->score, ci->health );
	}

	// local player's own score is always fresh via playerState_t
	ci = &cgs.clientinfo[cg.snap->ps.clientNum];
	CG_DrawPlayerInfoRow( PLAYERINFO_BASE_X, PLAYERINFO_BASE_Y, ci,
						   cg.snap->ps.persistant[PERS_SCORE],
						   cg.snap->ps.stats[STAT_HEALTH], cg.snap->ps.stats[STAT_MAX_HEALTH],
						   cg.snap->ps.stats[STAT_ARMOR] );

	if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement( PLACE_CENTER, PLACE_CENTER );
	}
}

static void CG_DrawPlayerWaves( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	char num[16];
	int value = cg.snap->ps.persistant[PERS_WAVES];

	if ( shader ) {
		trap_R_SetColor( color );
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
		trap_R_SetColor( NULL );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", value );
		value = CG_Text_Width( num, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
	}
}


static void CG_DrawHoldableItem( rectDef_t *rect, int font, float scale, qboolean draw2D ) {
	int value;
	gitem_t *item;

	item    = BG_FindItemForHoldable( cg.holdableSelect );

	if ( !item ) {
		return;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_RIGHT, PLACE_CENTER);
	}

	value   = cg.predictedPlayerState.holdable[cg.holdableSelect];

	if ( value ) {
		CG_RegisterItemVisuals( item - bg_itemlist );

		if ( cg.holdableSelect == HI_WINE ) {
			if ( value > 3 ) {
				value = 3;  // 3 stages to icon, just draw full if beyond 'full'
			}
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cg_items[item - bg_itemlist].icons[2 - ( value - 1 )] );
		} else {
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cg_items[item - bg_itemlist].icons[0] );
		}
	}
}

static void CG_DrawPlayerItem( rectDef_t *rect, int font, float scale, qboolean draw2D ) {
	int value;
	vec3_t origin, angles;

	value = cg.snap->ps.stats[STAT_HOLDABLE_ITEM];

	if ( value ) {
		CG_RegisterItemVisuals( value );

		if ( qtrue ) {
			CG_RegisterItemVisuals( value );
			CG_DrawPic( rect->x, rect->y, rect->w, rect->h, cg_items[ value ].icons[0] );
		} else {
			VectorClear( angles );
			origin[0] = 90;
			origin[1] = 0;
			origin[2] = -10;
			angles[YAW] = ( cg.time & 2047 ) * 360 / 2048.0;
			CG_Draw3DModel( rect->x, rect->y, rect->w, rect->h, cg_items[ value ].models[0], 0, origin, angles );
		}
	}
}

static void CG_DrawPlayerSpawnpoints( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	playerState_t   *ps;
	int value;
	char num[16];

	ps = &cg.snap->ps;

	value = ps->persistant[PERS_SPAWNPOINTS_LEFT] + 1;

	if ( shader ) {
		trap_R_SetColor( color );
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
		trap_R_SetColor( NULL );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", value );
		value = CG_Text_Width( num, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
	}

}

static void CG_DrawPlayerLives( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	playerState_t   *ps;
	int value;
	char num[16];

	ps = &cg.snap->ps;

	value = ps->persistant[PERS_RESPAWNS_LEFT] + 1;

	if ( shader ) {
		trap_R_SetColor( color );
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
		trap_R_SetColor( NULL );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", value );
		value = CG_Text_Width( num, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
	}
}


static void CG_DrawPlayerHealth( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	playerState_t   *ps;
	int value;
	char num[16];

	ps = &cg.snap->ps;

	value = ps->stats[STAT_HEALTH];

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_LEFT, PLACE_BOTTOM);
	}

	if ( shader ) {
		trap_R_SetColor( color );
		CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );
		trap_R_SetColor( NULL );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", value );
		value = CG_Text_Width( num, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
	}
}

static void CG_DrawRedScore( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	int value;
	char num[16];
	if ( cgs.scores1 == SCORE_NOT_PRESENT ) {
		Com_sprintf( num, sizeof( num ), "-" );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", cgs.scores1 );
	}
	value = CG_Text_Width( num, font, scale, 0 );
	CG_Text_Paint( rect->x + rect->w - value, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
}

static void CG_DrawBlueScore( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	int value;
	char num[16];

	if ( cgs.scores2 == SCORE_NOT_PRESENT ) {
		Com_sprintf( num, sizeof( num ), "-" );
	} else {
		Com_sprintf( num, sizeof( num ), "%i", cgs.scores2 );
	}
	value = CG_Text_Width( num, font, scale, 0 );
	CG_Text_Paint( rect->x + rect->w - value, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
}

static void CG_DrawTeamColor( rectDef_t *rect, vec4_t color ) {
	CG_DrawTeamBackground( rect->x, rect->y, rect->w, rect->h, color[3], cg.snap->ps.persistant[PERS_TEAM] );
}

float CG_GetValue( int ownerDraw, int type ) {
	centity_t   *cent;
	playerState_t   *ps;

	cent = &cg_entities[cg.snap->ps.clientNum];
	ps = &cg.snap->ps;

	switch ( ownerDraw ) {

	case CG_PLAYER_ARMOR_VALUE:
		return ps->stats[STAT_ARMOR];
		break;
	case CG_PLAYER_AMMO_VALUE:
		if ( cent->currentState.weapon ) {
			if ( type == RANGETYPE_RELATIVE ) {
				int weap = BG_FindAmmoForWeapon( cent->currentState.weapon );
				return (float)ps->ammo[weap] / (float)BG_GetMaxAmmo( ps, weap, FIELDOPS_AMMO_BONUS_MULTIPLIER );
			} else {
				return ps->ammo[BG_FindAmmoForWeapon( cent->currentState.weapon )];
			}
		}
		break;
	case CG_PLAYER_AMMOCLIP_VALUE:
		if ( cent->currentState.weapon ) {
			if ( type == RANGETYPE_RELATIVE ) {
				return (float)ps->ammoclip[BG_FindClipForWeapon( cent->currentState.weapon )] / (float)BG_GetMaxClip( ps, cent->currentState.weapon );
			} else {
				return ps->ammoclip[BG_FindClipForWeapon( cent->currentState.weapon )];
			}
		}
		break;
	case CG_PLAYER_SCORE:
		return cg.snap->ps.persistant[PERS_SCORE];
		break;
	case CG_PLAYER_LIVES:
		return ps->persistant[PERS_RESPAWNS_LEFT];
		break;
	case CG_PLAYER_SPAWNPOINTS:
		return ps->persistant[PERS_SPAWNPOINTS_LEFT];
		break;
	case CG_PLAYER_HEALTH:
		return ps->stats[STAT_HEALTH];
		break;
	case CG_RED_SCORE:
		return cgs.scores1;
		break;
	case CG_BLUE_SCORE:
		return cgs.scores2;
		break;
	case CG_PLAYER_WEAPON_STABILITY:    //----(SA)	added
		return ps->aimSpreadScale;
		break;
	default:
		break;
	}

	return -1;
}

// THINKABOUTME: should these be exclusive or inclusive..
//
qboolean CG_OwnerDrawVisible( int flags ) {

//----(SA)	added
	if ( flags & CG_SHOW_NOT_V_BINOC ) {      // if looking through binocs
		if ( cg.zoomedBinoc ) {
			return qfalse;
		}
	}

	if ( flags & CG_SHOW_NOT_V_SNIPER ) {     // if looking through sniper scope
		if ( cg.weaponSelect == WP_SNIPERRIFLE ) {
			return qfalse;
		}
	}

	if ( flags & CG_SHOW_NOT_V_FGSCOPE ) {        // if looking through fg42 scope
		if ( cg.weaponSelect == WP_FG42SCOPE ) {
			return qfalse;
		}
	}

//----(SA)	end
	if ( flags & CG_SHOW_HEALTHCRITICAL ) {
		if ( cg.snap->ps.stats[STAT_HEALTH] < 25 ) {
			return qtrue;
		}
	}

	if ( flags & CG_SHOW_HEALTHOK ) {
		if ( cg.snap->ps.stats[STAT_HEALTH] > 25 ) {
			return qtrue;
		}
	}

	if ( flags & CG_SHOW_SINGLEPLAYER ) {
		if ( cgs.gametype == GT_SINGLE_PLAYER ) {
			return qtrue;
		}
	}

	if ( flags & CG_SHOW_DURINGINCOMINGVOICE ) {
	}

//----(SA)	added
	if ( flags & CG_SHOW_NOT_V_CLEAR ) {
		// if /not/ looking through binocs or sniper
		if ( !cg.zoomedBinoc && !( cg.weaponSelect == WP_SNIPERRIFLE ) && !( cg.weaponSelect == WP_FG42SCOPE ) ) {
			return qfalse;
		}
	}

	if ( flags & ( CG_SHOW_NOT_V_BINOC | CG_SHOW_NOT_V_SNIPER | CG_SHOW_NOT_V_SNOOPER | CG_SHOW_NOT_V_FGSCOPE ) ) {
		// setting any of these does not necessarily disable drawing in regular view
		// CG_SHOW_NOT_V_CLEAR must also be set to hide for reg view
		if ( !( flags & CG_SHOW_NOT_V_CLEAR ) ) {
			return qtrue;
		}
	}

//----(SA)	end


	return qfalse;
}




static void CG_DrawAreaSystemChat( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader ) {
	CG_Text_Paint( rect->x, rect->y + rect->h, font, scale, color, systemChat, 0, 0, 0 );
}

static void CG_DrawAreaTeamChat( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader ) {
	CG_Text_Paint( rect->x, rect->y + rect->h, font, scale, color,teamChat1, 0, 0, 0 );
}

static void CG_DrawAreaChat( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader ) {
	CG_Text_Paint( rect->x, rect->y + rect->h, font, scale, color, teamChat2, 0, 0, 0 );
}

static const char *CG_GetKillerText( void ) {
	const unsigned char *s = (unsigned char *)"";
	if ( cg.killerName[0] ) {
		s = (unsigned char *)va( "Fragged by %s", cg.killerName );
	}
	return (const char*)s;
}


static void CG_DrawKiller( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	// fragged by ... line
	if ( cg.killerName[0] ) {
		int x = rect->x + rect->w / 2;
		CG_Text_Paint( x - CG_Text_Width( CG_GetKillerText(),font, scale, 0 ) / 2, rect->y + rect->h, font, scale, color, CG_GetKillerText(), 0, 0, textStyle );
	}

}


static void CG_DrawCapFragLimit( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	int limit = cgs.fraglimit;
	CG_Text_Paint( rect->x, rect->y, font, scale, color, va( "%2i", limit ),0, 0, textStyle );
}

static void CG_Draw1stPlace( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	if ( cgs.scores1 != SCORE_NOT_PRESENT ) {
		CG_Text_Paint( rect->x, rect->y, font, scale, color, va( "%2i", cgs.scores1 ),0, 0, textStyle );
	}
}

static void CG_Draw2ndPlace( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	if ( cgs.scores2 != SCORE_NOT_PRESENT ) {
		CG_Text_Paint( rect->x, rect->y, font, scale, color, va( "%2i", cgs.scores2 ),0, 0, textStyle );
	}
}

static const char *CG_GetGameStatusText( void ) {
	const unsigned char *s = (unsigned char*)"";
	if ( cg.snap->ps.persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		s = (unsigned char *)va( "%s place with %i",CG_PlaceString( cg.snap->ps.persistant[PERS_RANK] + 1 ),cg.snap->ps.persistant[PERS_SCORE] );
	}
	return (char *)s;
}

static void CG_DrawGameStatus( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	CG_Text_Paint( rect->x, rect->y + rect->h, font, scale, color, CG_GetGameStatusText(), 0, 0, textStyle );
}

static const char *CG_GameTypeString( void ) {
	if ( cgs.gametype == GT_SINGLE_PLAYER ) {
		return "Single Player";
	} else if ( cgs.gametype == GT_COOP ) {
		return "Cooperative";
	} else if ( cgs.gametype == GT_COOP_SURVIVAL ) {
		return "Survival";
	} else {
		return "";
	}
}

int CG_OwnerDrawWidth( int ownerDraw, int font, float scale ) {
	switch ( ownerDraw ) {
	case CG_GAME_TYPE:
		return CG_Text_Width( CG_GameTypeString(), font, scale, 0 );
	case CG_GAME_STATUS:
		return CG_Text_Width( CG_GetGameStatusText(), font, scale, 0 );
		break;
	case CG_KILLER:
		return CG_Text_Width( CG_GetKillerText(), font, scale, 0 );
		break;
	}
	return 0;
}

static void CG_DrawGameType( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	CG_Text_Paint( rect->x, rect->y + rect->h, font, scale, color, CG_GameTypeString(), 0, 0, textStyle );
}

static void CG_Text_Paint_Limit( float *maxX, float x, float y, int font, float scale, vec4_t color, const char* text, float adjust, int limit ) {
	int len, count;
	vec4_t newColor;
	glyphInfo_t *glyph;
	if ( text ) {
		const char *s = text;
		float max = *maxX;
		float useScale;

		fontInfoExtra_t *fnt = &cgDC.Assets.textFont;
		if ( font == UI_FONT_DEFAULT ) {
			if ( scale <= cg_smallFont.value ) {
				fnt = &cgDC.Assets.smallFont;
			} else if ( scale > cg_bigFont.value ) {
				fnt = &cgDC.Assets.bigFont;
			}
		} else {
			fnt = UI_SelectFont( &cgDC.Assets, font, fnt );
		}

		useScale = scale * fnt->glyphScale;
		trap_R_SetColor( color );
		len = Q_UTF8_Strlen( text );
		if ( limit > 0 && len > limit ) {
			len = limit;
		}
		count = 0;
		while ( s && *s && count < len ) {
			glyph = Q_UTF8_GetGlyphExtended( fnt, Q_UTF8_CodePoint( s ) );
			if ( Q_IsColorString( s ) ) {
				memcpy( newColor, g_color_table[ColorIndex( *( s + 1 ) )], sizeof( newColor ) );
				newColor[3] = color[3];
				trap_R_SetColor( newColor );
				s += 2;
				continue;
			} else {
				float yadj = useScale * glyph->top;
				if ( CG_Text_Width( (char *)s, font, useScale, 1 ) + x > max ) {
					*maxX = 0;
					break;
				}
				CG_Text_PaintChar( x, y - yadj,
								   glyph->imageWidth,
								   glyph->imageHeight,
								   useScale,
								   glyph->s,
								   glyph->t,
								   glyph->s2,
								   glyph->t2,
								   glyph->glyph );
				x += ( glyph->xSkip * useScale ) + adjust;
				*maxX = x;
				count++;
				s += Q_UTF8_Width( s );
			}
		}
		trap_R_SetColor( NULL );
	}

}



#define PIC_WIDTH 12

static void CG_DrawTeamSpectators( rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader ) {
	if ( cg.spectatorLen ) {
		float maxX;

		if ( cg.spectatorWidth == -1 ) {
			cg.spectatorWidth = 0;
			cg.spectatorPaintX = rect->x + 1;
			cg.spectatorPaintX2 = -1;
		}

		if ( cg.spectatorOffset > cg.spectatorLen ) {
			cg.spectatorOffset = 0;
			cg.spectatorPaintX = rect->x + 1;
			cg.spectatorPaintX2 = -1;
		}

		if ( cg.time > cg.spectatorTime ) {
			cg.spectatorTime = cg.time + 10;
			if ( cg.spectatorPaintX <= rect->x + 2 ) {
				if ( cg.spectatorOffset < cg.spectatorLen ) {
					cg.spectatorPaintX += CG_Text_Width( &cg.spectatorList[cg.spectatorOffset], font, scale, 1 ) - 1;
					cg.spectatorOffset++;
				} else {
					cg.spectatorOffset = 0;
					if ( cg.spectatorPaintX2 >= 0 ) {
						cg.spectatorPaintX = cg.spectatorPaintX2;
					} else {
						cg.spectatorPaintX = rect->x + rect->w - 2;
					}
					cg.spectatorPaintX2 = -1;
				}
			} else {
				cg.spectatorPaintX--;
				if ( cg.spectatorPaintX2 >= 0 ) {
					cg.spectatorPaintX2--;
				}
			}
		}

		maxX = rect->x + rect->w - 2;
		CG_Text_Paint_Limit( &maxX, cg.spectatorPaintX, rect->y + rect->h - 3, UI_FONT_DEFAULT, scale, color, &cg.spectatorList[cg.spectatorOffset], 0, 0 );
		if ( cg.spectatorPaintX2 >= 0 ) {
			float maxX2 = rect->x + rect->w - 2;
			CG_Text_Paint_Limit( &maxX2, cg.spectatorPaintX2, rect->y + rect->h - 3, UI_FONT_DEFAULT, scale, color, cg.spectatorList, 0, cg.spectatorOffset );
		}
		if ( cg.spectatorOffset && maxX > 0 ) {
			// if we have an offset ( we are skipping the first part of the string ) and we fit the string
			if ( cg.spectatorPaintX2 == -1 ) {
				cg.spectatorPaintX2 = rect->x + rect->w - 2;
			}
		} else {
			cg.spectatorPaintX2 = -1;
		}

	}
}



static void CG_DrawMedal( int ownerDraw, rectDef_t *rect, int font, float scale, vec4_t color, qhandle_t shader ) {
	score_t *score = &cg.scores[cg.selectedScore];
	float value = 0;
	char *text = NULL;
	color[3] = 0.25;

	switch ( ownerDraw ) {
	case CG_ACCURACY:
		value = score->accuracy;
		break;
	case CG_ASSISTS:
		value = score->assistCount;
		break;
	case CG_DEFEND:
		value = score->defendCount;
		break;
	case CG_EXCELLENT:
		value = score->excellentCount;
		break;
	case CG_IMPRESSIVE:
		value = score->impressiveCount;
		break;
	case CG_PERFECT:
		value = score->perfect;
		break;
	case CG_GAUNTLET:
		value = score->guantletCount;
		break;
	case CG_CAPTURES:
		value = score->captures;
		break;
	}

	if ( value > 0 ) {
		if ( ownerDraw != CG_PERFECT ) {
			if ( ownerDraw == CG_ACCURACY ) {
				text = va( "%i%%", (int)value );
				if ( value > 50 ) {
					color[3] = 1.0;
				}
			} else {
				text = va( "%i", (int)value );
				color[3] = 1.0;
			}
		} else {
			if ( value ) {
				color[3] = 1.0;
			}
			text = "Wow";
		}
	}

	trap_R_SetColor( color );
	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, shader );

	if ( text ) {
		color[3] = 1.0;
		value = CG_Text_Width( text, font, scale, 0 );
		CG_Text_Paint( rect->x + ( rect->w - value ) / 2, rect->y + rect->h + 10, font, scale, color, text, 0, 0, 0 );
	}
	trap_R_SetColor( NULL );

}


/*
==============
CG_DrawWeapStability
    draw a bar showing current stability level (0-255), max at current weapon/ability, and 'perfect' reference mark

    probably only drawn for scoped weapons
==============
*/
static void CG_DrawWeapStability( rectDef_t *rect, vec4_t color, int align ) {
	vec4_t goodColor = {0, 1, 0, 0.5f}, badColor = {1, 0, 0, 0.5f};

	if ( !cg_drawSpreadScale.integer ) {
		return;
	}

	if ( cg_drawSpreadScale.integer == 1 && !(GetWeaponTableData( cg.weaponSelect )->weaponClass & WEAPON_CLASS_SCOPED) ) {
		// cg_drawSpreadScale of '1' means only draw for scoped weapons, '2' means draw all the time (for debugging)
		return;
	}

	if ( !( cg.snap->ps.aimSpreadScale ) ) {
		return;
	}

	CG_FilledBar( rect->x, rect->y, rect->w, rect->h, goodColor, badColor, NULL, (float)cg.snap->ps.aimSpreadScale / 255.0f, 2 | 4 | 256 ); // flags (BAR_CENTER|BAR_VERT|BAR_LERP_COLOR)
}


/*
==============
CG_DrawWeapHeat
==============
*/
static void CG_DrawWeapHeat( rectDef_t *rect, int align ) {
	vec4_t color = {1, 0, 0, 0.2f}, color2 = {1, 0, 0, 0.5f};
	int flags = 0;

	if ( !( cg.snap->ps.curWeapHeat ) ) {
		return;
	}

	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement(PLACE_RIGHT, PLACE_BOTTOM);
	}

	if ( align != HUD_HORIZONTAL ) {
		flags |= 4;   // BAR_VERT

	}
	flags |= 1;       // BAR_LEFT			- this is hardcoded now, but will be decided by the menu script
	flags |= 16;      // BAR_BG			- draw the filled contrast box
//	flags|=32;		// BAR_BGSPACING_X0Y5	- different style

	flags |= 256;     // BAR_COLOR_LERP
	CG_FilledBar( rect->x, rect->y, rect->w, rect->h, color, color2, NULL, (float)cg.snap->ps.curWeapHeat / 255.0f, flags );
}


static void CG_DrawWeapRecharge( rectDef_t *rect, vec4_t color, int align ) {
	float barFrac;
	float chargeTime;
	int flags = 0;
	//qboolean fade = qfalse;
	vec4_t bgcolor = {1.0f, 1.0f, 1.0f, 0.25f};

	// Soldier has no active class ability, so there's nothing to charge
	if ( cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_SOLDIER ) {
		return;
	}

	if ( align != HUD_HORIZONTAL) {
		flags |= 4;   // BAR_VERT
		flags |= 1;   // BAR_LEFT (left, when vertical means grow 'up')
	}
	flags |= 16;


		// Determine charge time based on class
		switch (cg.snap->ps.stats[STAT_PLAYER_CLASS])
		{
		case PC_MEDIC:
			chargeTime = cg_medicChargeTime.value;
			break;
		case PC_ENGINEER:
			chargeTime = cg_engineerChargeTime.value;
			break;
		case PC_FIELDOPS:
			chargeTime = cg_airstrikeChargeTime.value;
			break;
		default:
		    chargeTime = 30000;
			break;
		}

		barFrac = (float)( cg.time - cg.snap->ps.classWeaponTime ) / chargeTime;

		if ( barFrac > 1.0 ) {
			barFrac = 1.0;
		}

		color[0] = 1.0f;
		color[1] = color[2] = barFrac;
		color[3] = 0.25 + barFrac * 0.5;

		/*if ( fade ) {
			bgcolor[3] *= 0.4f;
			color[3] *= 0.4;
		}*/

		CG_FilledBar( rect->x, rect->y + 6, rect->w, rect->h * 0.84f, color, NULL, bgcolor, barFrac, flags );

		color[1] = color[2] = 1.0f;
		color[3] = cg_hudAlpha.value;
		trap_R_SetColor( color );

}

/*
==============
CG_EquipRemainingMs

	Milliseconds until the given equipment weapon is ready again (0 if ready).
	Landmine has no charge cost, so it falls to the default case.
==============
*/
static int CG_EquipRemainingMs( int equipWeapon ) {
	float chargeTime;
	int lastUseTime;
	float remaining;

	switch ( equipWeapon ) {
	case WP_DYNAMITE:
		chargeTime = cg_engineerChargeTime.value;
		lastUseTime = cg.snap->ps.dynamiteWeaponTime;
		break;
	case WP_MEDCRATE:
		chargeTime = cg_medicChargeTime.value;
		lastUseTime = cg.snap->ps.medcrateWeaponTime;
		break;
	case WP_AIRSTRIKE:
		chargeTime = cg_airstrikeChargeTime.value;
		lastUseTime = cg.snap->ps.airstrikeWeaponTime;
		break;
	default:
		return 0;
	}

	remaining = chargeTime - (float)( cg.time - lastUseTime );

	if ( remaining <= 0 ) {
		return 0;
	}

	return (int)remaining;
}

/*
==============
CG_ArtyRemainingMs

	Milliseconds until the Field Ops' binocular arty call-in is ready again (0 if
	ready). Own clock/cvar, independent of the airstrike signal.
==============
*/
static int CG_ArtyRemainingMs( void ) {
	float remaining = cg_artyChargeTime.value - (float)( cg.time - cg.snap->ps.artyWeaponTime );

	if ( remaining <= 0 ) {
		return 0;
	}

	return (int)remaining;
}

/*
==============
CG_DrawEquipIcon

	Draws the equipment icon for the given slot (1, 2 or 3), dimmed on cooldown
	and highlighted (gold tint, or weaponIcon[1] for dynamite/M7) while selected.
==============
*/
static void CG_DrawEquipIcon( rectDef_t *rect, int slot ) {
	int equipWeapon = CG_GetEquipSlotWeapon( slot );
	qhandle_t icon;
	vec4_t color = {1.0f, 1.0f, 1.0f, 1.0f};
	qboolean ready = qtrue;

	if ( equipWeapon == WP_NONE ) {
		// Field Ops has no landmine for slot 2 - show binoculars there instead, if owned
		if ( slot == 2 && cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS &&
			 ( cg.snap->ps.stats[STAT_KEYS] & ( 1 << INV_BINOCS ) ) ) {
			icon = cgs.media.binocularsIcon;
			ready = ( CG_ArtyRemainingMs() == 0 );

			// binoculars aren't a selectable weapon - "selected" means actively zoomed in with them
			if ( cg.zoomedBinoc ) {
				color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f;
			}
		} else {
			return;
		}
	} else {
		CG_RegisterWeapon( equipWeapon );
		ready = ( CG_EquipRemainingMs( equipWeapon ) == 0 );

		switch ( equipWeapon ) {
		case WP_AIRSTRIKE:
		case WP_MEDCRATE:
		case WP_LANDMINE:
			// gold tint when selected, like binoculars above
			icon = cg_weapons[equipWeapon].weaponIcon[0];
			if ( cg.weaponSelect == equipWeapon ) {
				color[0] = 1.0f; color[1] = 0.8f; color[2] = 0.2f;
			}
			break;
		default:
			icon = cg_weapons[equipWeapon].weaponIcon[ cg.weaponSelect == equipWeapon ? 1 : 0 ];
			break;
		}
	}

	if ( !icon ) {
		return;
	}

	color[3] = ready ? cg_hudAlpha.value : cg_hudAlpha.value * 0.4f;
	trap_R_SetColor( color );
	CG_DrawPic( rect->x, rect->y, rect->w, rect->h, icon );
	trap_R_SetColor( NULL );
}

/*
==============
CG_DrawEquipValue

	Draws the equipment slot's number: ammo count for landmine, a countdown
	to ready for cooldown-only items, nothing once ready or for an empty slot.
==============
*/
static void CG_DrawEquipValue( rectDef_t *rect, int slot, int font, float scale, vec4_t color, int textStyle ) {
	int equipWeapon = CG_GetEquipSlotWeapon( slot );
	char num[16];
	int value;
	int w;

	if ( equipWeapon == WP_LANDMINE ) {
		// landmine is clip-only - real stock is in ammoclip, not the ammo reserve pool
		value = cg.snap->ps.ammoclip[BG_FindClipForWeapon( WP_LANDMINE )];
	} else if ( equipWeapon == WP_M7 ) {
		// total rifle grenades carried - the launcher's clip only ever holds the one that's chambered
		value = cg.snap->ps.ammo[BG_FindAmmoForWeapon( WP_M7 )];
	} else {
		int remainingMs;

		if ( equipWeapon != WP_NONE ) {
			remainingMs = CG_EquipRemainingMs( equipWeapon );
		} else if ( slot == 2 && cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS &&
					( cg.snap->ps.stats[STAT_KEYS] & ( 1 << INV_BINOCS ) ) ) {
			remainingMs = CG_ArtyRemainingMs();
		} else {
			return; // empty slot
		}

		if ( remainingMs == 0 ) {
			return; // ready - no number needed
		}
		value = ( remainingMs + 999 ) / 1000; // round up to whole seconds
	}

	Com_sprintf( num, sizeof( num ), "%i", value );
	w = CG_Text_Width( num, font, scale, 0 );
	CG_Text_Paint( rect->x + ( rect->w - w ) / 2, rect->y + rect->h, font, scale, color, num, 0, 0, textStyle );
}

/*
==============
CG_DrawEquipKeyHint

	Draws "[key]" over the equipment icon for whatever key is currently bound
	to select it. Glyphs drawn separately to compensate for "["'s uneven
	built-in bearing vs "]".
==============
*/
static void CG_DrawEquipKeyHint( rectDef_t *rect, int slot, int font, float scale, vec4_t color, int textStyle ) {
	int equipWeapon = CG_GetEquipSlotWeapon( slot );
	cgBindAction_t action;
	char keyName[32];
	float openW, keyW, closeW, totalW, kern;
	float x;

	if ( equipWeapon == WP_NONE ) {
		if ( slot == 2 && cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS &&
			 ( cg.snap->ps.stats[STAT_KEYS] & ( 1 << INV_BINOCS ) ) ) {
			action = CG_BIND_ZOOM;
		} else {
			return;
		}
	} else if ( slot == 1 ) {
		action = CG_BIND_SPECIALEQUIP1;
	} else if ( slot == 3 ) {
		action = CG_BIND_SPECIALEQUIP3;
	} else {
		action = CG_BIND_SPECIALEQUIP2;
	}

	CG_KeyNameForAction( action, keyName, sizeof( keyName ) );

	openW = CG_Text_Width( "[", font, scale, 0 );
	keyW = CG_Text_Width( keyName, font, scale, 0 );
	closeW = CG_Text_Width( "]", font, scale, 0 );
	kern = openW * 0.4f;
	totalW = openW + keyW + closeW - kern;

	x = rect->x + ( rect->w - totalW ) / 2;
	CG_Text_Paint( x, rect->y + rect->h, font, scale, color, "[", 0, 0, textStyle );
	x += openW - kern;
	CG_Text_Paint( x, rect->y + rect->h, font, scale, color, keyName, 0, 0, textStyle );
	x += keyW;
	CG_Text_Paint( x, rect->y + rect->h, font, scale, color, "]", 0, 0, textStyle );
}

/*
==============
CG_EquipSlotHasContent

Whether slot (1, 2 or 3) has anything to show - an owned equipment weapon,
or (slot 2 only) a Field Ops' binoculars.
==============
*/
static qboolean CG_EquipSlotHasContent( int slot ) {
	if ( CG_GetEquipSlotWeapon( slot ) != WP_NONE ) {
		return qtrue;
	}

	return (qboolean)( slot == 2 && cg.snap->ps.stats[STAT_PLAYER_CLASS] == PC_FIELDOPS &&
					   ( cg.snap->ps.stats[STAT_KEYS] & ( 1 << INV_BINOCS ) ) );
}

/*
==============
CG_EquipStackAnchorX

Mirrors CG_DrawGrenadeReadoutRow's centering math for the "Grenade" row, so
the equipment stack lines up with where its number actually starts.
==============
*/
static float CG_EquipStackAnchorX( void ) {
	float numW, nameW, gap, totalW;
	const float boxX = 490.0f, boxW = 140.0f, rowH = 24.0f, scale = 0.32f;

	numW = CG_Text_Width( "5", UI_FONT_NORMAL, scale, 0 );
	nameW = CG_Text_Width( "Grenade", UI_FONT_NORMAL, scale * 0.55f, 0 );
	gap = rowH * 0.3f;
	totalW = numW + gap + nameW;

	return boxX + ( boxW - totalW ) * 0.5f;
}

/*
==============
CG_DrawEquipmentStack

Bottom-anchored equipment stack: only slots with content are drawn,
compacted so the lowest one always sits just above the grenade counter.
==============
*/
#define EQUIP_STACK_BASE_Y  306   // bottom-most row's y - ~10px clear of the grenade counter at y=360
#define EQUIP_STACK_PITCH   54

void CG_DrawEquipmentStack( void ) {
	vec4_t textColor;
	float equipX;
	int slot;
	int present[3];
	int count = 0;
	int i;

	if ( cgs.gametype != GT_COOP_SURVIVAL ) {
		return;
	}

	equipX = CG_EquipStackAnchorX();

	// grenade-counter rows leave this set without resetting it, so reset it ourselves first
	if ( cg_fixedAspect.integer == 2 ) {
		CG_SetScreenPlacement( PLACE_RIGHT, PLACE_BOTTOM );
	} else if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement( PLACE_CENTER, PLACE_CENTER );
	}

	for ( slot = 1; slot <= 3; slot++ ) {
		if ( CG_EquipSlotHasContent( slot ) ) {
			present[count++] = slot;
		}
	}

	textColor[0] = textColor[1] = textColor[2] = 1.0f;
	textColor[3] = cg_hudAlpha.value;

	for ( i = 0; i < count; i++ ) {
		rectDef_t rect;
		int slotNum = present[i];
		float y = EQUIP_STACK_BASE_Y - i * EQUIP_STACK_PITCH;

		rect.x = equipX + 12;
		rect.y = y;
		rect.w = 32;
		rect.h = 32;
		CG_DrawEquipIcon( &rect, slotNum );

		rect.x = equipX + 12;
		rect.y = y + 32;
		rect.w = 32;
		rect.h = 14;
		CG_DrawEquipValue( &rect, slotNum, UI_FONT_NORMAL, 0.22f, textColor, ITEM_TEXTSTYLE_SHADOWED );

		rect.x = equipX;
		rect.y = y - 6;
		rect.w = 22;
		rect.h = 11;
		CG_DrawEquipKeyHint( &rect, slotNum, UI_FONT_NORMAL, 0.17f, textColor, ITEM_TEXTSTYLE_SHADOWED );
	}

	if ( cg_fixedAspect.integer ) {
		CG_SetScreenPlacement( PLACE_CENTER, PLACE_CENTER );
	}
}

/*
==============
CG_OwnerDraw
==============
*/
void CG_OwnerDraw( float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special, int font, float scale, vec4_t color, qhandle_t shader, int textStyle ) {
	rectDef_t rect;

	if ( cg_drawStatus.integer == 0 ) {
		return;
	}

	rect.x = x;
	rect.y = y;
	rect.w = w;
	rect.h = h;

	switch ( ownerDraw ) {
	case CG_PLAYER_WEAPON_ICON2D:
		CG_DrawPlayerWeaponIcon( &rect, ownerDrawFlags & CG_SHOW_HIGHLIGHTED, align );
		break;
	case CG_PLAYER_WEAPON_NAME:
		CG_DrawPlayerWeaponName( &rect, font, scale, color, textStyle );
		break;
	case CG_PLAYER_AMMO_COMBINED:
		CG_DrawPlayerAmmoCombined( &rect, font, scale, color, textStyle );
		break;
	case CG_PLAYER_ARMOR_ICON:
		break;
	case CG_PLAYER_ARMOR_ICON2D:
		break;
	case CG_PLAYER_ARMOR_VALUE:
		CG_DrawPlayerArmorValue( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_PLAYER_ARMOR_VALUE_BAR:
		CG_DrawPlayerArmorValueBar( &rect, color, align );
		break;
	case CG_PLAYER_AMMO_ICON:
		CG_DrawPlayerAmmoIcon( &rect, ownerDrawFlags & CG_SHOW_2DONLY );
		break;
	case CG_PLAYER_AMMO_ICON2D:
		CG_DrawPlayerAmmoIcon( &rect, qtrue );
		break;
	case CG_PLAYER_AMMO_VALUE:
		CG_DrawPlayerAmmoValue( &rect, font, scale, color, shader, textStyle, 0 );
		break;
	case CG_CURSORHINT:
		CG_DrawCursorhint( &rect );
		break;
	//----(SA)	added
	case CG_NEWMESSAGE:
		CG_DrawMessageIcon( &rect );
		break;
	//----(SA)	end
	case CG_PLAYER_AMMOCLIP_VALUE:
		CG_DrawPlayerAmmoValue( &rect, font, scale, color, shader, textStyle, 1 );
		break;
	case CG_PLAYER_GRENADE_COUNT:
		CG_DrawGrenadeCount( &rect, font, scale, color, textStyle );
		break;
	case CG_PLAYER_SPECIAL_GRENADE_COUNT:
		CG_DrawSpecialGrenadeCount( &rect, font, scale, color, textStyle );
		break;
	case CG_PLAYER_WEAPON_HEAT:
		CG_DrawWeapHeat( &rect, align );
		break;
	case CG_PLAYER_WEAPON_STABILITY:
		CG_DrawWeapStability( &rect, color, align );
		break;
	case CG_PLAYER_HEAD:
		CG_DrawPlayerHead( &rect, ownerDrawFlags & CG_SHOW_2DONLY );
		break;
	case CG_PLAYER_HOLDABLE:
		CG_DrawHoldableItem( &rect, font, scale, ownerDrawFlags & CG_SHOW_2DONLY );
		break;
	case CG_PLAYER_ITEM:
		CG_DrawPlayerItem( &rect, font, scale, ownerDrawFlags & CG_SHOW_2DONLY );
		break;
	case CG_PLAYER_WAVES:
		CG_DrawPlayerWaves( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_PLAYER_EQUIP1_ICON:
		CG_DrawEquipIcon( &rect, 1 );
		break;
	case CG_PLAYER_EQUIP1_VALUE:
		CG_DrawEquipValue( &rect, 1, font, scale, color, textStyle );
		break;
	case CG_PLAYER_EQUIP2_ICON:
		CG_DrawEquipIcon( &rect, 2 );
		break;
	case CG_PLAYER_EQUIP2_VALUE:
		CG_DrawEquipValue( &rect, 2, font, scale, color, textStyle );
		break;
	case CG_PLAYER_EQUIP1_KEY:
		CG_DrawEquipKeyHint( &rect, 1, font, scale, color, textStyle );
		break;
	case CG_PLAYER_EQUIP2_KEY:
		CG_DrawEquipKeyHint( &rect, 2, font, scale, color, textStyle );
		break;
	case CG_PLAYER_EQUIP3_ICON:
		CG_DrawEquipIcon( &rect, 3 );
		break;
	case CG_PLAYER_EQUIP3_VALUE:
		CG_DrawEquipValue( &rect, 3, font, scale, color, textStyle );
		break;
	case CG_PLAYER_EQUIP3_KEY:
		CG_DrawEquipKeyHint( &rect, 3, font, scale, color, textStyle );
		break;
	case CG_PLAYER_LIVES:
		CG_DrawPlayerLives( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_PLAYER_SPAWNPOINTS:
		CG_DrawPlayerSpawnpoints( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_PLAYER_HEALTH:
		CG_DrawPlayerHealth( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_PLAYER_WEAPON_RECHARGE:
		CG_DrawWeapRecharge( &rect, color, align );
		break;
	case CG_RED_SCORE:
		CG_DrawRedScore( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_BLUE_SCORE:
		CG_DrawBlueScore( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_RED_NAME:
		break;
	case CG_BLUE_NAME:
		break;
	case CG_BLUE_FLAGHEAD:
		break;
	case CG_BLUE_FLAGSTATUS:
		break;
	case CG_BLUE_FLAGNAME:
		break;
	case CG_RED_FLAGHEAD:
		break;
	case CG_RED_FLAGSTATUS:
		break;
	case CG_RED_FLAGNAME:
		break;
	case CG_HARVESTER_SKULLS:
		break;
	case CG_HARVESTER_SKULLS2D:
		break;
	case CG_ONEFLAG_STATUS:
		break;
	case CG_PLAYER_LOCATION:
		CG_DrawPlayerLocation( &rect, font, scale, color, textStyle );
		break;
	case CG_TEAM_COLOR:
		CG_DrawTeamColor( &rect, color );
		break;
	case CG_CTF_POWERUP:
		break;
	case CG_PLAYER_STATUS:
		break;
	case CG_AREA_SYSTEMCHAT:
		CG_DrawAreaSystemChat( &rect, font, scale, color, shader );
		break;
	case CG_AREA_TEAMCHAT:
		CG_DrawAreaTeamChat( &rect, font, scale, color, shader );
		break;
	case CG_AREA_CHAT:
		CG_DrawAreaChat( &rect, font, scale, color, shader );
		break;
	case CG_GAME_TYPE:
		CG_DrawGameType( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_GAME_STATUS:
		CG_DrawGameStatus( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_KILLER:
		CG_DrawKiller( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_ACCURACY:
	case CG_ASSISTS:
	case CG_DEFEND:
	case CG_EXCELLENT:
	case CG_IMPRESSIVE:
	case CG_PERFECT:
	case CG_GAUNTLET:
	case CG_CAPTURES:
		CG_DrawMedal( ownerDraw, &rect, font, scale, color, shader );
		break;
	case CG_SPECTATORS:
		CG_DrawTeamSpectators( &rect, font, scale, color, shader );
		break;
	case CG_TEAMINFO:
		break;
	case CG_CAPFRAGLIMIT:
		CG_DrawCapFragLimit( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_1STPLACE:
		CG_Draw1stPlace( &rect, font, scale, color, shader, textStyle );
		break;
	case CG_2NDPLACE:
		CG_Draw2ndPlace( &rect, font, scale, color, shader, textStyle );
		break;
	default:
		break;
	}
}

void CG_MouseEvent( int x, int y ) {
	int n;
	float minX, maxX, minY, maxY;

	if ( ( cg.predictedPlayerState.pm_type == PM_NORMAL || cg.predictedPlayerState.pm_type == PM_SPECTATOR ) && cg.showScores == qfalse ) {
		trap_Key_SetCatcher( 0 );
		return;
	}

	// widen clamp so cursor can reach real screen edges past the pillarboxed menu area
	if ( cg_fixedAspect.integer && cgs.screenXScale > 0 && cgs.screenYScale > 0 ) {
		minX = -cgs.screenXBias / cgs.screenXScale;
		maxX = 640 - minX;
		minY = -cgs.screenYBias / cgs.screenYScale;
		maxY = 480 - minY;
	} else {
		minX = 0;
		maxX = 640;
		minY = 0;
		maxY = 480;
	}

	cgs.cursorX += x;
	if ( cgs.cursorX < minX ) {
		cgs.cursorX = minX;
	} else if ( cgs.cursorX > maxX ) {
		cgs.cursorX = maxX;
	}

	cgs.cursorY += y;
	if ( cgs.cursorY < minY ) {
		cgs.cursorY = minY;
	} else if ( cgs.cursorY > maxY ) {
		cgs.cursorY = maxY;
	}

	n = Display_CursorType( cgs.cursorX, cgs.cursorY );
	cgs.activeCursor = 0;
	if ( n == CURSOR_ARROW ) {
		cgs.activeCursor = cgs.media.selectCursor;
	} else if ( n == CURSOR_SIZER ) {
		cgs.activeCursor = cgs.media.sizeCursor;
	}

	if ( cgs.capturedItem ) {
		Display_MouseMove( cgs.capturedItem, x, y );
	} else {
		Display_MouseMove( NULL, cgs.cursorX, cgs.cursorY );
	}

}

/*
==================
CG_EventHandling
==================
 type 0 - no event handling
      1 - team menu
      2 - hud editor

*/
void CG_EventHandling( int type ) {
	cgs.eventHandling = type;
	if ( type == CGAME_EVENT_NONE ) { // hide menus

	} else if ( type == CGAME_EVENT_TEAMMENU ) {

	} else if ( type == CGAME_EVENT_SCOREBOARD ) {

	}

}



void CG_KeyEvent( int key, qboolean down ) {

	if ( !down ) {
		return;
	}

	if ( cg.predictedPlayerState.pm_type == PM_NORMAL || ( cg.predictedPlayerState.pm_type == PM_SPECTATOR && cg.showScores == qfalse ) ) {
		CG_EventHandling( CGAME_EVENT_NONE );
		trap_Key_SetCatcher( 0 );
		return;
	}

	Display_HandleKey( key, down, cgs.cursorX, cgs.cursorY );

	if ( cgs.capturedItem ) {
		cgs.capturedItem = NULL;
	}   else {
		if ( key == K_MOUSE2 && down ) {
			cgs.capturedItem = Display_CaptureItem( cgs.cursorX, cgs.cursorY );
		}
	}
}

void CG_RunMenuScript( char **args ) {

}


void CG_GetTeamColor( vec4_t *color ) {
	if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_RED ) {
		( *color )[0] = 1;
		( *color )[3] = .25f;
		( *color )[1] = ( *color )[2] = 0;
	} else if ( cg.snap->ps.persistant[PERS_TEAM] == TEAM_BLUE ) {
		( *color )[0] = ( *color )[1] = 0;
		( *color )[2] = 1;
		( *color )[3] = .25f;
	} else {
		( *color )[0] = ( *color )[2] = 0;
		( *color )[1] = .17f;
		( *color )[3] = .25f;
	}
}
