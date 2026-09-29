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


/*****************************************************************************
 * name:		ai_main.c
 *
 * desc:		Quake3 bot AI
 *
 *
 *****************************************************************************/

#include "g_local.h"
#include "../qcommon/q_shared.h"
#include "../botlib/botlib.h"      //bot lib interface
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_char.h"
#include "../botlib/be_ai_chat.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_weap.h"
#include "../botlib/botai.h"          //bot ai interface

#include "ai_main.h"
//
#include "chars.h"
#include "inv.h"
#include "syn.h"

//bot states
bot_state_t *botstates[MAX_CLIENTS];
//number of bots
int numbots;
//
vmCvar_t bot_thinktime;
vmCvar_t memorydump;

// relocated from ai_dmq3.c/ai_chat.c when the dead Q3 multiplayer bot-AI files were removed;
// this is all that's left with a live caller in this file (\addbot connect/disconnect lifecycle).
#define MAX_WAYPOINTS 128
static bot_waypoint_t botai_waypoints[MAX_WAYPOINTS];
static bot_waypoint_t *botai_freewaypoints;

static int gametype;

#define CTF_SKIN_REDTEAM    "red"
#define CTF_SKIN_BLUETEAM   "blue"
#define CTF_TEAM_NONE       0
#define CTF_TEAM_RED        1
#define CTF_TEAM_BLUE       2

static vmCvar_t bot_grapple;
static vmCvar_t bot_rocketjump;
static vmCvar_t bot_fastchat;
static vmCvar_t bot_nochat;
static vmCvar_t bot_testrchat;

static void BotInitWaypoints( void ) {
	int i;

	botai_freewaypoints = NULL;
	for ( i = 0; i < MAX_WAYPOINTS; i++ ) {
		botai_waypoints[i].next = botai_freewaypoints;
		botai_freewaypoints = &botai_waypoints[i];
	}
}

static void BotFreeWaypoints( bot_waypoint_t *wp ) {
	bot_waypoint_t *nextwp;

	for (; wp; wp = nextwp ) {
		nextwp = wp->next;
		wp->next = botai_freewaypoints;
		botai_freewaypoints = wp;
	}
}

static void BotSetupDeathmatchAI( void ) {
	gametype = trap_Cvar_VariableIntegerValue( "g_gametype" );
	trap_Cvar_Register( &bot_rocketjump, "bot_rocketjump", "1", 0 );
	trap_Cvar_Register( &bot_grapple, "bot_grapple", "0", 0 );
	trap_Cvar_Register( &bot_fastchat, "bot_fastchat", "0", 0 );
	trap_Cvar_Register( &bot_nochat, "bot_nochat", "0", 0 );
	trap_Cvar_Register( &bot_testrchat, "bot_testrchat", "0", 0 );
	//initialize the waypoint heap
	BotInitWaypoints();
}

static int TeamPlayIsOn( void ) {
	return qfalse;
}

static int BotSameTeam( bot_state_t *bs, int entnum ) {
	if ( bs->client < 0 || bs->client >= MAX_CLIENTS ) {
		return qfalse;
	}
	if ( entnum < 0 || entnum >= MAX_CLIENTS ) {
		return qfalse;
	}
	return qfalse;
}

static char *ClientName( int client, char *name, int size ) {
	char buf[MAX_INFO_STRING];

	if ( client < 0 || client >= MAX_CLIENTS ) {
		BotAI_Print( PRT_ERROR, "ClientName: client out of range\n" );
		return "[client out of range]";
	}
	trap_GetConfigstring( CS_PLAYERS + client, buf, sizeof( buf ) );
	strncpy( name, Info_ValueForKey( buf, "n" ), size - 1 );
	name[size - 1] = '\0';
	Q_CleanStr( name );
	return name;
}

static char *ClientSkin( int client, char *skin, int size ) {
	char buf[MAX_INFO_STRING];

	if ( client < 0 || client >= MAX_CLIENTS ) {
		BotAI_Print( PRT_ERROR, "ClientSkin: client out of range\n" );
		return "[client out of range]";
	}
	trap_GetConfigstring( CS_PLAYERS + client, buf, sizeof( buf ) );
	strncpy( skin, Info_ValueForKey( buf, "model" ), size - 1 );
	skin[size - 1] = '\0';
	return skin;
}

static int BotCTFTeam( bot_state_t *bs ) {
	char skin[128], *p;

	if ( gametype <= GT_SINGLE_PLAYER ) {
		return CTF_TEAM_NONE;
	}
	ClientSkin( bs->client, skin, sizeof( skin ) );
	p = strchr( skin, '/' );
	if ( !p ) {
		p = skin;
	} else { p++;}
	if ( Q_stricmp( p, CTF_SKIN_REDTEAM ) == 0 ) {
		return CTF_TEAM_RED;
	}
	if ( Q_stricmp( p, CTF_SKIN_BLUETEAM ) == 0 ) {
		return CTF_TEAM_BLUE;
	}
	return CTF_TEAM_NONE;
}

static char *EasyClientName( int client, char *buf, int size ) {
	int i;
	char *str1, *str2, *ptr, c;
	char name[128] = {0};

	ClientName( client, name, sizeof( name ) );
	for ( i = 0; name[i]; i++ ) name[i] &= 127;
	//remove all spaces
	for ( ptr = strstr( name, " " ); ptr; ptr = strstr( name, " " ) ) {
		memmove( ptr, ptr + 1, strlen( ptr + 1 ) + 1 );
	}
	//check for [x] and ]x[ clan names
	str1 = strstr( name, "[" );
	str2 = strstr( name, "]" );
	if ( str1 && str2 ) {
		if ( str2 > str1 ) {
			memmove( str1, str2 + 1, strlen( str2 + 1 ) + 1 );
		} else { memmove( str2, str1 + 1, strlen( str1 + 1 ) + 1 );}
	}
	//remove Mr prefix
	if ( ( name[0] == 'm' || name[0] == 'M' ) &&
		 ( name[1] == 'r' || name[1] == 'R' ) ) {
		memmove( name, name + 2, strlen( name + 2 ) + 1 );
	}
	//only allow lower case alphabet characters
	ptr = name;
	while ( *ptr ) {
		c = *ptr;
		if ( ( c >= 'a' && c <= 'z' ) ||
			 ( c >= '0' && c <= '9' ) || c == '_' ) {
			ptr++;
		} else if ( c >= 'A' && c <= 'Z' )       {
			*ptr += 'a' - 'A';
			ptr++;
		} else {
			memmove( ptr, ptr + 1, strlen( ptr + 1 ) + 1 );
		}
	}
	strncpy( buf, name, size - 1 );
	buf[size - 1] = '\0';
	return buf;
}

static char *BotRandomOpponentName( bot_state_t *bs ) {
	int i, count;
	char buf[MAX_INFO_STRING];
	int opponents[MAX_CLIENTS], numopponents;
	static char name[32];

	numopponents = 0;
	opponents[0] = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		if ( i == bs->client ) {
			continue;
		}
		//
		trap_GetConfigstring( CS_PLAYERS + i, buf, sizeof( buf ) );
		//if no config string or no name
		if ( !strlen( buf ) || !strlen( Info_ValueForKey( buf, "n" ) ) ) {
			continue;
		}
		//skip spectators
		if ( atoi( Info_ValueForKey( buf, "t" ) ) == TEAM_SPECTATOR ) {
			continue;
		}
		//skip team mates
		if ( BotSameTeam( bs, i ) ) {
			continue;
		}
		//
		opponents[numopponents] = i;
		numopponents++;
	}
	count = random() * numopponents;
	for ( i = 0; i < numopponents; i++ ) {
		count--;
		if ( count <= 0 ) {
			EasyClientName( opponents[i], name, sizeof( name ) );
			return name;
		}
	}
	EasyClientName( opponents[0], name, sizeof( name ) );
	return name;
}

static char *BotMapTitle( void ) {
	char info[1024];
	static char mapname[128];

	trap_GetServerinfo( info, sizeof( info ) );

	strncpy( mapname, Info_ValueForKey( info, "mapname" ), sizeof( mapname ) - 1 );
	mapname[sizeof( mapname ) - 1] = '\0';

	return mapname;
}

static int BotNumActivePlayers( void ) {
	int i, num;
	char buf[MAX_INFO_STRING];

	num = 0;
	for ( i = 0; i < level.maxclients; i++ ) {
		trap_GetConfigstring( CS_PLAYERS + i, buf, sizeof( buf ) );
		//if no config string or no name
		if ( !strlen( buf ) || !strlen( Info_ValueForKey( buf, "n" ) ) ) {
			continue;
		}
		//skip spectators
		if ( atoi( Info_ValueForKey( buf, "t" ) ) == TEAM_SPECTATOR ) {
			continue;
		}
		//
		num++;
	}
	return num;
}

static int BotChat_ExitGame( bot_state_t *bs ) {
	char name[32];
	float rnd;

	if ( bot_nochat.integer ) {
		return qfalse;
	}
	if ( bs->lastchat_time > ( (float)level.time / 1000 ) - 3 ) {
		return qfalse;
	}
	//don't chat in teamplay
	if ( TeamPlayIsOn() ) {
		return qfalse;
	}
	rnd = trap_Characteristic_BFloat( bs->character, CHARACTERISTIC_CHAT_ENTEREXITGAME, 0, 1 );
	if ( !bot_fastchat.integer ) {
		if ( random() > rnd ) {
			return qfalse;
		}
	}
	if ( BotNumActivePlayers() <= 1 ) {
		return qfalse;
	}
	//
	BotAI_BotInitialChat( bs, "game_exit",
						  EasyClientName( bs->client, name, 32 ), // 0
						  BotRandomOpponentName( bs ),  // 1
						  "[invalid var]",          // 2
						  "[invalid var]",          // 3
						  BotMapTitle(),                // 4
						  NULL );
	bs->lastchat_time = (float)level.time / 1000;
	bs->chatto = CHAT_ALL;
	return qtrue;
}

/*
==================
BotAI_Print
==================
*/
void QDECL BotAI_Print( int type, char *fmt, ... ) {
	char str[2048];
	va_list ap;

	va_start( ap, fmt );
	Q_vsnprintf(str, sizeof(str), fmt, ap);
	va_end( ap );

	switch ( type ) {
	case PRT_MESSAGE: {
		G_Printf( "%s", str );
		break;
	}
	case PRT_WARNING: {
		G_Printf( S_COLOR_YELLOW "Warning: %s", str );
		break;
	}
	case PRT_ERROR: {
		G_Printf( S_COLOR_RED "Error: %s", str );
		break;
	}
	case PRT_FATAL: {
		G_Printf( S_COLOR_RED "Fatal: %s", str );
		break;
	}
	case PRT_EXIT: {
		G_Error( S_COLOR_RED "Exit: %s", str );
		break;
	}
	default: {
		G_Printf( "unknown print type\n" );
		break;
	}
	}
}

/*
==================
BotAI_Trace
==================
*/
void BotAI_Trace( bsp_trace_t *bsptrace, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask ) {
	trace_t trace;

	trap_Trace( &trace, start, mins, maxs, end, passent, contentmask );
	//copy the trace information
	bsptrace->allsolid = trace.allsolid;
	bsptrace->startsolid = trace.startsolid;
	bsptrace->fraction = trace.fraction;
	VectorCopy( trace.endpos, bsptrace->endpos );
	bsptrace->plane.dist = trace.plane.dist;
	VectorCopy( trace.plane.normal, bsptrace->plane.normal );
	bsptrace->plane.signbits = trace.plane.signbits;
	bsptrace->plane.type = trace.plane.type;
	bsptrace->surface.value = 0;
	bsptrace->surface.flags = trace.surfaceFlags;
	bsptrace->ent = trace.entityNum;
	bsptrace->exp_dist = 0;
	bsptrace->sidenum = 0;
	bsptrace->contents = 0;
}

/*
==================
BotAI_GetClientState
==================
*/
int BotAI_GetClientState( int clientNum, playerState_t *state ) {
	gentity_t   *ent;

	ent = &g_entities[clientNum];
	if ( !ent->inuse ) {
		return qfalse;
	}
	if ( !ent->client ) {
		return qfalse;
	}

	memcpy( state, &ent->client->ps, sizeof( playerState_t ) );
	return qtrue;
}

/*
==================
BotAI_GetEntityState
==================
*/
int BotAI_GetEntityState( int entityNum, entityState_t *state ) {
	gentity_t   *ent;

	ent = &g_entities[entityNum];
	memset( state, 0, sizeof( entityState_t ) );
	if ( !ent->inuse ) {
		return qfalse;
	}
	if ( !ent->r.linked ) {
		return qfalse;
	}
	if ( ent->r.svFlags & SVF_NOCLIENT ) {
		return qfalse;
	}
	memcpy( state, &ent->s, sizeof( entityState_t ) );
	return qtrue;
}

/*
==================
BotAI_GetSnapshotEntity
==================
*/
int BotAI_GetSnapshotEntity( int clientNum, int sequence, entityState_t *state ) {
	int entNum;

	entNum = trap_BotGetSnapshotEntity( clientNum, sequence );
	if ( entNum == -1 ) {
		memset( state, 0, sizeof( entityState_t ) );
		return -1;
	}

	BotAI_GetEntityState( entNum, state );

	return sequence + 1;
}

/*
==================
BotAI_BotInitialChat
==================
*/
void QDECL BotAI_BotInitialChat( bot_state_t *bs, char *type, ... ) {
	int i, mcontext;
	va_list ap;
	char    *p;
	char    *vars[MAX_MATCHVARIABLES];

	memset( vars, 0, sizeof( vars ) );
	va_start( ap, type );
	p = va_arg( ap, char * );
	for ( i = 0; i < MAX_MATCHVARIABLES; i++ ) {
		if ( !p ) {
			break;
		}
		vars[i] = p;
		p = va_arg( ap, char * );
	}
	va_end( ap );

	mcontext = CONTEXT_NORMAL | CONTEXT_NEARBYITEM | CONTEXT_NAMES;
	if ( BotCTFTeam( bs ) == CTF_TEAM_RED ) {
		mcontext |= CONTEXT_CTFREDTEAM;
	} else { mcontext |= CONTEXT_CTFBLUETEAM;}

	trap_BotInitialChat( bs->cs, type, mcontext, vars[0], vars[1], vars[2], vars[3], vars[4], vars[5], vars[6], vars[7] );
}

/*
==============
NumBots
==============
*/
int NumBots( void ) {
	return numbots;
}

/*
==============
AngleDifference
==============
*/
float AngleDifference( float ang1, float ang2 ) {
	float diff;

	diff = ang1 - ang2;
	if ( ang1 > ang2 ) {
		if ( diff > 180.0 ) {
			diff -= 360.0;
		}
	} else {
		if ( diff < -180.0 ) {
			diff += 360.0;
		}
	}
	return diff;
}

/*
==============
BotChangeViewAngle
==============
*/
float BotChangeViewAngle( float angle, float ideal_angle, float speed ) {
	float move;

	angle = AngleMod( angle );
	ideal_angle = AngleMod( ideal_angle );
	if ( angle == ideal_angle ) {
		return angle;
	}
	move = ideal_angle - angle;
	if ( ideal_angle > angle ) {
		if ( move > 180.0 ) {
			move -= 360.0;
		}
	} else {
		if ( move < -180.0 ) {
			move += 360.0;
		}
	}
	if ( move > 0 ) {
		if ( move > speed ) {
			move = speed;
		}
	} else {
		if ( move < -speed ) {
			move = -speed;
		}
	}
	return AngleMod( angle + move );
}

/*
==============
BotChangeViewAngles
==============
*/
void BotChangeViewAngles( bot_state_t *bs, float thinktime ) {
	float diff, factor, maxchange, anglespeed;
	int i;

	if ( bs->ideal_viewangles[PITCH] > 180 ) {
		bs->ideal_viewangles[PITCH] -= 360;
	}
	//
	if ( bs->enemy >= 0 ) {
		factor = trap_Characteristic_BFloat( bs->character, CHARACTERISTIC_VIEW_FACTOR, 0.01, 1 );
		maxchange = trap_Characteristic_BFloat( bs->character, CHARACTERISTIC_VIEW_MAXCHANGE, 1, 1800 );
	} else {
		factor = 0.25;
		maxchange = 300;
	}
	maxchange *= thinktime;
	for ( i = 0; i < 2; i++ ) {
		diff = fabs( AngleDifference( bs->viewangles[i], bs->ideal_viewangles[i] ) );
		anglespeed = diff * factor;
		if ( anglespeed > maxchange ) {
			anglespeed = maxchange;
		}
		bs->viewangles[i] = BotChangeViewAngle( bs->viewangles[i],
												bs->ideal_viewangles[i], anglespeed );
		//BotAI_Print(PRT_MESSAGE, "ideal_angles %f %f\n", bs->ideal_viewangles[0], bs->ideal_viewangles[1], bs->ideal_viewangles[2]);`
		//bs->viewangles[i] = bs->ideal_viewangles[i];
	}
	if ( bs->viewangles[PITCH] > 180 ) {
		bs->viewangles[PITCH] -= 360;
	}
	//elementary action: view
	trap_EA_View( bs->client, bs->viewangles );
}

/*
==============
BotInputToUserCommand
==============
*/
void BotInputToUserCommand( bot_input_t *bi, usercmd_t *ucmd, int delta_angles[3], int time ) {
	vec3_t angles, forward, right;
	short temp;
	int j;
	float f, r, u, m;

	//clear the whole structure
	memset( ucmd, 0, sizeof( usercmd_t ) );
	//
	//the duration for the user command in milli seconds
	ucmd->serverTime = time;
	//
	if ( bi->actionflags & ACTION_DELAYEDJUMP ) {
		bi->actionflags |= ACTION_JUMP;
		bi->actionflags &= ~ACTION_DELAYEDJUMP;
	}
	//set the buttons
	if ( bi->actionflags & ACTION_RESPAWN ) {
		ucmd->buttons = BUTTON_ATTACK;
	}
	if ( bi->actionflags & ACTION_ATTACK ) {
		ucmd->buttons |= BUTTON_ATTACK;
	}
	if ( bi->actionflags & ACTION_TALK ) {
		ucmd->buttons |= BUTTON_TALK;
	}
	if ( bi->actionflags & ACTION_GESTURE ) {
		ucmd->buttons |= BUTTON_GESTURE;
	}
	if ( bi->actionflags & ACTION_USE ) {
		ucmd->buttons |= BUTTON_USE_HOLDABLE;
	}
	if ( bi->actionflags & ACTION_WALK ) {
		ucmd->buttons |= BUTTON_WALKING;
	}
	ucmd->weapon = bi->weapon;
	//set the view angles
	//NOTE: the ucmd->angles are the angles WITHOUT the delta angles
	ucmd->angles[PITCH] = ANGLE2SHORT( bi->viewangles[PITCH] );
	ucmd->angles[YAW] = ANGLE2SHORT( bi->viewangles[YAW] );
	ucmd->angles[ROLL] = ANGLE2SHORT( bi->viewangles[ROLL] );
	//subtract the delta angles
	for ( j = 0; j < 3; j++ ) {
		temp = ucmd->angles[j] - delta_angles[j];
		/*NOTE: disabled because temp should be mod first
		if ( j == PITCH ) {
			// don't let the player look up or down more than 90 degrees
			if ( temp > 16000 ) temp = 16000;
			else if ( temp < -16000 ) temp = -16000;
		}
		*/
		ucmd->angles[j] = temp;
	}
	//NOTE: movement is relative to the REAL view angles
	//get the horizontal forward and right vector
	//get the pitch in the range [-180, 180]
	if ( bi->dir[2] ) {
		angles[PITCH] = bi->viewangles[PITCH];
	} else { angles[PITCH] = 0;}
	angles[YAW] = bi->viewangles[YAW];
	angles[ROLL] = 0;
	AngleVectors( angles, forward, right, NULL );
	//bot input speed is in the range [0, 400]
	bi->speed = bi->speed * 127 / 400;
	//set the view independent movement
	f = DotProduct(forward, bi->dir);
	r = DotProduct(right, bi->dir);
	u = fabs(forward[2]) * bi->dir[2];
	m = fabs(f);

	if (fabs(r) > m) {
		m = fabs(r);
	}

	if (fabs(u) > m) {
		m = fabs(u);
	}

	if (m > 0) {
		f *= bi->speed / m;
		r *= bi->speed / m;
		u *= bi->speed / m;
	}

	ucmd->forwardmove = f;
	ucmd->rightmove = r;
	ucmd->upmove = u;

	if (bi->actionflags & ACTION_MOVEFORWARD) ucmd->forwardmove = 127;
	if (bi->actionflags & ACTION_MOVEBACK) ucmd->forwardmove = -127;
	if (bi->actionflags & ACTION_MOVELEFT) ucmd->rightmove = -127;
	if (bi->actionflags & ACTION_MOVERIGHT) ucmd->rightmove = 127;
	//jump/moveup
	if (bi->actionflags & ACTION_JUMP) ucmd->upmove = 127;
	//crouch/movedown
	if (bi->actionflags & ACTION_CROUCH) ucmd->upmove = -127;
}

/*
==============
BotUpdateInput
==============
*/
void BotUpdateInput( bot_state_t *bs, int time ) {
	bot_input_t bi;
	int j;

	//add the delta angles to the bot's current view angles
	for ( j = 0; j < 3; j++ ) {
		bs->viewangles[j] = AngleMod( bs->viewangles[j] + SHORT2ANGLE( bs->cur_ps.delta_angles[j] ) );
	}
	//
	BotChangeViewAngles( bs, (float) time / 1000 );
	trap_EA_GetInput( bs->client, (float) time / 1000, &bi );
	//respawn hack
	if ( bi.actionflags & ACTION_RESPAWN ) {
		if ( bs->lastucmd.buttons & BUTTON_ATTACK ) {
			bi.actionflags &= ~( ACTION_RESPAWN | ACTION_ATTACK );
		}
	}
	//
	BotInputToUserCommand( &bi, &bs->lastucmd, bs->cur_ps.delta_angles, time );
	bs->lastucmd.serverTime = time;
	//subtract the delta angles
	for ( j = 0; j < 3; j++ ) {
		bs->viewangles[j] = AngleMod( bs->viewangles[j] - SHORT2ANGLE( bs->cur_ps.delta_angles[j] ) );
	}
}

/*
==================
BotScheduleBotThink
==================
*/
void BotScheduleBotThink( void ) {
	int i, botnum;

	botnum = 0;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( !botstates[i] || !botstates[i]->inuse ) {
			continue;
		}
		//initialize the bot think residual time
		botstates[i]->botthink_residual = bot_thinktime.integer * botnum / numbots;
		botnum++;
	}
}

/*
==============
BotAISetupClient
==============
*/
int BotAISetupClient( int client, struct bot_settings_s *settings ) {
	char filename[144], name[144], gender[144];
	bot_state_t *bs;
	int errnum;

	if ( !botstates[client] ) {
		botstates[client] = G_Alloc( sizeof( bot_state_t ) );
	}
	bs = botstates[client];

	if ( !bs ) {
		return qfalse;
	}

	if ( bs && bs->inuse ) {
		BotAI_Print( PRT_FATAL, "client %d already setup\n", client );
		return qfalse;
	}

	//load the bot character
	bs->character = trap_BotLoadCharacter( settings->characterfile, settings->skill );
	if ( !bs->character ) {
		BotAI_Print( PRT_FATAL, "couldn't load skill %f from %s\n", settings->skill, settings->characterfile );
		return qfalse;
	}
	//copy the settings
	memcpy( &bs->settings, settings, sizeof( bot_settings_t ) );
	//allocate a weapon state
	bs->ws = trap_BotAllocWeaponState();
	//load the weapon weights
	trap_Characteristic_String( bs->character, CHARACTERISTIC_WEAPONWEIGHTS, filename, sizeof( filename ) );
	errnum = trap_BotLoadWeaponWeights( bs->ws, filename );
	if ( errnum != BLERR_NOERROR ) {
		trap_BotFreeWeaponState( bs->ws );
		return qfalse;
	}
	//allocate a chat state
	bs->cs = trap_BotAllocChatState();
	//load the chat file
	trap_Characteristic_String( bs->character, CHARACTERISTIC_CHAT_FILE, filename, sizeof( filename ) );
	trap_Characteristic_String( bs->character, CHARACTERISTIC_CHAT_NAME, name, sizeof( name ) );
	errnum = trap_BotLoadChatFile( bs->cs, filename, name );
	if ( errnum != BLERR_NOERROR ) {
		trap_BotFreeChatState( bs->cs );
		trap_BotFreeWeaponState( bs->ws );
		return qfalse;
	}
	//get the gender characteristic
	trap_Characteristic_String( bs->character, CHARACTERISTIC_GENDER, gender, sizeof( gender ) );
	//set the chat gender
	if ( *gender == 'f' || *gender == 'F' ) {
		trap_BotSetChatGender( bs->cs, CHAT_GENDERFEMALE );
	} else if ( *gender == 'm' || *gender == 'M' )  {
		trap_BotSetChatGender( bs->cs, CHAT_GENDERMALE );
	} else { trap_BotSetChatGender( bs->cs, CHAT_GENDERLESS );}

	bs->inuse = qtrue;
	bs->client = client;
	bs->entitynum = client;
	bs->setupcount = 4;
	bs->walker = trap_Characteristic_BFloat( bs->character, CHARACTERISTIC_WALKER, 0, 1 );
	numbots++;

	//NOTE: reschedule the bot thinking
	BotScheduleBotThink();
	//
	return qtrue;
}

/*
==============
BotAIShutdownClient
==============
*/
int BotAIShutdownClient( int client ) {
	bot_state_t *bs;

	// Wolfenstein
	if ( g_entities[client].r.svFlags & SVF_CASTAI ) {
		AICast_ShutdownClient( client );
		return BLERR_NOERROR;
	}
	// done.

	bs = botstates[client];
	if ( !bs || !bs->inuse ) {
		// BotAI_Print(PRT_ERROR, "client %d already shutdown\n", client);
		return BLERR_AICLIENTALREADYSHUTDOWN;
	}

	if ( BotChat_ExitGame( bs ) ) {
		trap_BotEnterChat( bs->cs, bs->client, CHAT_ALL );
	}

	//free the chat file
	trap_BotFreeChatState( bs->cs );
	//free the weapon weights
	trap_BotFreeWeaponState( bs->ws );
	//free the bot character
	trap_BotFreeCharacter( bs->character );
	//
	BotFreeWaypoints( bs->checkpoints );
	BotFreeWaypoints( bs->patrolpoints );
	//clear the bot state
	memset( bs, 0, sizeof( bot_state_t ) );
	//set the inuse flag to qfalse
	bs->inuse = qfalse;
	//there's one bot less
	numbots--;
	//everything went ok
	return BLERR_NOERROR;
}

/*
==============
BotResetState

called when a bot enters the intermission or observer mode and
when the level is changed
==============
*/
void BotResetState( bot_state_t *bs ) {
	int client, entitynum, inuse;
	int chatstate, weaponstate;
	bot_settings_t settings;
	int character;
	playerState_t ps;                           //current player state

	//save some things that should not be reset here
	memcpy( &settings, &bs->settings, sizeof( bot_settings_t ) );
	memcpy( &ps, &bs->cur_ps, sizeof( playerState_t ) );
	inuse = bs->inuse;
	client = bs->client;
	entitynum = bs->entitynum;
	character = bs->character;
	chatstate = bs->cs;
	weaponstate = bs->ws;
	//free checkpoints and patrol points
	BotFreeWaypoints( bs->checkpoints );
	BotFreeWaypoints( bs->patrolpoints );
	//reset the whole state
	memset( bs, 0, sizeof( bot_state_t ) );
	//copy back some state stuff that should not be reset
	bs->cs = chatstate;
	bs->ws = weaponstate;
	memcpy( &bs->cur_ps, &ps, sizeof( playerState_t ) );
	memcpy( &bs->settings, &settings, sizeof( bot_settings_t ) );
	bs->inuse = inuse;
	bs->client = client;
	bs->entitynum = entitynum;
	bs->character = character;
	//reset several states
	if ( bs->ws ) {
		trap_BotResetWeaponState( bs->ws );
	}
}

/*
==============
BotAILoadMap
==============
*/
int BotAILoadMap( int restart ) {
	int i;
	vmCvar_t mapname;

	if ( !restart ) {
		trap_Cvar_Register( &mapname, "mapname", "", CVAR_SERVERINFO | CVAR_ROM );
		trap_BotLibLoadMap( mapname.string );
		trap_Nav_LoadMap( mapname.string );
	}

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( botstates[i] && botstates[i]->inuse ) {
			BotResetState( botstates[i] );
			botstates[i]->setupcount = 4;
		}
	}

	BotSetupDeathmatchAI();

	return BLERR_NOERROR;
}

/*
==================
BotAIStartFrame
==================
*/
int BotAIStartFrame( int time ) {
	int elapsed_time, thinktime;
	static int local_time;
	static int botlib_residual;
	static int lastbotthink_time;

	trap_Cvar_Update( &bot_rocketjump );
	trap_Cvar_Update( &bot_grapple );
	trap_Cvar_Update( &bot_fastchat );
	trap_Cvar_Update( &bot_nochat );
	trap_Cvar_Update( &bot_testrchat );
	trap_Cvar_Update( &bot_thinktime );
	trap_Cvar_Update( &nav_debugmesh );
	trap_Cvar_Update( &nav_debugpath );
	trap_Nav_SelectClass( 0 );
	trap_Cvar_Update( &memorydump );

	if ( memorydump.integer ) {
		trap_BotLibVarSet( "memorydump", "1" );
		trap_Cvar_Set( "memorydump", "0" );
	}

	//if the bot think time changed we should reschedule the bots
	if ( bot_thinktime.integer != lastbotthink_time ) {
		lastbotthink_time = bot_thinktime.integer;
		BotScheduleBotThink();
	}

	elapsed_time = time - local_time;
	local_time = time;

	botlib_residual += elapsed_time;

	if ( elapsed_time > bot_thinktime.integer ) {
		thinktime = elapsed_time;
	} else { thinktime = bot_thinktime.integer;}

	// update the bot library
	if ( botlib_residual >= thinktime ) {
		botlib_residual -= thinktime;

		trap_BotLibStartFrame( (float) time / 1000 );

		// Ridah, only check the default world
		trap_Nav_SelectClass( 0 );
	}

	return BLERR_NOERROR;
}

/*
==============
BotInitLibrary
==============
*/
int BotInitLibrary( void ) {
	char buf[144];

	//set the maxclients and maxentities library variables before calling BotSetupLibrary
	Com_sprintf( buf, sizeof( buf ), "%d", level.maxclients );
	trap_BotLibVarSet( "maxclients", buf );
	Com_sprintf( buf, sizeof( buf ), "%d", MAX_GENTITIES );
	trap_BotLibVarSet( "maxentities", buf );
	//bsp checksum
	trap_Cvar_VariableStringBuffer( "sv_mapChecksum", buf, sizeof( buf ) );
	if ( strlen( buf ) ) {
		trap_BotLibVarSet( "sv_mapChecksum", buf );
	}
	//maximum number of items in a level
	trap_Cvar_VariableStringBuffer( "max_levelitems", buf, sizeof( buf ) );
	if ( strlen( buf ) ) {
		trap_BotLibVarSet( "max_levelitems", buf );
	}
	//
	trap_Cvar_VariableStringBuffer( "g_gametype", buf, sizeof( buf ) );
	if ( !strlen( buf ) ) {
		strcpy( buf, "0" );
	}
	trap_BotLibVarSet( "g_gametype", buf );
	//
	// Rafael gameskill
	trap_Cvar_VariableStringBuffer( "g_gameskill", buf, sizeof( buf ) );
	if ( !strlen( buf ) ) {
		strcpy( buf, "0" );
	}
	trap_BotLibVarSet( "g_gamekill", buf );
	// done
	//
	trap_Cvar_VariableStringBuffer( "bot_developer", buf, sizeof( buf ) );
	if ( !strlen( buf ) ) {
		strcpy( buf, "0" );
	}
	trap_BotLibVarSet( "bot_developer", buf );
	//log file
	trap_Cvar_VariableStringBuffer( "bot_developer", buf, sizeof( buf ) );
	if ( !strlen( buf ) ) {
		strcpy( buf, "0" );
	}
	trap_Cvar_VariableStringBuffer( "logfile", buf, sizeof( buf ) );
	trap_BotLibVarSet( "log", buf );
	//no chatting
	trap_Cvar_VariableStringBuffer( "bot_nochat", buf, sizeof( buf ) );
	if ( strlen( buf ) ) {
		trap_BotLibVarSet( "nochat", buf );
	}
	//
	trap_Cvar_VariableStringBuffer( "bot_reloadcharacters", buf, sizeof( buf ) );
	if ( !strlen( buf ) ) {
		strcpy( buf, "0" );
	}
	trap_BotLibVarSet( "bot_reloadcharacters", buf );
	//base directory
	trap_Cvar_VariableStringBuffer( "fs_basepath", buf, sizeof( buf ) );
	if ( strlen( buf ) ) {
		trap_BotLibVarSet( "basedir", buf );
	}
	//game directory
	trap_Cvar_VariableStringBuffer( "fs_game", buf, sizeof( buf ) );
	if ( strlen( buf ) ) {
		trap_BotLibVarSet( "gamedir", buf );
	}
	//home directory
	trap_Cvar_VariableStringBuffer( "fs_homepath", buf, sizeof( buf ) );
	if ( strlen( buf ) ) {
		trap_BotLibVarSet("homedir", buf);
	}
	//setup the bot library
	return trap_BotLibSetup();
}

/*
==============
BotAISetup
==============
*/
int BotAISetup( int restart ) {
	int errnum;

#ifdef RANDOMIZE
	srand( (unsigned)time( NULL ) );
#endif //RANDOMIZE

	trap_Cvar_Register( &bot_thinktime, "bot_thinktime", "100", 0 );
	trap_Cvar_Register( &memorydump, "memorydump", "0", 0 );

	//if the game is restarted for a tournament
	if ( restart ) {
		return BLERR_NOERROR;
	}

	//initialize the bot states
	memset( botstates, 0, sizeof( botstates ) );

	trap_Cvar_Register( &bot_thinktime, "bot_thinktime", "100", 0 );

	errnum = BotInitLibrary();
	if ( errnum != BLERR_NOERROR ) {
		return qfalse;
	}
	return BLERR_NOERROR;
}

/*
==============
BotAIShutdown
==============
*/
int BotAIShutdown( int restart ) {

	int i;

	//if the game is restarted for a tournament
	if ( restart ) {
		//shutdown all the bots in the botlib
		for ( i = 0; i < MAX_CLIENTS; i++ ) {
			if ( botstates[i] && botstates[i]->inuse ) {
				BotAIShutdownClient( botstates[i]->client );
			}
		}
		//don't shutdown the bot library
	} else {
		trap_BotLibShutdown();
	}
	return qtrue;
}

