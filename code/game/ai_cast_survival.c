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
 * name:		ai_cast_survival.c
 *
 * desc:		Wolfenstein AI Character Survival
 *
*/

#include <stdlib.h> // For rand()
#include <stdio.h>  // For snprintf()

#include "g_local.h"
#include "../qcommon/q_shared.h"
#include "../botlib/botlib.h"      //bot lib interface
#include "../botlib/be_aas.h"
#include "../botlib/be_ea.h"
#include "../botlib/be_ai_gen.h"
#include "../botlib/be_ai_goal.h"
#include "../botlib/be_ai_move.h"
#include "../botlib/botai.h"          //bot ai interface

#include "ai_cast.h"
#include "g_survival.h"

#include "../steam/steam.h"

int BotPointAreaNum( vec3_t origin );  // ai_dmq3.c - handles floor-edge cases raw trap_AAS_PointAreaNum misses

#define PREPARE_TIME 15
#define INTERMISSION_TIME 15
#define INITIAL_KILLCOUNT_REQ 5
#define FRIENDLY_SPAWN_TIME 15

#define WAVE_EG 3
#define WAVE_BG 5
#define WAVE_VENOM 7
#define WAVE_PROTO 9
#define WAVE_WARRIORS 3
#define WAVE_HELGA 10
#define WAVE_HEINRICH 15
#define WAVE_LOPERS 5
#define WAVE_FLAMERS 4

#define MAX_SOLDIERS_SURVIVAL 10
#define SOLDIERS_INCREASE 1

#define MAX_ZOMBIES_SURVIVAL 20
#define ZOMBIES_INCREASE 1

#define MAX_FLAMERS_SURVIVAL 3
#define FLAMERS_INCREASE 1

#define MAX_WARRIORS_SURVIVAL 5
#define WARRIORS_INCREASE 1

#define MAX_ELITES_SURVIVAL 4
#define ELITES_INCREASE 1

#define MAX_BLACKGUARDS_SURVIVAL 4
#define BLACKGUARDS_INCREASE 1

#define MAX_HELGAS_SURVIVAL 1
#define HELGAS_INCREASE 1

#define MAX_HEINRICHS_SURVIVAL 1
#define HEINRICHS_INCREASE 1

#define MAX_LOPERS_SURVIVAL 3
#define LOPERS_INCREASE 1

#define MAX_VENOMS_SURVIVAL 4
#define VENOMS_INCREASE 1

#define MAX_PROTOS_SURVIVAL 2
#define PROTOS_INCREASE 1

#define SPECIAL_WAVE_CHANCE 30
#define SPECIAL_WAVE_MIN_START 4
#define SPECIAL_WAVE_MIN_GAP 2
#define SPECIAL_WAVE_MAX_GAP 5
#define SPECIAL_WAVE_LOPERS_INITIAL 3
#define SPECIAL_WAVE_LOPERS_INCREASE 1
#define SPECIAL_WAVE_LOPERS_MAX 8

/*
============
AICast_InitSurvival
============
*/
void AICast_InitSurvival(void) {
	svParams.killCountRequirement = INITIAL_KILLCOUNT_REQ;
	svParams.spawnedThisWave = 0;
	svParams.spawnedThisWaveFriendly = 0;
	svParams.waveCount = 0;
	svParams.waveInProgress = qfalse;
	svParams.wavePending = qtrue;
	svParams.waveChangeTime = level.time + PREPARE_TIME * 1000;

    // Special wave defaults
    svParams.specialWaveActive    = qfalse;
    svParams.lastSpecialWave      = 0;

	svParams.maxActiveAI[AICHAR_SOLDIER] = 5;
	svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] = 5;
	svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] = 0;
	svParams.maxActiveAI[AICHAR_WARZOMBIE] = 0;
	svParams.maxActiveAI[AICHAR_PROTOSOLDIER] = 0;
	svParams.maxActiveAI[AICHAR_PARTISAN] = 2;
	svParams.maxActiveAI[AICHAR_ELITEGUARD] = 0;
	svParams.maxActiveAI[AICHAR_BLACKGUARD] = 0;
	svParams.maxActiveAI[AICHAR_VENOM] = 0;
	svParams.maxActiveAI[AICHAR_LOPER] = 0;
	svParams.maxActiveAI[AICHAR_HELGA] = 0;
	svParams.maxActiveAI[AICHAR_HEINRICH] = 0;
}


/*
============
AICast_CreateCharacter_Survival

Applies Survival mode overrides after character creation
============
*/
void AICast_CreateCharacter_Survival(gentity_t *newent, cast_state_t *cs) {
    // If the character is friendly AI (aiTeam == 1), set respawnsleft to 0
    if (newent->aiTeam == 1) {
        cs->respawnsleft = 0;
    } else {
        // Unlimited respawn for other AI
        cs->respawnsleft = -1;
    }
	cs->registeredSurvivalKill = qfalse;
}


/*
============
AIChar_AIScript_AlertEntity_Survival

  triggered spawning, called from AI scripting
============
*/
void AIChar_AIScript_AlertEntity_Survival( gentity_t *ent ) {
	
	vec3_t mins, maxs;
	int numTouch, touch[10], i;
	cast_state_t    *cs;
	vec3_t spawn_origin, spawn_angles;

	gentity_t *player = AICast_FindEntityForName( "player" );

	if ( !ent->aiInactive ) {
		return;
	}

	cs = AICast_GetCastState( ent->s.number );

	if (svParams.specialWaveActive && ent->aiTeam != 1 && ent->aiCharacter != AICHAR_LOPER_SPECIAL)
	{
		cs->aiFlags |= AIFL_WAITINGTOSPAWN;
		return;
	}

	if (!svParams.specialWaveActive && ent->aiTeam != 1 && ent->aiCharacter == AICHAR_LOPER_SPECIAL)
	{
		cs->aiFlags |= AIFL_WAITINGTOSPAWN;
		return;
	}

	// if the current bounding box is invalid, then wait
	VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
	VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );
	trap_UnlinkEntity( ent );

	numTouch = trap_EntitiesInBox( mins, maxs, touch, 10 );

	// check that another client isn't inside us
	if ( numTouch ) {
		for ( i = 0; i < numTouch; i++ ) {
			// RF, note we should only check against clients since zombies need to spawn inside func_explosive (so they dont clip into view after it explodes)
			if ( g_entities[touch[i]].client && g_entities[touch[i]].r.contents == CONTENTS_BODY ) {
				//if (g_entities[touch[i]].r.contents & MASK_PLAYERSOLID)
				break;
			}
		}
		if ( i == numTouch ) {
			numTouch = 0;
		}
	}

	if ( numTouch ) {
		// invalid location
		cs->aiFlags |= AIFL_WAITINGTOSPAWN;
		return;
	}

    
	   if ( svParams.activeAI[ent->aiCharacter] >= svParams.maxActiveAI[ent->aiCharacter] || !svParams.waveInProgress)  { 
		cs->aiFlags |= AIFL_WAITINGTOSPAWN;
		return;
	   }

	    // Prevent friendly AI from spawning if respawnsleft is 0
		if (ent->aiTeam == 1 && cs->respawnsleft == 0) {
			cs->aiFlags |= AIFL_WAITINGTOSPAWN; // Mark as waiting to spawn
			return;
		}

	// Selecting the spawn point for the AI
				SelectSpawnPoint_AI( player, ent, spawn_origin, spawn_angles );
				G_SetOrigin( ent, spawn_origin );
				VectorCopy( spawn_origin, ent->client->ps.origin );
				SetClientViewAngle( ent, spawn_angles );
				// Increment the counter for active AI characters
                svParams.activeAI[ent->aiCharacter]++;

	// RF, has to disable this so I could test some maps which have erroneously placed alertentity calls
	//ent->AIScript_AlertEntity = NULL;
	cs->aiFlags &= ~AIFL_WAITINGTOSPAWN;
	ent->aiInactive = qfalse;
	trap_LinkEntity( ent );

	// trigger a spawn script event
	AICast_ScriptEvent( AICast_GetCastState( ent->s.number ), "respawn", "" );

	// Increment spawned counters based on aiTeam
	if (ent->aiTeam == 1) { 
		svParams.spawnedThisWaveFriendly++;
	} else { 
		svParams.spawnedThisWave++;
	}

	// make it think so we update animations/angles
	AICast_Think( ent->s.number, (float)FRAMETIME / 1000 );
	cs->lastThink = level.time;
	AICast_UpdateInput( cs, FRAMETIME );
	trap_BotUserCommand( cs->bs->client, &( cs->lastucmd ) );
}


/*
===============
AICast_RegisterSurvivalKill

Handles wave and survival kill counters + progression trigger.
Call this from AICast_Die_Survival.
===============
*/
void AICast_RegisterSurvivalKill(gentity_t *self, gentity_t *attacker, int meansOfDeath) {
	
	if (!self)
	{
		Com_Printf("^1[AI_SURVIVE] ERROR: AICast_RegisterSurvivalKill called with NULL self\n");
		return;
	}

	// ===== GUARD AGAINST DOUBLE REGISTRATION =====
	cast_state_t* cs = AICast_GetCastState(self->s.number);
	if (cs->registeredSurvivalKill) {
		Com_Printf("^1[AI_SURVIVE] WARNING: Duplicate kill registration for %s (ent %d)\n",
			self->aiName ? self->aiName : "UNKNOWN", self->s.number);
		return;
	}
	cs->registeredSurvivalKill = qtrue;

	 // Skip counting if the dying entity is a friendly AI (aiTeam == 1)
    if (self->aiCharacter && self->aiTeam == 1) {
        //Com_Printf("^3[AI_SURVIVE] INFO: Friendly AI death ignored. aiCharacter=%d, aiTeam=%d\n", self->aiCharacter, self->aiTeam);
        return;
    }

	if (!svParams.waveInProgress){
		Com_Printf("^3[AI_SURVIVE] INFO: Wave not in progress, kill not counted. aiCharacter=%d, aiTeam=%d\n", self->aiCharacter, self->aiTeam);
		return;
	}

	qboolean killerPlayer   = attacker->client && !attacker->aiCharacter;
	qboolean killerFriendly = attacker->aiCharacter && attacker->aiTeam == 1;

if (!attacker) {
        Com_Printf("^1[AI_SURVIVE] WARNING: AI %s died with no attacker (meansOfDeath=%d)\n",
        self->aiName ? self->aiName : "UNKNOWN", meansOfDeath);
		svParams.waveKillCount++;
        AICast_CheckSurvivalProgression(&g_entities[0]);
        return;
}

if (!killerPlayer && !killerFriendly) {
   /* Com_Printf(
        "^3[AI_SURVIVE] WARNING: uncounted kill. attacker->client=%d, aiCharacter=%d, aiTeam=%d, meansOfDeath=%d\n",
        attacker->client != NULL,
        attacker->aiCharacter,
        attacker->aiTeam,
        meansOfDeath
    );*/

	svParams.waveKillCount++;
	AICast_CheckSurvivalProgression(&g_entities[0]);
	return;
}

	svParams.survivalKillCount++;
	svParams.waveKillCount++;

	if (killerPlayer) {
		Survival_AddKillScore(attacker, self, meansOfDeath);
	}

	// Always use attacker to trigger progression check
	AICast_CheckSurvivalProgression(attacker);

}


/*
====================
AICast_SetRebirthTimeSurvival

Centralized function for calculating AI rebirth time in Survival mode.
Handles friendly AI, and special cases per AI character if needed.
====================
*/
void AICast_SetRebirthTimeSurvival(gentity_t *ent, cast_state_t *cs) {
	if (!ent || !cs)
		return;

	// Skip characters that should never respawn
	if (ent->aiCharacter == AICHAR_ZOMBIE ||
		cs->norespawn)
		return;

	if (ent->aiTeam == AITEAM_ALLIES) { // Friendly AI
		if (cs->respawnsleft > 0) {
			cs->respawnsleft--;
		}
		svParams.spawnedThisWaveFriendly--;
		cs->rebirthTime = level.time + (FRIENDLY_SPAWN_TIME * 1000) + rand() % 2000;
		return;
	}

	// Non-friendly AI
	if (cs->respawnsleft != 0) {
		if (cs->respawnsleft > 0) {
			cs->respawnsleft--;
		}

		int baseTime;

		switch (ent->aiCharacter) {
			case AICHAR_ELITEGUARD:
				baseTime = 5 * 1000;
				break;
			case AICHAR_HELGA:
			    baseTime = 350 * 1000;
				break;
			case AICHAR_HEINRICH:
			    baseTime = 500 * 1000;
				break;
			case AICHAR_BLACKGUARD:
				baseTime = 5 * 1000;
				break;
			case AICHAR_VENOM:
				baseTime = 60 * 1000;
				break;
			case AICHAR_PROTOSOLDIER:
				baseTime = 120 * 1000;
				break;
			case AICHAR_WARZOMBIE:
				baseTime = 5 * 1000;
				break;
			case AICHAR_LOPER:
				baseTime = 5 * 1000;
				break;
			default: // Regular soldiers and zombies
				baseTime = 5 * 1000;
				break;
		}

		baseTime += rand() % 2000; // slight randomness

		cs->rebirthTime = level.time + baseTime;
	}
}

/*
============
AICast_Die_Survival
============
*/
void AICast_Die_Survival( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	int contents;
	int killer = 0;
	cast_state_t    *cs;
	qboolean nogib = qtrue;

	qboolean killerPlayer	 = attacker && attacker->client && !( attacker->aiCharacter );
	
	
	  if (killerPlayer && attacker->client->ps.powerups[PW_VAMPIRE]) {

			trap_SendServerCommand( -1, "mu_play sound/Zombie/firstsight/firstsight3.wav 0\n" );
		    attacker->health += 25;
		
			if ( attacker->health > 300 ) 
			{
			attacker->health = 300;
		    }

	  }

	// print debugging message
	if ( aicast_debug.integer == 2 && attacker->s.number == 0 ) {
		G_Printf( "killed %s\n", self->aiName );
	}

    AICast_RegisterSurvivalKill(self, attacker, meansOfDeath);

	cs = AICast_GetCastState( self->s.number );

	if ( attacker ) {
		killer = attacker->s.number;
	} else {
		killer = ENTITYNUM_WORLD;
	}

	// record the sighting (FIXME: silent weapons shouldn't do this, but the AI should react in some way)
	if ( attacker && attacker->client ) {
		AICast_UpdateVisibility( self, attacker, qtrue, qtrue );
	}

	if ( self->aiCharacter == AICHAR_HEINRICH || self->aiCharacter == AICHAR_HELGA || self->aiCharacter == AICHAR_SUPERSOLDIER || self->aiCharacter == AICHAR_PROTOSOLDIER ) {
		if ( self->health <= GIB_HEALTH ) {
			self->health = -1;
		}
	}

	// process the event
	if ( self->client->ps.pm_type == PM_DEAD ) {
		// already dead
		if ( self->health < GIB_HEALTH ) {
			if ( self->aiCharacter == AICHAR_ZOMBIE || self->aiCharacter == AICHAR_ZOMBIE_SURV || self->aiCharacter == AICHAR_ZOMBIE_FLAME  ) {
				// RF, changed this so Zombies always gib now
				GibEntity( self, killer );
				nogib = qfalse;
				self->takedamage = qfalse;
				self->r.contents = 0;
				cs->secondDeadTime = 2;
				cs->rebirthTime = 0;
				cs->revivingTime = 0;
			} else {
				body_die( self, inflictor, attacker, damage, meansOfDeath );
				return;
			}
		}

	} else {    // this is our first death, so set everything up

		if ( level.intermissiontime ) {
			return;
		}

		self->client->ps.pm_type = PM_DEAD;

		self->enemy = attacker;

		// drop a weapon?
		// if client is in a nodrop area, don't drop anything
		contents = trap_PointContents( self->r.currentOrigin, -1 );
		if ( !( contents & CONTENTS_NODROP ) ) {
			TossClientWeapons( self );
			TossClientItems_Survival( self, attacker );
			TossClientPowerups( self, attacker );
		}

		// make sure the client doesn't forget about this entity until it's set to "dead" frame
		// otherwise it might replay it's death animation if it goes out and into client view
		self->r.svFlags |= SVF_BROADCAST;

		self->takedamage = qtrue;   // can still be gibbed

		self->s.weapon = WP_NONE;
		if ( cs->bs ) {
			cs->weaponNum = WP_NONE;
		}
		self->client->ps.weapon = WP_NONE;

		self->s.powerups = 0;
		self->r.contents = CONTENTS_CORPSE;

		self->s.angles[0] = 0;
		self->s.angles[1] = self->client->ps.viewangles[1];
		self->s.angles[2] = 0;

		VectorCopy( self->s.angles, self->client->ps.viewangles );

		self->s.loopSound = 0;

		self->r.maxs[2] = -8;
		self->client->ps.maxs[2] = self->r.maxs[2];

		// remove powerups
		memset( self->client->ps.powerups, 0, sizeof( self->client->ps.powerups ) );

		// never gib in a nodrop
		if ( self->health <= GIB_HEALTH ) {
			if ( self->aiCharacter == AICHAR_ZOMBIE || self->aiCharacter == AICHAR_ZOMBIE_SURV || self->aiCharacter == AICHAR_ZOMBIE_FLAME  ) {
				// RF, changed this so Zombies always gib now
				GibEntity( self, killer );
				nogib = qfalse;
			} else if ( !( contents & CONTENTS_NODROP ) ) {
				body_die( self, inflictor, attacker, damage, meansOfDeath );
				nogib = qfalse;
			}
		}

		// set enemy weapon
		BG_UpdateConditionValue(self->s.number, ANIM_COND_ENEMY_WEAPON, 0, qfalse);
		if (attacker && attacker->client)
		{
			BG_UpdateConditionValue(self->s.number, ANIM_COND_ENEMY_WEAPON, inflictor->s.weapon, qtrue);
		}
		else
		{
			BG_UpdateConditionValue(self->s.number, ANIM_COND_ENEMY_WEAPON, 0, qfalse);
		}

		// set enemy location
		BG_UpdateConditionValue(self->s.number, ANIM_COND_ENEMY_POSITION, 0, qfalse);
		if (infront(self, inflictor))
		{
			BG_UpdateConditionValue(self->s.number, ANIM_COND_ENEMY_POSITION, POSITION_INFRONT, qtrue);
		}
		else
		{
			BG_UpdateConditionValue(self->s.number, ANIM_COND_ENEMY_POSITION, POSITION_BEHIND, qtrue);
		}

		if (self->takedamage)
		{ // only play the anim if we haven't gibbed
			// play the animation
			BG_AnimScriptEvent(&self->client->ps, ANIM_ET_DEATH, qfalse, qtrue);
		}

		// set gib delay
		if (cs->aiCharacter == AICHAR_HEINRICH || cs->aiCharacter == AICHAR_HELGA)
		{
			cs->lastLoadTime = level.time + self->client->ps.torsoTimer - 200;
		}

		// set this flag so no other anims override us
		self->client->ps.eFlags |= EF_DEAD;
		self->s.eFlags |= EF_DEAD;

		cs->deadSinkStartTime = 0;
	}

	if ( nogib ) {
		// the body can still be gibbed
		self->die = body_die;
	}

	nogib = qtrue;

    AICast_SetRebirthTimeSurvival(self, cs);

	trap_LinkEntity( self );

	// kill, instanly, any streaming sound the character had going
	G_AddEvent( &g_entities[self->s.number], EV_STOPSTREAMINGSOUND, 0 );

	// mark the time of death
	cs->deathTime = level.time;

	// really dead now, so call the script
	if (self->aiCharacter != AICHAR_ZOMBIE && self->aiCharacter != AICHAR_HELGA && self->aiCharacter != AICHAR_HEINRICH && nogib && !cs->norespawn)
	{

		if (!cs->died)
		{
			G_UseTargets(self, self); // testing
			AICast_ScriptEvent(cs, "death", "");
			cs->died = qtrue;
		}
	}
	else
	{
		AICast_ScriptEvent(cs, "fakedeath", "");
	}
	// call the deathfunc for this cast, so we can play associated sounds, or do any character-specific things
	if (!(cs->aiFlags & AIFL_DENYACTION) && cs->deathfunc)
	{
		cs->deathfunc(self, attacker, damage, meansOfDeath);
	}
}

/*
============
AICast_UpdateMaxActiveAI
  Updates the maximum number of active AI characters based on the current wave count.
  This function is called at the start of each wave to adjust the limits for each character type.
  The limits are defined in svParams and are adjusted according to the wave count.
  The function ensures that the number of active AI characters does not exceed the defined maximums.
============
*/
void AICast_UpdateMaxActiveAI(void)
{
    // Normal soldiers
    svParams.maxActiveAI[AICHAR_SOLDIER] += SOLDIERS_INCREASE;
    if (svParams.maxActiveAI[AICHAR_SOLDIER] > MAX_SOLDIERS_SURVIVAL) {
        svParams.maxActiveAI[AICHAR_SOLDIER] = MAX_SOLDIERS_SURVIVAL;
    }

    // Elite Guards
    if (svParams.waveCount >= WAVE_EG) {
        svParams.maxActiveAI[AICHAR_ELITEGUARD] += ELITES_INCREASE;
        if (svParams.maxActiveAI[AICHAR_ELITEGUARD] > MAX_ELITES_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_ELITEGUARD] = MAX_ELITES_SURVIVAL;
        }

	}

	if (svParams.waveCount >= WAVE_HELGA)
	{
		svParams.maxActiveAI[AICHAR_HELGA] += HELGAS_INCREASE;
		if (svParams.maxActiveAI[AICHAR_HELGA] > MAX_HELGAS_SURVIVAL)
		{
			svParams.maxActiveAI[AICHAR_HELGA] = MAX_HELGAS_SURVIVAL;
		}
	}

	if (svParams.waveCount >= WAVE_HEINRICH)
	{
		svParams.maxActiveAI[AICHAR_HEINRICH] += HEINRICHS_INCREASE;
		if (svParams.maxActiveAI[AICHAR_HEINRICH] > MAX_HEINRICHS_SURVIVAL)
		{
			svParams.maxActiveAI[AICHAR_HEINRICH] = MAX_HEINRICHS_SURVIVAL;
		}
	}

	// Black Guards
    if (svParams.waveCount >= WAVE_BG) {
        svParams.maxActiveAI[AICHAR_BLACKGUARD] += BLACKGUARDS_INCREASE;
        if (svParams.maxActiveAI[AICHAR_BLACKGUARD] > MAX_BLACKGUARDS_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_BLACKGUARD] = MAX_BLACKGUARDS_SURVIVAL;
        }
    }

    // Venoms
    if (svParams.waveCount >= WAVE_VENOM) {
        svParams.maxActiveAI[AICHAR_VENOM] += VENOMS_INCREASE;
        if (svParams.maxActiveAI[AICHAR_VENOM] > MAX_VENOMS_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_VENOM] = MAX_VENOMS_SURVIVAL;
        }
    }

    // Default Zombies
    svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] += ZOMBIES_INCREASE;
    if (svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] > MAX_ZOMBIES_SURVIVAL) {
        svParams.maxActiveAI[AICHAR_ZOMBIE_SURV] = MAX_ZOMBIES_SURVIVAL;
    }

    // Flamer Zombies
    if (svParams.waveCount >= WAVE_FLAMERS) {
        svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] += FLAMERS_INCREASE;
        if (svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] > MAX_FLAMERS_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_ZOMBIE_FLAME] = MAX_FLAMERS_SURVIVAL;
        }
    }


    // Warriors
    if (svParams.waveCount >= WAVE_WARRIORS) {
        svParams.maxActiveAI[AICHAR_WARZOMBIE] += WARRIORS_INCREASE;
        if (svParams.maxActiveAI[AICHAR_WARZOMBIE] > MAX_WARRIORS_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_WARZOMBIE] = MAX_WARRIORS_SURVIVAL;
        }
    }

    // Protos
    if (svParams.waveCount >= WAVE_PROTO) {
        svParams.maxActiveAI[AICHAR_PROTOSOLDIER] += PROTOS_INCREASE;
        if (svParams.maxActiveAI[AICHAR_PROTOSOLDIER] > MAX_PROTOS_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_PROTOSOLDIER] = MAX_PROTOS_SURVIVAL;
        }
    }

	if (svParams.waveCount >= WAVE_LOPERS) {
        svParams.maxActiveAI[AICHAR_LOPER] += LOPERS_INCREASE;
        if (svParams.maxActiveAI[AICHAR_LOPER] > MAX_LOPERS_SURVIVAL) {
            svParams.maxActiveAI[AICHAR_LOPER] = MAX_LOPERS_SURVIVAL;
        }
    }
}

/*
============
AICast_ApplySurvivalAttributes
  Applies survival mode attributes to the AI character based on the current wave count.
  The function adjusts health, speed scales, and other attributes based on the character type and wave count.
  This function is called when a new wave starts or when an AI character is spawned.
  The adjustments are made to ensure that the AI characters become progressively stronger as the game progresses.
============
*/
void AICast_ApplySurvivalAttributes(gentity_t *ent, cast_state_t *cs)
{
	int waveAppeared = 0;
	switch (cs->aiCharacter)
	{
	case AICHAR_ELITEGUARD:
		waveAppeared = WAVE_EG;
		break;
	case AICHAR_HEINRICH:
		waveAppeared = WAVE_HEINRICH;
		break;
	case AICHAR_HELGA:
		waveAppeared = WAVE_HELGA;
		break;
	case AICHAR_BLACKGUARD:
		waveAppeared = WAVE_BG;
		break;
	case AICHAR_VENOM:
		waveAppeared = WAVE_VENOM;
		break;
	case AICHAR_WARZOMBIE:
		waveAppeared = WAVE_WARRIORS;
		break;
	case AICHAR_ZOMBIE_FLAME:
		waveAppeared = WAVE_FLAMERS;
		break;
	case AICHAR_PROTOSOLDIER:
		waveAppeared = WAVE_PROTO;
		break;
	case AICHAR_LOPER:
		waveAppeared = WAVE_LOPERS;
		break;
	case AICHAR_SOLDIER:
	case AICHAR_ZOMBIE_SURV:
	    waveAppeared = 1;
	default:
		waveAppeared = 0;
		break;
	}

	int rawSteps = (svParams.waveCount > 1) ? (svParams.waveCount - waveAppeared) : 0;
	if (rawSteps < 0)
		rawSteps = 0;

	if (g_survivalDifficulty.integer == 1)
	{
		if (svParams.waveCount < 10)
		{
			rawSteps += 2 + rawSteps / 2;
		}
		else
		{
			rawSteps += 4 + rawSteps / 3;
		}

	}
	

	int newHealth = 0;
	float runSpeedScale = 1.0f;
	float sprintSpeedScale = 1.0f;
	float crouchSpeedScale = 1.0f;

	switch (cs->aiCharacter) {
		case AICHAR_SOLDIER:
			if (svParams.waveCount < 10)
			{
				newHealth = 20 + rawSteps * 10;
			}
			else
			{
				float growth = powf(1.12f, (float)(rawSteps - 9));
				newHealth = (int)((20 + 9 * 10) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
				if (newHealth > 100) newHealth = 100;
			}
			break;

		case AICHAR_ELITEGUARD:
			if (svParams.waveCount < 10)
			{
				newHealth = 30 + rawSteps * 12;
			}
			else
			{
				float growth = powf(1.13f, (float)(rawSteps - 9));
				newHealth = (int)((30 + 9 * 12) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 150) newHealth = 150;
			}
			break;

		case AICHAR_HELGA:
			if (svParams.waveCount < 10)
			{
				newHealth = 4000 + rawSteps * 14;
			}
			else
			{
				float growth = powf(1.13f, (float)(rawSteps - 9));
				newHealth = (int)((50 + 9 * 14) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 20000) newHealth = 20000;
			}
			break;


		case AICHAR_HEINRICH:
			if (svParams.waveCount < 10)
			{
				newHealth = 8000 + rawSteps * 14;
			}
			else
			{
				float growth = powf(1.13f, (float)(rawSteps - 9));
				newHealth = (int)((50 + 9 * 14) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 40000) newHealth = 40000;
			}
			break;

		case AICHAR_BLACKGUARD:
			if (svParams.waveCount < 10)
			{
				newHealth = 80 + rawSteps * 16;
			}
			else
			{
				float growth = powf(1.14f, (float)(rawSteps - 9));
				newHealth = (int)((80 + 9 * 16) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 200) newHealth = 200;
			}
			break;

		case AICHAR_VENOM:
			if (svParams.waveCount < 10)
			{
				newHealth = 100 + rawSteps * 18;
			}
			else
			{
				float growth = powf(1.15f, (float)(rawSteps - 9));
				newHealth = (int)((100 + 9 * 18) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 500) newHealth = 500;
			}
			break;

		case AICHAR_ZOMBIE_SURV:
		case AICHAR_ZOMBIE_FLAME:
			if (svParams.waveCount < 10)
			{
				newHealth = 20 + rawSteps * 10;
			}
			else
			{
				float growth = powf(1.12f, (float)(rawSteps - 9));
				newHealth = (int)((20 + 9 * 10) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 200) newHealth = 200;
			}
			runSpeedScale    = fminf(0.8f + rawSteps * 0.03f, 1.2f);
			sprintSpeedScale = fminf(1.2f + rawSteps * 0.04f, 1.6f);
			crouchSpeedScale = fminf(0.25f + rawSteps * 0.02f, 0.5f);
			break;

		case AICHAR_WARZOMBIE:
			if (svParams.waveCount < 10)
			{
				newHealth = 50 + rawSteps * 14;
			}
			else
			{
				float growth = powf(1.14f, (float)(rawSteps - 9));
				newHealth = (int)((50 + 9 * 14) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 500) newHealth = 500;
			}
			runSpeedScale    = fminf(0.8f + rawSteps * 0.04f, 1.6f);
			sprintSpeedScale = fminf(1.2f + rawSteps * 0.05f, 2.0f);
			crouchSpeedScale = fminf(0.25f + rawSteps * 0.03f, 0.75f);
			break;

		case AICHAR_PROTOSOLDIER:
			if (svParams.waveCount < 10)
			{
				newHealth = 1000 + rawSteps * 25;
			}
			else
			{
				float growth = powf(1.18f, (float)(rawSteps - 9));
				newHealth = (int)((1000 + 9 * 25) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 2000) newHealth = 2000;
			}
			runSpeedScale    = fminf(0.8f + rawSteps * 0.04f, 1.6f);
			sprintSpeedScale = fminf(1.2f + rawSteps * 0.03f, 1.5f);
			crouchSpeedScale = fminf(0.25f + rawSteps * 0.03f, 0.75f);
			break;

		case AICHAR_PARTISAN:
			if (svParams.waveCount < 10)
			{
				newHealth = 500 + rawSteps * 20;
			}
			else
			{
				float growth = powf(1.16f, (float)(rawSteps - 9));
				newHealth = (int)((500 + 9 * 20) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 1000) newHealth = 1000;
			}
			break;

		case AICHAR_LOPER:
			if (svParams.waveCount < 10)
			{
				newHealth = 250 + rawSteps * 18;
			}
			else
			{
				float growth = powf(1.16f, (float)(rawSteps - 9));
				newHealth = (int)((250 + 9 * 18) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 500) newHealth = 500;
			}
			break;

		case AICHAR_LOPER_SPECIAL:
			if (svParams.waveCount < 10)
			{
				newHealth = 25 + rawSteps * 12;
			}
			else
			{
				float growth = powf(1.13f, (float)(rawSteps - 9));
				newHealth = (int)((25 + 9 * 12) * growth);
			}
			if (g_survivalAiHealthCap.integer == 1)
			{
			if (newHealth > 250) newHealth = 250;
			}
			break;

		default:
			break;
	}

	// Apply to entity
	ent->health = ent->client->ps.stats[STAT_HEALTH] = ent->client->ps.stats[STAT_MAX_HEALTH] = cs->attributes[STARTING_HEALTH] = newHealth;
	ent->client->ps.runSpeedScale    = runSpeedScale;
	ent->client->ps.sprintSpeedScale = sprintSpeedScale;
	ent->client->ps.crouchSpeedScale = crouchSpeedScale;
}

/*
============
BG_SetBehaviorForSurvival
  Sets the behavior attributes for AI characters in survival mode.
  The function adjusts the aim skill, accuracy, attack skill, aggression, and reaction time
  based on the current wave count. The values are capped to ensure they do not exceed defined limits.
  This function is called at the start of each wave to adjust the AI behavior dynamically.
============
*/
void BG_SetBehaviorForSurvival(AICharacters_t characterNum) {
	int waveAppeared = 1;
	switch (characterNum) {
		case AICHAR_ELITEGUARD:   waveAppeared = WAVE_EG; break;
		case AICHAR_BLACKGUARD:   waveAppeared = WAVE_BG; break;
		case AICHAR_VENOM:        waveAppeared = WAVE_VENOM; break;
		case AICHAR_PROTOSOLDIER: waveAppeared = WAVE_PROTO; break;
		case AICHAR_SOLDIER: waveAppeared = 1; break;
		default:  waveAppeared = 0; break;
	}

	int rawSteps = (svParams.waveCount > 1) ? (svParams.waveCount - waveAppeared) : 0;
	if (rawSteps < 0) rawSteps = 0;

	float delta = rawSteps * 0.05f;

	float aimSkill     = 0.0f;
	float aimAccuracy  = 0.0f;
	float attackSkill  = 0.0f;
	float aggression   = 0.0f;
	float reactionTime = 1.0f;

	switch (characterNum) {
		case AICHAR_SOLDIER:
			if (g_survivalDifficulty.integer == 1) {
				aimSkill     = fminf(0.4f + delta, 0.8f);
				aimAccuracy  = fminf(0.4f + delta, 0.8f);
				attackSkill  = fminf(0.4f + delta, 0.8f);
				aggression   = fminf(0.4f + delta, 1.0f);
				reactionTime = fmaxf(0.8f - delta, 0.4f);
			} else {
				aimSkill     = fminf(0.1f + delta, 0.7f);
				aimAccuracy  = fminf(0.1f + delta, 0.7f);
				attackSkill  = fminf(0.1f + delta, 0.7f);
				aggression   = fminf(0.1f + delta, 1.0f);
				reactionTime = fmaxf(1.0f - delta, 0.4f);
			}
			break;
		case AICHAR_ELITEGUARD:
			if (g_survivalDifficulty.integer == 1) {
				aimSkill     = fminf(0.5f + delta, 0.9f);
				aimAccuracy  = fminf(0.5f + delta, 0.9f);
				attackSkill  = fminf(0.5f + delta, 0.9f);
				aggression   = fminf(0.5f + delta, 1.0f);
				reactionTime = fmaxf(0.6f - delta, 0.2f);
			} else {
				aimSkill     = fminf(0.4f + delta, 0.8f);
				aimAccuracy  = fminf(0.4f + delta, 0.8f);
				attackSkill  = fminf(0.4f + delta, 0.8f);
				aggression   = fminf(0.4f + delta, 1.0f);
				reactionTime = fmaxf(1.0f - delta, 0.3f);
			}
			break;
		case AICHAR_BLACKGUARD:
			if (g_survivalDifficulty.integer == 1) {
				aimSkill     = fminf(0.55f + delta, 0.9f);
				aimAccuracy  = fminf(0.55f + delta, 0.9f);
				attackSkill  = fminf(0.55f + delta, 0.9f);
				aggression   = fminf(0.55f + delta, 1.0f);
				reactionTime = fmaxf(0.7f - delta, 0.3f);
			} else {
				aimSkill     = fminf(0.4f + delta, 0.9f);
				aimAccuracy  = fminf(0.4f + delta, 0.9f);
				attackSkill  = fminf(0.4f + delta, 0.9f);
				aggression   = fminf(0.5f + delta, 1.0f);
				reactionTime = fmaxf(1.0f - delta, 0.3f);
			}
			break;
		case AICHAR_VENOM:
			if (g_survivalDifficulty.integer == 1) {
				aimSkill     = fminf(0.5f + delta, 0.8f);
				aimAccuracy  = fminf(0.5f + delta, 0.8f);
				attackSkill  = fminf(0.5f + delta, 0.8f);
				aggression   = fminf(0.6f + delta, 1.0f);
				reactionTime = fmaxf(0.7f - delta, 0.3f);
			} else {
				aimSkill     = fminf(0.4f + delta, 0.8f);
				aimAccuracy  = fminf(0.4f + delta, 0.8f);
				attackSkill  = fminf(0.4f + delta, 0.8f);
				aggression   = fminf(0.5f + delta, 1.0f);
				reactionTime = fmaxf(1.0f - delta, 0.3f);
			}
			break;
		case AICHAR_PROTOSOLDIER:
			if (g_survivalDifficulty.integer == 1) {
				aimSkill     = fminf(0.6f + delta, 0.9f);
				aimAccuracy  = fminf(0.6f + delta, 0.9f);
				attackSkill  = fminf(0.6f + delta, 0.9f);
				aggression   = fminf(0.6f + delta, 1.0f);
				reactionTime = fmaxf(0.8f - delta, 0.2f);
			} else {
				aimSkill     = fminf(0.4f + delta, 0.9f);
				aimAccuracy  = fminf(0.4f + delta, 0.9f);
				attackSkill  = fminf(0.4f + delta, 0.9f);
				aggression   = fminf(0.5f + delta, 1.0f);
				reactionTime = fmaxf(1.0f - delta, 0.2f);
			}
			break;
		case AICHAR_PARTISAN:
			aimSkill     = 0.9f;
			aimAccuracy  = 0.9f;
			attackSkill  = 0.9f;
			aggression   = 0.9f;
			reactionTime = 0.1f;
			break;
		case AICHAR_ZOMBIE_SURV:
		case AICHAR_ZOMBIE_FLAME:
		case AICHAR_WARZOMBIE:
		case AICHAR_LOPER:
		case AICHAR_LOPER_SPECIAL:
		case AICHAR_HELGA:
		case AICHAR_HEINRICH:
			aimSkill     = 1.0f;
			aimAccuracy  = 1.0f;
			attackSkill  = 1.0f;
			aggression   = 1.0f;
			reactionTime = 0.5f;
			break;
		default:
			return;
	}

	aiDefaults[characterNum].attributes[AIM_SKILL]     = aimSkill;
	aiDefaults[characterNum].attributes[AIM_ACCURACY]  = aimAccuracy;
	aiDefaults[characterNum].attributes[ATTACK_SKILL]  = attackSkill;
	aiDefaults[characterNum].attributes[AGGRESSION]    = aggression;
	aiDefaults[characterNum].attributes[REACTION_TIME] = reactionTime;
}

static void Survival_GameManagerEvent( const char *event ) {
	gentity_t *gm;
	char eventBuffer[MAX_QPATH];
	char emptyParams[] = "";

	gm = G_Find( NULL, FOFS( scriptName ), "game_manager" );
	if ( !gm ) {
		G_Printf( "Survival: ERROR no game_manager found for event '%s'\n", event );
		return;
	}

	Q_strncpyz( eventBuffer, event, sizeof( eventBuffer ) );

	G_Printf( "Survival: game_manager event -> %s\n", eventBuffer );

	G_Script_ScriptEvent( gm, eventBuffer, emptyParams );
}


static qboolean AICast_ShouldStartSpecialWave(void) {
    // 0 = disabled → never start special waves
    if (g_specialWaves.integer == 0 || SPECIAL_WAVE_CHANCE <= 0) 
        return qfalse;

    int wave = svParams.waveCount; // wave we’re starting now

    // Too early
    if (wave < SPECIAL_WAVE_MIN_START)
        return qfalse;

    // Enforce minimal gap after a special (cooldown)
    if (svParams.lastSpecialWave > 0) {
        int delta = wave - svParams.lastSpecialWave; // distance to previous special
        if (delta <= SPECIAL_WAVE_MIN_GAP)
            return qfalse; // still cooling down
    }

    // How many full NON-special waves since last special?
    // If none yet, count since first eligible (specialWaveMinStart).
    int gapSince = (svParams.lastSpecialWave > 0)
        ? (wave - svParams.lastSpecialWave - 1)
        : (wave - SPECIAL_WAVE_MIN_START);

    // Hard guarantee: if we waited long enough, force a special
    if (gapSince >= SPECIAL_WAVE_MAX_GAP)
        return qtrue;

    // Otherwise randomized
    int roll = rand() % 100; // 0..99
    return (roll < SPECIAL_WAVE_CHANCE) ? qtrue : qfalse;
}

void AICast_CheckSurvivalProgression( gentity_t *attacker ) {
	int enemiesAlive;
	int i;

	if ( g_gametype.integer != GT_COOP_SURVIVAL ) {
		return;
	}

	if ( !svParams.waveInProgress ) {
		return;
	}

	if ( svParams.wavePending ) {
		return;
	}

	if ( svParams.waveKillCount < svParams.killCountRequirement ) {
		enemiesAlive = 0;

		for ( i = 0; i < level.num_entities; i++ ) {
			gentity_t *ent = &g_entities[i];

			if ( !ent->inuse || !ent->client ) {
				continue;
			}

			if ( ent->aiTeam == AITEAM_ALLIES ) {
				continue;
			}

			if ( ent->client->ps.eFlags & EF_DEAD ) {
				continue;
			}

			// Optional, but safer if some non-survival AI exists on map
			if ( !( ent->r.svFlags & SVF_CASTAI ) ) {
				continue;
			}

			enemiesAlive++;
		}

		if ( enemiesAlive == 0 ) {
			G_Printf( "^1[AI_SURVIVE] ERROR: No enemies alive but wave not complete (%d/%d kills)\n",
				svParams.waveKillCount,
				svParams.killCountRequirement );

			svParams.waveKillCount = svParams.killCountRequirement;
		}
	}

	if ( svParams.waveKillCount >= svParams.killCountRequirement ) {
		const char *endEvent;

		G_Printf( "^2[AI_SURVIVE] Wave %d complete (%d/%d)^7\n",
			svParams.waveCount,
			svParams.waveKillCount,
			svParams.killCountRequirement );

		svParams.wavePending = qtrue;
		svParams.waveInProgress = qfalse;
		svParams.waveChangeTime = level.time + INTERMISSION_TIME * 1000;

		endEvent = svParams.specialWaveActive ? "specialwave_end" : "wave_end";

		Survival_GameManagerEvent( endEvent );
	}
}

void AICast_TickSurvivalWave( void ) {
	int i;
	int wave;
	int killReq;

	if ( !svParams.wavePending ) {
		return;
	}

	if ( level.time < svParams.waveChangeTime ) {
		return;
	}

	G_Printf( "Survival: starting wave %i\n", svParams.waveCount + 1 );

	svParams.wavePending = qfalse;
	svParams.waveInProgress = qtrue;
	svParams.waveCount++;
	svParams.waveKillCount = 0;
	svParams.spawnedThisWave = 0;

	// Some map-placed AI never go through AIChar_AIScript_AlertEntity_Survival
	for ( i = 0; i < level.num_entities; i++ ) {
		gentity_t *ent = &g_entities[i];

		if ( !ent->inuse || !ent->client || !( ent->r.svFlags & SVF_CASTAI ) ) {
			continue;
		}
		if ( ent->aiTeam == AITEAM_ALLIES ) {
			continue;
		}
		if ( ent->aiInactive ) {
			continue;
		}
		if ( ent->client->ps.eFlags & EF_DEAD ) {
			continue;
		}

		svParams.spawnedThisWave++;
	}

	wave = svParams.waveCount;
	killReq = 0;

if ( wave == 1 ) {
	G_Printf( "Survival: wave 1 before start_survival script\n" );
	Survival_GameManagerEvent( "start_survival" );
	G_Printf( "Survival: wave 1 after start_survival script\n" );

	killReq = INITIAL_KILLCOUNT_REQ;
} else {
		killReq = (int)( 0.15f * wave * wave + 3.0f * wave + 10.0f );
	}

	svParams.specialWaveActive = qfalse;

	if ( AICast_ShouldStartSpecialWave() ) {
		int lopers;

		svParams.specialWaveActive = qtrue;
		svParams.lastSpecialWave = wave;

		lopers = SPECIAL_WAVE_LOPERS_INITIAL +
			SPECIAL_WAVE_LOPERS_INCREASE * ( wave - SPECIAL_WAVE_MIN_START );

		if ( lopers < SPECIAL_WAVE_LOPERS_INITIAL ) {
			lopers = SPECIAL_WAVE_LOPERS_INITIAL;
		}

		svParams.killCountRequirement = lopers;

		svParams.maxActiveAI[AICHAR_LOPER_SPECIAL] =
			( lopers < SPECIAL_WAVE_LOPERS_MAX ) ? lopers : SPECIAL_WAVE_LOPERS_MAX;
	} else {
		svParams.killCountRequirement = killReq;

		if ( wave > 1 ) {
			AICast_UpdateMaxActiveAI();
		}
	}

	for ( i = 0; i < g_maxclients.integer; i++ ) {
		gentity_t *cl = &g_entities[i];

		if ( !cl->inuse || !cl->client ) {
			continue;
		}

		if ( cl->client->pers.connected != CON_CONNECTED ) {
			continue;
		}

		cl->client->ps.persistant[PERS_WAVES]++;
	}

	if ( svParams.specialWaveActive ) {
		Survival_GameManagerEvent( "specialwave_start" );
	} else {
		Survival_GameManagerEvent( "wave_start" );
	}
}


/*
============
AICast_SurvivalRespawn
============
*/
void AICast_SurvivalRespawn(gentity_t *ent, cast_state_t *cs) {

   vec3_t mins, maxs;
   int touch[10], numTouch;
   float oldmaxZ;
   int i;
   gentity_t *player;
   vec3_t spawn_origin, spawn_angles;

   if (ent->aiTeam != 1 && (svParams.spawnedThisWave >= svParams.killCountRequirement || !svParams.waveInProgress))
   {
	   return;
   }

	// Prevent friendly AI from respawning if respawnsleft is 0
    if (ent->aiTeam == 1 && cs->respawnsleft == 0) {
        return;
    }

	// Same block for respawns
	if (svParams.specialWaveActive && ent->aiTeam != 1 && ent->aiCharacter != AICHAR_LOPER_SPECIAL)
	{
		return;
	}

	if (!svParams.specialWaveActive && ent->aiTeam != 1 && ent->aiCharacter == AICHAR_LOPER_SPECIAL)
	{
		return;
	}

			if ( ent->aiCharacter != AICHAR_ZOMBIE && ent->aiCharacter != AICHAR_HELGA
				 && ent->aiCharacter != AICHAR_HEINRICH ) {

				for ( i = 0 ; i < g_maxclients.integer ; i++ ) {
					player = &g_entities[i];

					if ( !player || !player->inuse ) {
						continue;
					}

					if ( player->r.svFlags & SVF_CASTAI ) {
						continue;
					}
				}
			}

			oldmaxZ = ent->r.maxs[2];

			// make sure the area is clear
			AIChar_SetBBox( ent, cs, qfalse );

			VectorAdd( ent->r.currentOrigin, ent->r.mins, mins );
			VectorAdd( ent->r.currentOrigin, ent->r.maxs, maxs );
			trap_UnlinkEntity( ent );

			numTouch = trap_EntitiesInBox( mins, maxs, touch, 10 );

			if ( numTouch ) {
				for ( i = 0; i < numTouch; i++ ) {
					if ( g_entities[touch[i]].r.contents & MASK_PLAYERSOLID ) {
						break;
					}
				}
				if ( i == numTouch ) {
					numTouch = 0;
				}
			}

			if ( numTouch == 0 ) {    // ok to spawn

				BG_SetBehaviorForSurvival(ent->aiCharacter);
				AICast_ApplySurvivalAttributes(ent, cs);
				ent->r.contents = CONTENTS_BODY;
				ent->clipmask = MASK_PLAYERSOLID | CONTENTS_MONSTERCLIP;
				ent->takedamage = qtrue;
				ent->waterlevel = 0;
				ent->watertype = 0;
				ent->flags = 0;
				ent->s.dmgFlags = 0;
				ent->die = AICast_Die;
				ent->client->ps.eFlags &= ~EF_DEAD;
				ent->s.eFlags &= ~EF_DEAD;
				player = AICast_FindEntityForName( "player" );

                // Selecting the spawn point for the AI
				SelectSpawnPoint_AI( player, ent, spawn_origin, spawn_angles );
				G_SetOrigin( ent, spawn_origin );
				VectorCopy( spawn_origin, ent->client->ps.origin );
				SetClientViewAngle( ent, spawn_angles );

		        // Refresh now so script actions that read our position this frame see where we actually are
				VectorCopy( ent->client->ps.origin, cs->bs->origin );
				VectorCopy( ent->client->ps.origin, cs->bs->eye );
				cs->bs->eye[2] += ent->client->ps.viewheight;
				cs->bs->areanum = BotPointAreaNum( cs->bs->origin );
				if ( cs->bs->areanum ) {
					cs->lastValidAreaNum[cs->aasWorldIndex] = cs->bs->areanum;
					cs->lastValidAreaTime[cs->aasWorldIndex] = level.time;
				}

				// Activate respawn scripts for AI
				AICast_ScriptEvent(cs, "respawn", "");

				// Clear leftover vanilla/script movement state after respawn script.
				cs->castScriptStatus.scriptGotoId = -1;
				cs->castScriptStatus.scriptGotoEnt = -1;
				cs->followEntity = -1;
				cs->enemyNum = -1;
				cs->lastEnemy = -1;
				cs->combatGoalTime = 0;
				cs->battleHuntPauseTime = 0;
				cs->battleHuntViewTime = 0;
				cs->takeCoverTime = 0;
				cs->obstructingTime = 0;
				cs->grenadeFlushFiring = qfalse;
				cs->grenadeFlushEndTime = -1;
				cs->lockViewAnglesTime = -1;

				// Turn off Headshot flag and reattach hat
				ent->client->ps.eFlags &= ~EF_HEADSHOT;
				G_AddEvent( ent, EV_REATTACH_HAT, 0 );

				cs->rebirthTime = 0;
				cs->deathTime = 0;
				cs->died = qfalse;
				cs->registeredSurvivalKill = qfalse;

				ent->client->ps.eFlags &= ~EF_DEATH_FRAME;
				ent->client->ps.eFlags &= ~EF_FORCE_END_FRAME;
				ent->client->ps.eFlags |= EF_NO_TURN_ANIM;

				// play the revive animation
				cs->revivingTime = level.time + BG_AnimScriptEvent( &ent->client->ps, ANIM_ET_REVIVE, qfalse, qtrue );

				if (ent->aiTeam == 1)
				{
					AICast_StateChange(cs, AISTATE_RELAXED);
					AIFunc_IdleStart(cs);

					svParams.spawnedThisWaveFriendly++;
				}
				else
				{
					VectorCopy(player->r.currentOrigin, cs->survivalAwarenessPos);
					cs->survivalAwarenessEnt = player->s.number;
					cs->survivalAwarenessExpireTime = level.time + 30000;

					AIFunc_SurvivalHuntStart(cs);

					svParams.spawnedThisWave++;
				}
			} else {
				// can't spawn yet, so set bbox back, and wait
				ent->r.maxs[2] = oldmaxZ;
				ent->client->ps.maxs[2] = ent->r.maxs[2];
			}
			trap_LinkEntity( ent );


}
