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
 * name:		ai_cast_funcs.c
 *
 * desc:		Wolfenstein AI Character Decision Making
 *
*/


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

/*
This file contains the generic thinking states for the characters.

Different types of movement or behaviour will be represented by
a seperate thinking function, which may or may not pass control
over to a new behaviour function.

If control is passed onto a new function, the string name of the
current function is returned, mostly for debugging purposes.

!!! NOTE: control must not be passed to a new AI func from outside of
this file. A new AI func must only be called from within another AI func.

This gives us the ability to keep all code related to sections of AI
self-contained, so adding new features to the AI will be less likely to
step on other areas of AI.
*/

static int enemies[MAX_CLIENTS], numEnemies;

// this is used to prevent try/abort/try/abort/etc grenade flush behaviour
static int lastGrenadeFlush = 0;

#define AICAST_LEADERDIST_MAX   240     // try and stay at least this close to them when nothing else to do
#define AICAST_LEADERDIST_MIN   64      // get this close if we have a clear line of sight to them

char *AIFunc_BattleChase( cast_state_t *cs );
char *AIFunc_Battle( cast_state_t *cs );

static bot_moveresult_t *moveresult;

/*
============
AIFunc_Restore()

  restores the last aifunc that was backed up
============
*/
char *AIFunc_Restore( cast_state_t *cs ) {
	// if the old aifunc was BattleChase, set it back to Battle, in case we have found a good position
	if ( cs->oldAifunc == AIFunc_BattleChase ) {
		cs->oldAifunc = AIFunc_Battle;
	}
	cs->aifunc = cs->oldAifunc;
	return cs->aifunc( cs );
}

/*
============
AICast_GetRandomViewAngle()
============
*/
float AICast_GetRandomViewAngle( cast_state_t *cs, float tracedist ) {
	int cnt, passent, contents_mask;
	vec3_t vec, dir, start, end;
	trace_t trace;
	float bestdist, bestyaw;

	cnt = 0;
	VectorClear( vec );
	//
	VectorCopy( cs->bs->origin, start );
	start[2] += cs->bs->cur_ps.viewheight;
	//
	passent = cs->entityNum;
	contents_mask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_WATER | CONTENTS_SLIME;
//	contents_mask = CONTENTS_SOLID|CONTENTS_PLAYERCLIP|CONTENTS_WATER;
	bestdist = 0;
	bestyaw = 0;
	//
	while ( cnt++ < 4 )
	{
		vec[YAW] = random() * 360.0;
		//
		AngleVectors( vec, dir, NULL, NULL );
		VectorMA( start, tracedist, dir, end );
		//
		trap_Trace( &trace, start, NULL, NULL, end, passent, contents_mask );
		//
		if ( trace.fraction >= 1 ) {
			return vec[YAW];
		} else if ( !bestdist || bestdist < trace.fraction ) {
			bestdist = trace.fraction;
			bestyaw = vec[YAW];
		}
	}
	//
	if ( bestdist ) {
		return bestyaw;
	}
	// just return their current direction
	return cs->ideal_viewangles[YAW];
}

/*
============
AICast_MoveToPos()

  returns a pointer to the moveresult it used to make the move, so we can investigate it
  outside of this function
============
*/
bot_moveresult_t *AICast_MoveToPos( cast_state_t *cs, vec3_t pos, int entnum ) {
	bot_goal_t goal;
	vec3_t /*target,*/ dir;
	static bot_moveresult_t lmoveresult;
	int tfl;
	bot_state_t *bs;
	float dist;

//int pretime = Sys_MilliSeconds();

	moveresult = NULL;

	if ( cs->castScriptStatus.scriptNoMoveTime > level.time ) {
		return NULL;
	}
	if ( cs->pauseTime > level.time ) {
		return NULL;
	}
	//
	bs = cs->bs;
	tfl = cs->travelflags;
	//if in lava or slime the bot should be able to get out
	if ( BotInLava( bs ) ) {
		tfl |= TFL_LAVA;
	}
	if ( BotInSlime( bs ) ) {
		tfl |= TFL_SLIME;
	}
	//
	//create the chase goal
	memset( &goal, 0, sizeof( goal ) );
	goal.entitynum = entnum;
	if ( entnum >= 0 && entnum < level.maxclients && caststates[entnum].lastValidAreaTime[cs->aasWorldIndex] > level.time - 100 ) {
		goal.areanum = caststates[entnum].lastValidAreaNum[cs->aasWorldIndex];
	} else {
		goal.areanum = BotPointAreaNum( pos );
		if ( entnum >= 0 && entnum < level.maxclients ) {
			if ( !goal.areanum ) {
				// use the last valid area
				goal.areanum = caststates[entnum].lastValidAreaNum[cs->aasWorldIndex];
			}
		}
	}
	VectorCopy( pos, goal.origin );
	VectorSet( goal.mins, -8, -8, -8 );
	VectorSet( goal.maxs, 8, 8, 8 );
	if ( entnum > -1 && entnum == cs->followEntity && !cs->followSlowApproach ) {
		goal.flags |= GFL_NOSLOWAPPROACH;   // just speed right passed it
	}
	//
	// debugging, show the route
	if ( aicast_debug.integer == 2 && ( g_entities[cs->entityNum].aiName && !strcmp( aicast_debugname.string, g_entities[cs->entityNum].aiName ) ) ) {
		trap_AAS_RT_ShowRoute( cs->bs->origin, cs->bs->areanum, goal.areanum );
	}
	//
	//initialize the movement state
	BotSetupForMovement( bs );
	//if this is a slow moving creature, don't use avoidreach
	if ( cs->attributes[RUNNING_SPEED] < 100 ) {
		//reset the avoid reach, otherwise bot is stuck in current area
		trap_BotResetAvoidReach( bs->ms );
	} else if ( !VectorCompare( cs->lastMoveToPosGoalOrg, pos ) ) {
		//reset the avoid reach, otherwise bot is stuck in current area
		trap_BotResetAvoidReach( bs->ms );
		VectorCopy( pos, cs->lastMoveToPosGoalOrg );
	}
	//move towards the goal
	if ( !( cs->aiFlags & AIFL_EXPLICIT_ROUTING ) || ( entnum < 0 ) || Q_strcasecmp( g_entities[entnum].classname, "ai_marker" ) ) {
		// use AAS routing
		trap_BotMoveToGoal( &lmoveresult, bs->ms, &goal, tfl );
		//if the movement failed
		if ( lmoveresult.failure ) {

			//reset the avoid reach, otherwise bot is stuck in current area
			trap_BotResetAvoidReach( bs->ms );
			//BotAI_Print(PRT_MESSAGE, "movement failure %d\n", lmoveresult.traveltype);
			// clear all movement
			trap_EA_Move( cs->entityNum, vec3_origin, 0 );

		} else {

			if ( entnum > 0 && goal.areanum && entnum >= 0 && entnum < level.maxclients ) {   // NOTE: dont do this for the player
				// save this destination point
				caststates[entnum].lastValidAreaNum[cs->aasWorldIndex] = goal.areanum;
				caststates[entnum].lastValidAreaTime[cs->aasWorldIndex] = level.time;
			}

			if ( lmoveresult.flags & ( MOVERESULT_MOVEMENTVIEW | MOVERESULT_SWIMVIEW ) ) {
				VectorCopy( lmoveresult.ideal_viewangles, cs->ideal_viewangles );
				VectorCopy( cs->ideal_viewangles, cs->viewlock_viewangles );
				cs->aiFlags |= AIFL_VIEWLOCKED;
			} else if ( !( cs->bFlags & BFL_ATTACKED ) )       { // if we are attacking, don't change angles
				bot_input_t bi;

				trap_EA_GetInput( bs->client, 0.1, &bi );
				if ( VectorLength( lmoveresult.movedir ) < 0.5 ) {
					VectorSubtract( goal.origin, bs->origin, dir );
					vectoangles( dir, cs->ideal_viewangles );
				} else {
					// use our velocity if we are moving
					if ( VectorNormalize2( cs->bs->cur_ps.velocity, dir ) > 1 ) {
						vectoangles( dir, cs->ideal_viewangles );
					} else {
						vectoangles( lmoveresult.movedir, cs->ideal_viewangles );
					}
				}
				cs->ideal_viewangles[2] *= 0.5;
				// look towards our future direction (like looking around a corner as we approach it)
				if ( !( cs->aiFlags & AIFL_WALKFORWARD ) && ( lmoveresult.flags & MOVERESULT_FUTUREVIEW ) ) {
					if ( AngleDifference( cs->ideal_viewangles[1], lmoveresult.ideal_viewangles[1] ) > 45 ) {
						cs->ideal_viewangles[1] -= 45;
					} else if ( AngleDifference( cs->ideal_viewangles[1], lmoveresult.ideal_viewangles[1] ) < -45 ) {
						cs->ideal_viewangles[1] += 45;
					} else {
						cs->ideal_viewangles[1] = lmoveresult.ideal_viewangles[1];
					}
					cs->ideal_viewangles[1] = AngleNormalize360( cs->ideal_viewangles[1] );
					cs->ideal_viewangles[0] = lmoveresult.ideal_viewangles[0];
					cs->ideal_viewangles[0] = 0.5 * AngleNormalize180( cs->ideal_viewangles[0] );
				}
			}

		}
	} else {    // manual routing towards markers

		VectorSubtract( pos, cs->bs->origin, dir );
		if ( ( dist = VectorNormalize( dir ) ) < 64 ) {
			trap_EA_Move( cs->entityNum, dir, 100.0 + 300.0 * ( dist / 64.0 ) );
		} else {
			trap_EA_Move( cs->entityNum, dir, 400 );
		}

		// look towards the marker also
		vectoangles( dir, cs->ideal_viewangles );
		cs->ideal_viewangles[2] *= 0.5;

	}
	// this must go last so we face the direction we avoid move
	AICast_Blocked( cs, &lmoveresult, qfalse, &goal );

//	G_Printf("MoveToPos: %i ms\n", -pretime + Sys_MilliSeconds() );

/*
	// debug, print movement info
	if ( 0 ) {	// (SA) added to hide the print
		bot_input_t bi;

		trap_EA_GetInput( cs->bs->client, (float) level.time / 1000, &bi );
		G_Printf( "spd: %i\n", (int)bi.speed );
	}
*/

	return ( moveresult = &lmoveresult );
}

/*
============
AICast_SpeedScaleForDistance()
============
*/
float AICast_SpeedScaleForDistance( cast_state_t *cs, float startdist, float idealDist ) {
#define PREDICT_TIME_WALK   0.2
#define PREDICT_TIME_CROUCH 0.2
#define PREDICT_TIME_RUN    0.3
	float speed, dist;

	dist = startdist - idealDist;
	if ( dist < 1 ) {
		dist = 1;
	}

	// if walking
	if ( cs->movestate == MS_WALK ) {
		speed = cs->attributes[WALKING_SPEED];
		if ( speed * PREDICT_TIME_WALK > dist ) {
			return 0.2 + 0.8 * ( dist / ( speed * PREDICT_TIME_WALK ) );
		} else {
			return 1.0;
		}
	} else
	// if crouching
	if ( cs->movestate == MS_CROUCH || cs->attackcrouch_time > level.time ) {
		speed = cs->attributes[CROUCHING_SPEED];
		if ( speed * PREDICT_TIME_CROUCH > dist ) {
			return 0.3 + 0.7 * ( dist / ( speed * PREDICT_TIME_CROUCH ) );
		} else {
			return 1.0;
		}
	} else
	// running
	{
		speed = cs->attributes[RUNNING_SPEED];
		if ( speed * PREDICT_TIME_RUN > dist ) {
			return 0.2 + 0.8 * ( dist / ( speed * PREDICT_TIME_RUN ) );
		} else {
			return 1.0;
		}
	}
}

/*
============
AICast_SpecialFunc
============
*/
void AICast_SpecialFunc( cast_state_t *cs ) {
	gentity_t *ent = &g_entities[cs->entityNum];
	gentity_t *enemy = NULL;

	if ( cs->enemyNum >= 0 ) {
		enemy = &g_entities[cs->enemyNum];
	}

	switch ( cs->aiCharacter ) {
	case AICHAR_WARZOMBIE:
		// disable defense unless we want it
		ent->flags &= ~FL_DEFENSE_CROUCH;
		// if we are pursuing the player from a distance, use our "crouch moving defense"
		if (    ( enemy )
				&&  ( cs->vislist[cs->enemyNum].real_visible_timestamp > level.time - 5000 )
				&&  ( Distance( cs->bs->origin, enemy->s.pos.trBase ) > 200 )
				&&  ( Distance( cs->bs->origin, enemy->s.pos.trBase ) < 600 )
				&&  ( cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE )
				//&&	(infront( ent, enemy ))
				&&  ( infront( enemy, ent ) ) ) {
			// crouch
			trap_EA_Crouch( cs->entityNum );
			// enable defense pose
			ent->flags |= FL_DEFENSE_CROUCH;
		}
		break;
	case AICHAR_HELGA:
		// if she has recently finished a spirit attack, go into charge mode
		if ( ( cs->weaponFireTimes[WP_MONSTER_ATTACK2] && ( cs->weaponFireTimes[WP_MONSTER_ATTACK2] > level.time - 12000 ) ) ||
			 ( cs->weaponFireTimes[WP_MONSTER_ATTACK1] && ( cs->weaponFireTimes[WP_MONSTER_ATTACK1] > level.time - 6000 ) ) ) {
			BG_UpdateConditionValue( cs->entityNum, ANIM_COND_CHARGING, 1, qfalse );
			cs->actionFlags &= ~CASTACTION_WALK;
		} else {    // not charging
			BG_UpdateConditionValue( cs->entityNum, ANIM_COND_CHARGING, 0, qfalse );
		}
		//
		if ( ent->health <= 0 && ent->takedamage ) {
			if ( ent->client->ps.torsoTimer < 500 ) {
				// blow up
				GibEntity( ent, 0 );
				ent->takedamage = qfalse;
				ent->r.contents = 0;
				ent->health = GIB_HEALTH - 1;
			}
		}
		break;
	case AICHAR_HEINRICH:
		if (    ( ent->health <= 0.25 * cs->attributes[STARTING_HEALTH] )
				||  ( cs->weaponFireTimes[WP_MONSTER_ATTACK1] > level.time - 6000 ) // walk for period after attack
				||  ( cs->weaponFireTimes[WP_MONSTER_ATTACK1] % 8000 < 3000 ) ) {   // dont run constantly
			cs->actionFlags |= CASTACTION_WALK;
		} else {    // charging
			cs->actionFlags &= ~CASTACTION_WALK;
		}
		// allow running while attacking
		if ( ent->client->ps.torsoTimer && !ent->client->ps.legsTimer ) {
			cs->actionFlags &= ~CASTACTION_WALK;
		}
		//
		if ( ent->health <= 0 && ent->takedamage ) {
			if ( ent->client->ps.torsoTimer < 500 ) {
				// blow up
				GibEntity( ent, 0 );
				ent->takedamage = qfalse;
				ent->r.contents = 0;
				ent->health = GIB_HEALTH - 1;
			}
			// blow up other warriors left around
			if ( !ent->takedamage || ( ent->count2 < level.time && ent->client->ps.torsoTimer < 4000 ) ) {
				int i;
				gentity_t *trav;
				for ( i = 0, trav = g_entities; i < level.maxclients; i++, trav++ ) {
					if ( !trav->inuse ) {
						continue;
					}
					if ( trav->aiCharacter != AICHAR_WARZOMBIE ) {
						continue;
					}
					if ( trav->aiInactive ) {
						continue;
					}
					if ( trav->health <= 0 ) {
						continue;
					}
					// blow it up, set some delay
					G_Damage( trav, ent, ent, NULL, NULL, 99999, 0, MOD_CRUSH );
					if ( ent->takedamage ) {
						ent->count2 = level.time + 200 + rand() % 1500;
					}
				}
			}
		}
		break;
	case AICHAR_ZOMBIE:
	case AICHAR_ZOMBIE_SURV:
	case AICHAR_ZOMBIE_FLAME:
	case AICHAR_ZOMBIE_GHOST:
		if ( COM_BitCheck( cs->bs->cur_ps.weapons, WP_MONSTER_ATTACK1 ) ) { // flaming zombie, run
			BG_UpdateConditionValue( cs->entityNum, ANIM_COND_CHARGING, 1, qfalse );
		}
		break;
	}
}

/*
============
AIFunc_Idle

Default idle behavior for active AI.
Handles danger/door/defend overrides, leader following, enemy/event scanning,
post-combat alert behavior, obstruction movement, idle looking, and idle reloading.
============
*/
char *AIFunc_Idle( cast_state_t *cs ) {
	gentity_t *ent = &g_entities[cs->entityNum];
	qboolean hasLeader;
	qboolean targetIsDangerous;
	qboolean shouldReturnToLastEnemy;

	hasLeader = ( cs->leaderNum >= 0 );

	// Avoid active danger first.
	if ( cs->dangerEntityValidTime >= level.time ) {
		AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos );

		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;

		return AIFunc_AvoidDangerStart( cs );
	}

	// Handle door waiting / door marker movement.
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend override can fully handle idle behavior.
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	// Stay near leader if following one.
	if ( hasLeader && Distance( cs->bs->origin, g_entities[cs->leaderNum].r.currentOrigin ) > MAX_LEADER_DIST ) {
		return AIFunc_ChaseGoalStart( cs, cs->leaderNum, AICAST_LEADERDIST_MAX, qtrue );
	}

	// Look for enemies or suspicious events.
	numEnemies = AICast_ScanForEnemies( cs, enemies );

	if ( numEnemies == -1 ) { // query mode
		return NULL;
	}

	if ( numEnemies == -2 ) { // friendly inspection
		char *retval;

		if ( ( retval = AIFunc_InspectFriendlyStart( cs, enemies[0] ) ) ) {
			return retval;
		}
	} else if ( numEnemies == -3 ) { // bullet impact
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectBulletImpactStart( cs );
		}
	} else if ( numEnemies == -4 ) { // audible event
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
		}
	} else if ( numEnemies > 0 ) {
		int i;

		cs->enemyNum = -1;

		// Pick a valid enemy.
		for ( i = 0; i < numEnemies; i++ ) {
			if ( Distance( cs->bs->origin, cs->vislist[enemies[i]].visible_pos ) <= 16 ) {
				continue;
			}

			if ( cs->enemyNum < 0 ) {
				cs->enemyNum = enemies[i];
			} else if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
				cs->enemyNum = enemies[i];
				return AIFunc_BattleStart( cs );
			}
		}

		if ( cs->enemyNum >= 0 ) {
			targetIsDangerous =
				AICast_EntityVisible( AICast_GetCastState( cs->enemyNum ), cs->entityNum, qtrue ) ||
				AICast_CheckAttack( AICast_GetCastState( cs->enemyNum ), cs->entityNum, qfalse );

			if ( ( !hasLeader || cs->thinkFuncChangeTime < level.time - 3000 ) && AICast_WantsToChase( cs ) ) {
				return AIFunc_BattleStart( cs );
			}

			if ( targetIsDangerous ) {
				// Tactical non-followers may prefer cover instead of immediately attacking.
				if ( !hasLeader && cs->attributes[TACTICAL] > 0.4 && cs->attributes[AGGRESSION] < 1.0 ) {
					if ( AICast_GetTakeCoverPos( cs, cs->enemyNum, cs->vislist[cs->enemyNum].visible_pos, cs->takeCoverPos ) ) {
						cs->takeCoverTime = level.time + 2000 + rand() % 3000;
						return AIFunc_BattleTakeCoverStart( cs );
					}
				}

				return AIFunc_BattleStart( cs );
			}

			if ( !hasLeader ) {
				return AIFunc_BattleStart( cs );
			}

			// Enemy cannot see us and we are following a leader, so ignore for now.
			cs->lastEnemy = cs->enemyNum;
			cs->enemyNum = -1;

			if ( cs->attributes[ATTACK_CROUCH] > 0.5 ) {
				cs->attackcrouch_time = level.time + 1000;
			}
		}
	}

	// Relax from combat state if we no longer have an enemy.
	if ( cs->aiState >= AISTATE_COMBAT ) {
		AICast_StateChange( cs, AISTATE_ALERT );
	}

	// If we recently lost an enemy, go investigate again after some time.
	shouldReturnToLastEnemy =
		!hasLeader &&
		cs->lastEnemy >= 0 &&
		g_entities[cs->lastEnemy].health > 0 &&
		cs->vislist[cs->lastEnemy].real_visible_timestamp < level.time - 5000 &&
		cs->takeCoverTime < level.time - 5000;

	if ( shouldReturnToLastEnemy ) {
		cs->enemyNum = cs->lastEnemy;
		return AIFunc_BattleStart( cs );
	}

	// Recently fought enemies: keep looking alert instead of standing dumbly.
	if ( cs->lastEnemy >= 0 ) {
		if ( ent->aiTeam != AITEAM_ALLIES ) {
			vec3_t dir;

			VectorSubtract( g_entities[cs->lastEnemy].s.pos.trBase, cs->bs->origin, dir );

			if ( VectorLength( dir ) < 1 ) {
				cs->ideal_viewangles[PITCH] = 0;
			} else {
				VectorNormalize( dir );
				vectoangles( dir, cs->ideal_viewangles );
				cs->ideal_viewangles[PITCH] = AngleNormalize180( cs->ideal_viewangles[PITCH] ) * 0.5;
			}
		} else if ( cs->attributes[TACTICAL] && cs->nextIdleAngleChange < level.time ) {
			if ( ( cs->nextIdleAngleChange + 3000 ) < level.time ) {
				cs->idleYaw = AICast_GetRandomViewAngle( cs, 512 );

				if ( fabs( AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] ) ) < 45 ) {
					cs->nextIdleAngleChange = level.time + 1000 + rand() % 2500;
				} else {
					cs->nextIdleAngleChange = level.time + 500;
				}

				cs->idleYawChange = AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] );
				cs->ideal_viewangles[PITCH] = 0;
			}
		} else if ( cs->idleYawChange ) {
			cs->idleYawChange = AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] );
			cs->ideal_viewangles[YAW] = AngleMod( cs->ideal_viewangles[YAW] + ( cs->idleYawChange * cs->bs->thinktime ) );
		}
	}

	// Move away if currently obstructing something.
	if ( cs->obstructingTime > level.time ) {
		AICast_MoveToPos( cs, cs->obstructingPos, -1 );

		if ( cs->movestate != MS_CROUCH ) {
			cs->movestate = MS_WALK;
		}

		cs->movestateType = MSTYPE_TEMPORARY;
	}

	// Tactical idle AI can use head-looking when not focused on an enemy.
	if ( cs->enemyNum < 0 && cs->attributes[TACTICAL] >= 0.5 && !( cs->aiFlags & AIFL_NO_HEADLOOK ) ) {
		g_entities[cs->entityNum].client->ps.eFlags |= EF_HEADLOOK;
	}

	AICast_IdleReload( cs );

	return NULL;
}

/*
============
AIFunc_IdleStart

Transitions the AI into its default idle state.
Clears temporary combat/follow behavior, resets movement avoidance state,
optionally triggers post-combat body inspection, and restores idle animation flags.
============
*/
char *AIFunc_IdleStart( cast_state_t *cs ) {
	gentity_t *ent;
	gentity_t *enemy;

	ent = &g_entities[cs->entityNum];

	// Leaving active behavior, so clear temporary combat/action flags.
	ent->flags &= ~FL_AI_GRENADE_KICK;

	// Idle AI should not keep following a temporary target.
	cs->followEntity = -1;

	// If the current enemy just died, Nazi AI may inspect the body.
	if ( cs->enemyNum >= 0 ) {
		enemy = &g_entities[cs->enemyNum];

		if ( ent->aiTeam == AITEAM_NAZI && enemy->health <= 0 ) {
			return AIFunc_InspectBodyStart( cs );
		}

		cs->enemyNum = -1;
	}

	// Clear old movement avoidance state before returning to idle.
	trap_BotInitAvoidReach( cs->bs->ms );

	// Use alternate idle standing animation if this AI is configured for it.
	if ( cs->aiFlags & AIFL_STAND_IDLE2 ) {
		ent->client->ps.eFlags |= EF_STAND_IDLE2;
	}

	cs->aifunc = AIFunc_Idle;

	return "AIFunc_Idle";
}

/*
============
AIFunc_InspectFriendly

Handles movement and behavior while inspecting or following a friendly AI.
Manages approach/navigation, danger reactions, enemy interruption, scripted inspection events,
and idle observation behavior during the inspection state.
============
*/
char *AIFunc_InspectFriendly( cast_state_t *cs ) {
	gentity_t *ent;
	gentity_t *followEnt;
	bot_state_t *bs;
	vec3_t destorg;
	float dist;
	qboolean moved;
	qboolean followEntReady;

	ent = &g_entities[cs->entityNum];
	moved = qfalse;

	// Enemy takes priority over inspection.
	if ( cs->enemyNum >= 0 ) {
		return AIFunc_BattleStart( cs );
	}

	cs->followEntity = cs->inspectNum;
	cs->followDist = 64;

	// Pause scripted walking while inspecting.
	cs->scriptPauseTime = level.time + 4000;

	// Avoid active danger first.
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
			cs->attackcrouch_time = 0;
			cs->castScriptStatus.scriptGotoId = -1;
			cs->movestate = MS_DEFAULT;
			cs->movestateType = MSTYPE_NONE;

			return AIFunc_AvoidDangerStart( cs );
		}
	}

	// Door marker handling.
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend override.
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	followEnt = &g_entities[cs->followEntity];

	// Target is not spawned/ready yet.
	if ( !followEnt->inuse ) {
		followEntReady =
			cs->followEntity < MAX_CLIENTS &&
			(
				( followEnt->client && followEnt->client->pers.connected == CON_CONNECTING ) ||
				( level.time < 3000 )
			);

		if ( followEntReady ) {
			return AIFunc_ChaseGoalIdleStart( cs, cs->followEntity, cs->followDist );
		}

		AICast_EndChase( cs );
		return AIFunc_IdleStart( cs );
	}

	// Get the position we should move toward.
	if ( followEnt->client ) {
		VectorCopy( followEnt->client->ps.origin, destorg );
	} else {
		VectorCopy( followEnt->r.currentOrigin, destorg );
	}

	dist = Distance( destorg, cs->bs->origin );

	// Move toward inspected friendly until inside follow distance.
	if ( !( dist < cs->followDist && ( ent->waterlevel || cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE ) ) ) {
		bs = cs->bs;

		// Remember that we were moving toward the inspected entity.
		cs->aiFlags |= AIFL_MISCFLAG1;

		// Try direct movement first if grounded or swimming.
		if ( cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE || ent->waterlevel > 1 ) {
			aicast_predictmove_t move;
			vec3_t dir;
			bot_input_t bi;
			usercmd_t ucmd;
			trace_t tr;
			qboolean simTest;

			simTest = qfalse;

			if ( cs->attributes[RUNNING_SPEED] < 120 ) {
				simTest = qtrue;
			}

			if ( !simTest ) {
				trap_Trace(
					&tr,
					cs->bs->origin,
					NULL,
					NULL,
					followEnt->r.currentOrigin,
					cs->entityNum,
					ent->clipmask
				);

				if ( tr.entityNum == cs->followEntity || tr.fraction == 1 ) {
					simTest = qtrue;
				}
			}

			if ( simTest ) {
				VectorSubtract( followEnt->r.currentOrigin, cs->bs->origin, dir );
				VectorNormalize( dir );

				if ( !ent->waterlevel ) {
					dir[2] = 0;
				}

				trap_EA_GetInput( cs->entityNum, (float)level.time / 1000, &bi );

				VectorCopy( dir, bi.dir );
				bi.speed = 400;
				bi.actionflags = 0;

				AICast_InputToUserCommand( cs, &bi, &ucmd, bs->cur_ps.delta_angles );
				AICast_PredictMovement( cs, 10, 0.8, &move, &ucmd, cs->followEntity );

				if ( move.stopevent == PREDICTSTOP_HITENT ) {
					trap_EA_Move( cs->entityNum, dir, 400 );

					vectoangles( dir, cs->ideal_viewangles );
					cs->ideal_viewangles[2] *= 0.5;

					moved = qtrue;
				}
			}
		}

		// If direct movement failed, use AAS routing.
		if ( !moved ) {
			moveresult = AICast_MoveToPos( cs, followEnt->r.currentOrigin, cs->followEntity );

			// If pathing failed, face toward the first visible route point if possible.
			if ( !moveresult || moveresult->failure ) {
				if ( !( cs->aiFlags & AIFL_MISCFLAG2 ) ) {
					if ( trap_AAS_GetRouteFirstVisPos( followEnt->r.currentOrigin, cs->bs->origin, cs->travelflags, cs->takeCoverEnemyPos ) ) {
						cs->aiFlags |= AIFL_MISCFLAG2;
					} else {
						VectorCopy( followEnt->r.currentOrigin, cs->takeCoverEnemyPos );
					}
				}

				VectorSubtract( cs->takeCoverEnemyPos, cs->bs->origin, destorg );
				VectorNormalize( destorg );
				vectoangles( destorg, cs->ideal_viewangles );
			}
		}

		if ( cs->followDist && cs->followSlowApproach ) {
			cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, cs->followDist );
		}
	} else if ( cs->aiFlags & AIFL_MISCFLAG1 ) {
		// We just reached the inspected entity.
		cs->aiFlags &= ~AIFL_MISCFLAG1;

		if ( g_entities[cs->inspectNum].health <= 0 ) {
			cs->aiFlags &= ~AIFL_DENYACTION;

			AICast_ForceScriptEvent( cs, "inspectbodyend", g_entities[cs->inspectNum].aiName );

			if ( cs->aiFlags & AIFL_DENYACTION ) {
				return AIFunc_DefaultStart( cs );
			}
		} else {
			AICast_UpdateVisibility( ent, &g_entities[cs->inspectNum], qtrue, qtrue );
		}
	}

	// While inspecting, still react to new enemies.
	{
		int numEnemies;

		numEnemies = AICast_ScanForEnemies( cs, enemies );

		if ( numEnemies == -1 ) {
			return NULL;
		} else if ( numEnemies == -2 ) {
			if ( g_entities[cs->inspectNum].health <= 0 && g_entities[enemies[0]].health > 0 ) {
				return AIFunc_InspectFriendlyStart( cs, enemies[0] );
			}
		} else if ( numEnemies > 0 ) {
			int i;

			cs->enemyNum = enemies[0];

			for ( i = 1; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
					cs->enemyNum = enemies[i];
					break;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}

			return AIFunc_BattleStart( cs );
		}
	}

	// Idle looking behavior while inspecting.
	if ( cs->nextIdleAngleChange < level.time ) {
		if ( ( cs->nextIdleAngleChange + 3000 ) < level.time ) {
			cs->idleYaw = AICast_GetRandomViewAngle( cs, 512 );

			if ( fabs( AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] ) ) < 45 ) {
				cs->nextIdleAngleChange = level.time + 1000 + rand() % 2500;
			} else {
				cs->nextIdleAngleChange = level.time + 500;
			}

			cs->idleYawChange = AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] );
			cs->ideal_viewangles[PITCH] = 0;
		}
	} else if ( cs->idleYawChange ) {
		cs->idleYawChange = AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] );
		cs->ideal_viewangles[YAW] = AngleMod( cs->ideal_viewangles[YAW] + ( cs->idleYawChange * cs->bs->thinktime ) );
	}

	// Tactical AI can use head-look while not focused on an enemy.
	if ( cs->enemyNum < 0 && cs->attributes[TACTICAL] >= 0.5 && !( cs->aiFlags & AIFL_NO_HEADLOOK ) ) {
		ent->client->ps.eFlags |= EF_HEADLOOK;
	}

	AICast_IdleReload( cs );

	return NULL;
}

/*
============
AIFunc_InspectFriendlyStart

Initializes inspection behavior toward a friendly AI.
Handles combat/death reactions, script overrides, and optional defensive cover behavior
before transitioning into friendly inspection logic.
============
*/
char *AIFunc_InspectFriendlyStart( cast_state_t *cs, int entnum ) {
	cast_state_t *otherCS;
	gentity_t *inspectEnt;
	qboolean inspectEntDead;
	qboolean inspectEntInCombat;

	otherCS = AICast_GetCastState( entnum );
	inspectEnt = &g_entities[entnum];

	inspectEntDead = ( inspectEnt->health <= 0 );
	inspectEntInCombat = ( otherCS->aiState >= AISTATE_COMBAT );

	// We are now handling this inspection request.
	cs->vislist[entnum].flags &= ~AIVIS_INSPECT;

	// Pause scripted walking while deciding how to react.
	cs->scriptPauseTime = level.time + 4000;

	// Clear cached route-visible position flag.
	cs->aiFlags &= ~AIFL_MISCFLAG2;

	if ( inspectEntInCombat || inspectEntDead ) {
		cs->vislist[entnum].flags |= AIVIS_INSPECTED;
	}

	// Dead friendlies should be approached and inspected directly.
	if ( inspectEntDead ) {
		cs->inspectNum = entnum;
		cs->aifunc = AIFunc_InspectFriendly;

		return "AIFunc_InspectFriendlyStart";
	}

	// Alive friendlies can trigger scripting before the AI decides what to do.
	AICast_ForceScriptEvent( cs, "inspectfriendlycombatstart", inspectEnt->aiName );

	if ( cs->aiFlags & AIFL_DENYACTION ) {
		// Script denied the action, so ignore this friendly from now on.
		cs->vislist[entnum].flags |= AIVIS_INSPECTED;

		return NULL;
	}

	// Low-aggression AI may hide instead of running toward a friendly in combat.
	if ( inspectEntInCombat && cs->attributes[AGGRESSION] < 0.3 ) {
		if ( AICast_GetTakeCoverPos( cs, entnum, inspectEnt->client->ps.origin, cs->takeCoverPos ) ) {
			cs->takeCoverTime = level.time + 10000;
			cs->scriptPauseTime = cs->takeCoverTime;

			if ( cs->attributes[ATTACK_CROUCH] > 0.1 ) {
				cs->attackcrouch_time = level.time + 3000;
			}

			return AIFunc_BattleTakeCoverStart( cs );
		}
	}

	// Otherwise go to the friendly and inspect/follow them.
	cs->inspectNum = entnum;
	cs->aifunc = AIFunc_InspectFriendly;

	return "AIFunc_InspectFriendly";
}

/*
============
AIFunc_InspectBulletImpact()
============
*/
char *AIFunc_InspectBulletImpact( cast_state_t *cs ) {
	gentity_t *ent;
	vec3_t v1;
	//
	//
	ent = &g_entities[cs->entityNum];
	//
	cs->bulletImpactIgnoreTime = level.time + 800;
	//
	// do we need to avoid a danger?
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// go to a position that cannot be seen from the dangerPos
			cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
			cs->attackcrouch_time = 0;
			cs->castScriptStatus.scriptGotoId = -1;
			cs->movestate = MS_DEFAULT;
			cs->movestateType = MSTYPE_NONE;
			return AIFunc_AvoidDangerStart( cs );
		}
	}
	// wait until we are looking at the impact
	if ( cs->aiFlags & AIFL_MISCFLAG2 ) {
		// pause any scripting
		cs->scriptPauseTime = level.time + 1000;
		// look at bullet impact
		VectorSubtract( cs->bulletImpactEnd, cs->bs->origin, v1 );
		VectorNormalize( v1 );
		vectoangles( v1, cs->ideal_viewangles );
		//
		// if we are facing that direction, we've looked at the impact point
		if ( fabs( cs->ideal_viewangles[YAW] - cs->viewangles[YAW] ) < 1 ) {
			cs->aiFlags &= ~AIFL_MISCFLAG2;
		}
		return NULL;
	} else if ( cs->aiFlags & AIFL_MISCFLAG1 ) {
		// clear the flag now
		cs->aiFlags &= ~AIFL_MISCFLAG1;
		// start looking back at bullet
		VectorSubtract( cs->bulletImpactStart, cs->bs->origin, v1 );
		VectorNormalize( v1 );
		vectoangles( v1, cs->ideal_viewangles );
		if ( cs->aiState < AISTATE_ALERT ) {
			// change to alert state
			if ( !AICast_StateChange( cs, AISTATE_ALERT ) ) {
				if ( cs->lastEnemy < 0 && cs->enemyNum < 0 ) {
					// look back at our original angles
					VectorCopy( ent->s.angles, cs->ideal_viewangles );
				}
				// stop doing whatever we are doing, and return control to scripting
				cs->scriptPauseTime = 0;
				return AIFunc_IdleStart( cs );
			}
			// make sure we didn't change thinkfunc
			if ( cs->aifunc != AIFunc_InspectBulletImpact ) {
				//G_Error( "scripting passed control out of AIFunc_InspectBulletImpact(), this is bad" );
				return NULL;
			}
		}
		// pause any scripting
		if ( ent->client->ps.legsTimer ) {
			cs->scriptPauseTime = level.time + ent->client->ps.legsTimer;
		} else { // just wait for a few seconds looking at the source
			cs->scriptPauseTime = level.time + 3500;
		}
	}
	// are we done?
	if ( cs->scriptPauseTime < level.time ) {
		if ( cs->lastEnemy < 0 && cs->enemyNum < 0 ) {
			// look back at our original angles
			VectorCopy( ent->s.angles, cs->ideal_viewangles );
		}
		return AIFunc_IdleStart( cs );
	}
	//
	// reload?
	AICast_IdleReload( cs );
	//
	// check for enemies
	{
		int numEnemies;
		//
		// look for things we should attack
		numEnemies = AICast_ScanForEnemies( cs, enemies );
		if ( numEnemies == -2 ) { // inspection
			// only override current objective if we are inspecting a dead guy, and the new inspect target is fighting someone
			if ( ( g_entities[cs->inspectNum].health <= 0 ) && ( g_entities[enemies[0]].health > 0 ) ) {

				return AIFunc_InspectFriendlyStart( cs, enemies[0] );
			}
		} else if ( numEnemies > 0 )     {
			int i;

			cs->enemyNum = enemies[0];  // just attack the first one
			// override with a visible enemy
			for ( i = 1; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
					cs->enemyNum = enemies[i];
					break;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}

			return AIFunc_BattleStart( cs );
		}
	}
	//
	return NULL;
}

/*
============
AIFunc_InspectBulletImpactStart()
============
*/
char *AIFunc_InspectBulletImpactStart( cast_state_t *cs ) {
	int oldScriptIndex;
	// set the impact timer so we ignore bullets while inspecting this one
	cs->bulletImpactIgnoreTime = level.time + 5000;
	// pause any scripting
	cs->scriptPauseTime = level.time + 1000;
	// set this so we know if we've started the trace back to the bullet origin
	cs->aiFlags |= AIFL_MISCFLAG1;
	cs->aiFlags |= AIFL_MISCFLAG2;
	//
	// call the script event
	oldScriptIndex = cs->scriptCallIndex;
	AICast_ScriptEvent( cs, "bulletimpactsound", "" );
	if ( oldScriptIndex == cs->scriptCallIndex ) {
		// no script event, so call the animation script
		BG_AnimScriptEvent( &g_entities[cs->entityNum].client->ps, ANIM_ET_BULLETIMPACT, qfalse, qtrue );
	}
	//
	// if the origin is not visible, set the bullet origin to the closest visible area from the src
	if ( !trap_InPVS( cs->bulletImpactStart, cs->bs->origin ) ) {
		// if it fails, then just look at the source
		trap_AAS_GetRouteFirstVisPos( g_entities[cs->bulletImpactEntity].s.pos.trBase, cs->bs->origin, cs->travelflags, cs->bulletImpactStart );
	}
	//
	cs->aifunc = AIFunc_InspectBulletImpact;
	return "AIFunc_InspectBulletImpact";
}

/*
============
AIFunc_InspectAudibleEvent

Investigates a remembered sound position.
Moves toward the sound, handles arrival/end script events, scans for threats, and idles briefly.
============
*/
char *AIFunc_InspectAudibleEvent( cast_state_t *cs ) {
	gentity_t *ent;
	bot_state_t *bs;
	vec3_t destorg, vec;
	float dist;
	qboolean moved;

	ent = &g_entities[cs->entityNum];
	moved = qfalse;

	if ( cs->enemyNum >= 0 ) {
		return AIFunc_BattleStart( cs );
	}

	cs->followDist = 64;

	// Danger can temporarily override sound inspection if cover is available
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( AICast_GetTakeCoverPos( cs, cs->dangerEntity,
									 cs->dangerEntityPos, cs->takeCoverPos ) ) {
			cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
			cs->attackcrouch_time = 0;
			cs->castScriptStatus.scriptGotoId = -1;
			cs->movestate = MS_DEFAULT;
			cs->movestateType = MSTYPE_NONE;
			return AIFunc_AvoidDangerStart( cs );
		}
	}

	// Door marker behavior temporarily overrides sound inspection
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	// Look-only mode waits for script pause to expire
	if ( cs->aiFlags & AIFL_MISCFLAG2 ) {
		if ( cs->scriptPauseTime <= level.time ) {
			return AIFunc_DefaultStart( cs );
		}

		return NULL;
	}

	VectorCopy( cs->audibleEventOrg, destorg );

	dist = Distance( destorg, cs->bs->origin );

	if ( !( dist < cs->followDist &&
			( ent->waterlevel || cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE ) ) ) {
		bs = cs->bs;

		// Mark that we are still travelling to the sound
		cs->aiFlags |= AIFL_MISCFLAG1;

		// Less aggressive AI investigates cautiously
		if ( cs->attributes[AGGRESSION] <= 0.8 ) {
			cs->movestate = MS_CROUCH;
			cs->movestateType = MSTYPE_TEMPORARY;
		}

		// Try direct movement before falling back to AAS routing
		if ( cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE || ent->waterlevel > 1 ) {
			aicast_predictmove_t move;
			vec3_t dir;
			bot_input_t bi;
			usercmd_t ucmd;
			trace_t tr;
			qboolean simTest;

			simTest = qfalse;

			if ( cs->attributes[RUNNING_SPEED] < 120 ) {
				simTest = qtrue;
			}

			if ( !simTest ) {
				trap_Trace( &tr, cs->bs->origin, NULL, NULL,
							destorg, cs->entityNum, ent->clipmask );

				if ( tr.fraction == 1 ) {
					simTest = qtrue;
				}
			}

			if ( simTest ) {
				gentity_t *gent;

				gent = G_Spawn();
				VectorCopy( destorg, gent->r.currentOrigin );

				VectorSubtract( destorg, cs->bs->origin, dir );
				VectorNormalize( dir );

				if ( !ent->waterlevel ) {
					dir[2] = 0;
				}

				trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi );
				VectorCopy( dir, bi.dir );
				bi.speed = 400;
				bi.actionflags = 0;

				AICast_InputToUserCommand( cs, &bi, &ucmd, bs->cur_ps.delta_angles );
				AICast_PredictMovement( cs, 10, 0.8, &move, &ucmd, gent->s.number );

				if ( move.stopevent == PREDICTSTOP_HITENT ) {
					trap_EA_Move( cs->entityNum, dir, 400 );
					vectoangles( dir, cs->ideal_viewangles );
					cs->ideal_viewangles[2] *= 0.5;
					moved = qtrue;
				}

				G_FreeEntity( gent );
			}
		}

		if ( !moved ) {
			moveresult = AICast_MoveToPos( cs, destorg, -1 );

			if ( moveresult && moveresult->failure ) {
				// If route fails, face the first visible route point if possible
				if ( trap_AAS_GetRouteFirstVisPos( cs->audibleEventOrg,
												   cs->bs->origin,
												   cs->travelflags,
												   destorg ) ) {
					cs->aiFlags |= AIFL_MISCFLAG2;

					VectorSubtract( destorg, cs->bs->origin, destorg );
					VectorNormalize( destorg );
					vectoangles( destorg, cs->ideal_viewangles );
					return NULL;
				}

				if ( cs->lastEnemy < 0 && cs->enemyNum < 0 ) {
					VectorCopy( ent->s.angles, cs->ideal_viewangles );
				}

				return AIFunc_DefaultStart( cs );
			} else if ( !moveresult ) {
				if ( trap_InPVS( destorg, cs->bs->origin ) ) {
					VectorSubtract( destorg, cs->bs->origin, vec );
					VectorNormalize( vec );
					vectoangles( vec, cs->ideal_viewangles );
				}
			}
		}

		if ( cs->followDist && cs->followSlowApproach ) {
			cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, cs->followDist );
		}
	} else if ( cs->aiFlags & AIFL_MISCFLAG1 ) {
		// Just arrived at the sound source
		cs->aiFlags &= ~AIFL_MISCFLAG1;
		cs->aiFlags &= ~AIFL_DENYACTION;

		AICast_ForceScriptEvent( cs, "inspectsoundend", g_entities[cs->audibleEventEnt].aiName );

		if ( cs->aiFlags & AIFL_DENYACTION ) {
			return AIFunc_DefaultStart( cs );
		}
	} else {
		// Wait and look around at the sound source
		if ( cs->battleHuntViewTime < level.time ) {
			cs->battleHuntViewTime = level.time + 700 + rand() % 1000;
			cs->ideal_viewangles[YAW] = AngleMod( cs->ideal_viewangles[YAW] +
												  ( 45.0 + random() * 45.0 ) *
												  ( 2 * ( rand() % 2 ) - 1 ) );
		}

		if ( cs->scriptPauseTime < level.time ) {
			if ( cs->lastEnemy < 0 && cs->enemyNum < 0 ) {
				VectorCopy( ent->s.angles, cs->ideal_viewangles );
			}

			return AIFunc_DefaultStart( cs );
		}
	}

	// Scan for new threats or higher-priority events during inspection
	{
		int numEnemies;

		numEnemies = AICast_ScanForEnemies( cs, enemies );

		if ( numEnemies == -1 ) {
			return NULL;
		} else if ( numEnemies == -2 ) {
			if ( g_entities[cs->inspectNum].health <= 0 &&
				 g_entities[enemies[0]].health > 0 ) {
				return AIFunc_InspectFriendlyStart( cs, enemies[0] );
			}
		} else if ( numEnemies == -4 ) {
			return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
		} else if ( numEnemies > 0 ) {
			int i;

			cs->enemyNum = enemies[0];

			for ( i = 1; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
					cs->enemyNum = enemies[i];
					break;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}

			return AIFunc_BattleStart( cs );
		}
	}

	if ( cs->enemyNum < 0 &&
		 cs->attributes[TACTICAL] >= 0.5 &&
		 !( cs->aiFlags & AIFL_NO_HEADLOOK ) ) {
		ent->client->ps.eFlags |= EF_HEADLOOK;
	}

	AICast_IdleReload( cs );

	return NULL;
}

/*
============
AIFunc_InspectAudibleEventStart

Starts investigation of an audible event.
Runs script approval first, then either inspects the source, takes cover, or investigates the sound.
============
*/
char *AIFunc_InspectAudibleEventStart( cast_state_t *cs, int entnum ) {
	cast_state_t *ocs;
	int oldScriptIndex;

	ocs = AICast_GetCastState( entnum );

	// Mark the audible event as processed
	cs->audibleEventTime = -9999;

	// Let scripts deny or override this investigation
	oldScriptIndex = cs->scriptCallIndex;
	AICast_ForceScriptEvent( cs, "inspectsoundstart", g_entities[cs->audibleEventEnt].aiName );

	if ( cs->aiFlags & AIFL_DENYACTION ) {
		return NULL;
	}

	if ( cs->aiState < AISTATE_ALERT ) {
		AICast_StateChange( cs, AISTATE_ALERT );
	}

	// If script did not change, play the default inspect-sound animation
	if ( oldScriptIndex == cs->scriptCallIndex ) {
		BG_AnimScriptEvent( &g_entities[cs->entityNum].client->ps,
							 ANIM_ET_INSPECTSOUND, qfalse, qtrue );
	}

	// Pause scripted walking while investigating
	cs->scriptPauseTime = level.time + 4000;

	// Clear "look only" flag
	cs->aiFlags &= ~AIFL_MISCFLAG2;

	// Dead sound source is inspected like a friendly/body event
	if ( g_entities[entnum].health <= 0 ) {
		cs->inspectNum = entnum;
		cs->aifunc = AIFunc_InspectFriendly;
		return "AIFunc_InspectFriendlyStart";
	}

	// Low-aggression AI may hide from combat sounds
	if ( ocs->aiState >= AISTATE_COMBAT && cs->attributes[AGGRESSION] < 0.3 ) {
		if ( AICast_GetTakeCoverPos( cs, entnum,
									 g_entities[entnum].client->ps.origin,
									 cs->takeCoverPos ) ) {
			cs->takeCoverTime = level.time + 10000;
			cs->scriptPauseTime = cs->takeCoverTime;

			if ( cs->attributes[ATTACK_CROUCH] > 0.1 ) {
				cs->attackcrouch_time = level.time + 3000;
			}

			return AIFunc_BattleTakeCoverStart( cs );
		}
	}

	cs->aifunc = AIFunc_InspectAudibleEvent;
	return "AIFunc_InspectAudibleEvent";
}

/*
============
AIFunc_ChaseGoalIdle

Idle wait state while following a target.
Resumes chase when target moves out of range, reacts to threats, and faces relevant targets.
============
*/
char *AIFunc_ChaseGoalIdle( cast_state_t *cs ) {
	gentity_t *followent;
	vec3_t dir;

	if ( cs->followEntity < 0 ) {
		AICast_EndChase( cs );
		return AIFunc_IdleStart( cs );
	}

	followent = &g_entities[cs->followEntity];

	// Danger can temporarily override follow idle if cover is available
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( AICast_GetTakeCoverPos( cs, cs->dangerEntity,
									 cs->dangerEntityPos, cs->takeCoverPos ) ) {
			cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
			cs->attackcrouch_time = 0;
			return AIFunc_AvoidDangerStart( cs );
		}
	}

	// Door marker behavior temporarily overrides follow idle
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	// Wait for unavailable follow target
	if ( !followent->inuse ) {
		return NULL;
	}

	// Scripted goto expired
	if ( cs->followIsGoto && cs->followTime < level.time ) {
		return AIFunc_Idle( cs );
	}

	// Target moved outside follow range, resume chasing
	if ( Distance( followent->r.currentOrigin, cs->bs->origin ) > cs->followDist ) {
		return AIFunc_ChaseGoalStart( cs, cs->followEntity, cs->followDist, qtrue );
	}

	if ( cs->obstructingTime > level.time ) {
		AICast_MoveToPos( cs, cs->obstructingPos, -1 );
		cs->speedScale = cs->attributes[WALKING_SPEED] / cs->attributes[RUNNING_SPEED];
	} else if ( cs->enemyNum >= 0 ) {
		AICast_ProcessAttack( cs );
	} else if ( cs->lastEnemy >= 0 ) {
		VectorSubtract( cs->vislist[cs->lastEnemy].visible_pos, cs->bs->origin, dir );

		if ( VectorLength( dir ) < 1 ) {
			cs->ideal_viewangles[PITCH] = 0;
		} else {
			VectorNormalize( dir );
			vectoangles( dir, cs->ideal_viewangles );
		}

		AICast_IdleReload( cs );
	} else if ( followent->client ) {
		// Face the followed client while waiting
		VectorSubtract( followent->r.currentOrigin, cs->bs->origin, dir );
		dir[2] += followent->client->ps.viewheight -
				  g_entities[cs->bs->entitynum].client->ps.viewheight;
		VectorNormalize( dir );
		vectoangles( dir, cs->ideal_viewangles );
	}

	// Scan for enemies or other perception events
	numEnemies = AICast_ScanForEnemies( cs, enemies );

	if ( numEnemies == -1 ) {
		return NULL;
	} else if ( numEnemies == -2 ) {
		char *retval;

		retval = AIFunc_InspectFriendlyStart( cs, enemies[0] );
		if ( retval ) {
			return retval;
		}
	} else if ( numEnemies == -3 ) {
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectBulletImpactStart( cs );
		}
	} else if ( numEnemies == -4 ) {
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
		}
	} else if ( numEnemies > 0 ) {
		cs->enemyNum = enemies[0];
	}

	if ( cs->enemyNum < 0 &&
		 cs->attributes[TACTICAL] >= 0.5 &&
		 !( cs->aiFlags & AIFL_NO_HEADLOOK ) ) {
		g_entities[cs->entityNum].client->ps.eFlags |= EF_HEADLOOK;
	}

	return NULL;
}

/*
============
AIFunc_ChaseGoalIdleStart

Starts idle waiting while following a target.
Resets avoid-reach data, sets idle animation style, and stores follow target/range.
============
*/
char *AIFunc_ChaseGoalIdleStart( cast_state_t *cs, int entitynum, float reachdist ) {
	// Clear old avoid-reach data when entering follow idle
	trap_BotInitAvoidReach( cs->bs->ms );

	if ( entitynum < MAX_CLIENTS ) {
		// Followers of clients should stay in ready/default idle
		g_entities[cs->entityNum].client->ps.eFlags &= ~EF_STAND_IDLE2;
	} else if ( cs->aiFlags & AIFL_STAND_IDLE2 ) {
		// Non-client targets may use alternate casual idle
		g_entities[cs->entityNum].client->ps.eFlags |= EF_STAND_IDLE2;
	}

	cs->followEntity = entitynum;
	cs->followDist = reachdist;

	cs->aifunc = AIFunc_ChaseGoalIdle;
	return "AIFunc_ChaseGoalIdle";
}

/*
============
AIFunc_ChaseGoal

Follows a target entity using direct movement when possible, otherwise AAS routing.
Handles danger, doors, scripted goto expiry, leader spacing, obstruction, and enemy scanning.
============
*/
char *AIFunc_ChaseGoal( cast_state_t *cs ) {
	gentity_t *followent, *ent;
	bot_state_t *bs;
	vec3_t destorg;
	float dist;
	qboolean moved;

	ent = &g_entities[cs->entityNum];
	bs = cs->bs;
	moved = qfalse;

	if ( cs->followEntity < 0 ) {
		AICast_EndChase( cs );
		return AIFunc_IdleStart( cs );
	}

	// Danger can temporarily override following if cover is available
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( AICast_GetTakeCoverPos( cs, cs->dangerEntity,
									 cs->dangerEntityPos, cs->takeCoverPos ) ) {
			cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
			cs->attackcrouch_time = 0;
			cs->castScriptStatus.scriptGotoId = -1;
			cs->movestate = MS_DEFAULT;
			cs->movestateType = MSTYPE_NONE;
			return AIFunc_AvoidDangerStart( cs );
		}
	}

	// Door marker behavior temporarily overrides following
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	followent = &g_entities[cs->followEntity];

	// Target is not ready or disappeared
	if ( !followent->inuse ) {
		if ( cs->followEntity < MAX_CLIENTS &&
			 ( ( followent->client && followent->client->pers.connected == CON_CONNECTING ) ||
			   level.time < 3000 ) ) {
			return AIFunc_ChaseGoalIdleStart( cs, cs->followEntity, cs->followDist );
		}

		AICast_EndChase( cs );
		return AIFunc_IdleStart( cs );
	}

	// Scripted goto expired
	if ( cs->followIsGoto && cs->followTime < level.time ) {
		return AIFunc_IdleStart( cs );
	}

	if ( followent->client ) {
		VectorCopy( followent->client->ps.origin, destorg );
	} else {
		VectorCopy( followent->r.currentOrigin, destorg );
	}

	dist = Distance( destorg, cs->bs->origin );

	// Slow approach allows stopping inside follow distance when grounded or swimming
	if ( cs->followSlowApproach &&
		 dist < cs->followDist &&
		 ( ent->waterlevel || cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE ) ) {
		if ( cs->followEntity == cs->castScriptStatus.scriptGotoEnt ) {
			AICast_EndChase( cs );
			return AIFunc_IdleStart( cs );
		}

		if ( cs->followEntity == cs->leaderNum ) {
			if ( dist < AICAST_LEADERDIST_MIN ) {
				AICast_EndChase( cs );
				return AIFunc_IdleStart( cs );
			} else {
				trace_t tr;

				// If there is no clear line to the leader, stop crowding forward
				trap_Trace( &tr, cs->bs->origin, cs->bs->cur_ps.mins, cs->bs->cur_ps.maxs,
							g_entities[cs->followEntity].r.currentOrigin,
							cs->entityNum, ent->clipmask );

				if ( tr.entityNum != cs->followEntity ) {
					AICast_EndChase( cs );
					return AIFunc_IdleStart( cs );
				}

				if ( cs->attributes[ATTACK_CROUCH] > 0.1 ) {
					cs->attackcrouch_time = level.time + 1000;
				}
			}
		} else {
			return AIFunc_ChaseGoalIdleStart( cs, cs->followEntity, cs->followDist );
		}
	}

	// Try direct movement before falling back to AAS routing
	if ( cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE || ent->waterlevel > 1 ) {
		aicast_predictmove_t move;
		vec3_t dir;
		bot_input_t bi;
		usercmd_t ucmd;
		trace_t tr;
		qboolean simTest;
		float frameTime, goaldist;

		simTest = qfalse;
		frameTime = 0.8;

		if ( cs->attributes[RUNNING_SPEED] < 120 ) {
			simTest = qtrue;
		}

		if ( !simTest ) {
			trap_Trace( &tr, cs->bs->origin, NULL, NULL,
						followent->r.currentOrigin, cs->entityNum, ent->clipmask );

			if ( tr.entityNum == cs->followEntity || tr.fraction == 1 ) {
				simTest = qtrue;
			}
		}

		if ( simTest ) {
			VectorSubtract( followent->r.currentOrigin, cs->bs->origin, dir );

			if ( !ent->waterlevel ) {
				dir[2] = 0;
			}

			goaldist = VectorNormalize( dir );

			trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi );
			VectorCopy( dir, bi.dir );
			bi.speed = 400;
			bi.actionflags = 0;

			AICast_InputToUserCommand( cs, &bi, &ucmd, bs->cur_ps.delta_angles );
			AICast_PredictMovement( cs, 10, frameTime, &move, &ucmd, cs->followEntity );

			if ( move.stopevent == PREDICTSTOP_HITENT ) {
				// Avoid direct movement if prediction spent too long sliding along obstacles
				if ( move.frames * frameTime <
					 1.0 + goaldist / ( bs->cur_ps.speed * bs->cur_ps.runSpeedScale ) ) {
					trap_EA_Move( cs->entityNum, dir, 400 );
					vectoangles( dir, cs->ideal_viewangles );
					cs->ideal_viewangles[2] *= 0.5;
					moved = qtrue;
				}
			}
		}
	}

	if ( !moved ) {
		moveresult = AICast_MoveToPos( cs, followent->r.currentOrigin, cs->followEntity );

		if ( moveresult && moveresult->failure ) {
			// Path failed, keep current chase state and retry later
		}
	}

	if ( cs->followDist && cs->followSlowApproach && cs->followDist < 48 ) {
		cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, cs->followDist );
	}

	// Move out of another AI's way if requested
	if ( cs->obstructingTime > level.time ) {
		AICast_MoveToPos( cs, cs->obstructingPos, -1 );

		if ( cs->movestate != MS_CROUCH ) {
			cs->movestate = MS_WALK;
		}

		cs->movestateType = MSTYPE_TEMPORARY;
	}

	if ( cs->enemyNum >= 0 ) {
		AICast_ProcessAttack( cs );
	} else {
		int numEnemies;

		numEnemies = AICast_ScanForEnemies( cs, enemies );

		if ( numEnemies == -1 ) {
			return NULL;
		} else if ( numEnemies == -2 ) {
			char *retval;

			retval = AIFunc_InspectFriendlyStart( cs, enemies[0] );
			if ( retval ) {
				return retval;
			}
		} else if ( numEnemies == -3 ) {
			if ( cs->aiState < AISTATE_COMBAT ) {
				return AIFunc_InspectBulletImpactStart( cs );
			}
		} else if ( numEnemies == -4 ) {
			if ( cs->aiState < AISTATE_COMBAT ) {
				return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
			}
		} else if ( numEnemies > 0 ) {
			int i;

			cs->enemyNum = enemies[0];

			for ( i = 1; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
					cs->enemyNum = enemies[i];
					break;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}
		}

		AICast_IdleReload( cs );
	}

	if ( cs->enemyNum < 0 &&
		 cs->attributes[TACTICAL] >= 0.5 &&
		 !( cs->aiFlags & AIFL_NO_HEADLOOK ) ) {
		ent->client->ps.eFlags |= EF_HEADLOOK;
	}

	return NULL;
}

/*
============
AIFunc_ChaseGoalStart

Starts chasing/following a target entity.
Stores follow distance, goto state, and slow-approach behavior.
============
*/
char *AIFunc_ChaseGoalStart( cast_state_t *cs, int entitynum, float reachdist, qboolean slowApproach ) {
	cs->followEntity = entitynum;
	cs->followDist = reachdist;
	cs->followIsGoto = qfalse;
	cs->followSlowApproach = slowApproach;

	cs->aifunc = AIFunc_ChaseGoal;
	return "AIFunc_ChaseGoal";
}

/*
============
AIFunc_DoorMarker()
============
*/
char *AIFunc_DoorMarker( cast_state_t *cs ) {
	gentity_t   *followent, *door;
	vec3_t destorg = { 0 };
	float dist;
	//
	// do we need to avoid a danger?
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// shit??
		}
		// go to a position that cannot be seen from the dangerPos
		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}

	followent = &g_entities[cs->doorMarker];

	// if the entity is not ready yet
	if ( !followent->inuse ) {
		cs->doorMarkerTime = 0;
		//return AIFunc_DefaultStart( cs );
		return AIFunc_Restore( cs );
	}

	// if the door is open or idle
	door = &g_entities[cs->doorEntNum];
	if (    ( !door->key ) &&
			( door->s.apos.trType == TR_STATIONARY && door->s.pos.trType == TR_STATIONARY ) ) {
		cs->doorMarkerTime = 0;
		//return AIFunc_DefaultStart( cs );
		return AIFunc_Restore( cs );
	}

	// if we have an enemy, fire if they're visible
	if ( cs->enemyNum >= 0 ) { //attack the enemy if possible
		AICast_ProcessAttack( cs );
	}

	// they are ready, are they inside range? FIXME: make configurable
	dist = Distance( destorg, cs->bs->origin );
	if ( dist < 12 ) {
		// check for a movement we should be making
		if ( cs->obstructingTime > level.time ) {
			AICast_MoveToPos( cs, cs->obstructingPos, -1 );
		}
		// if the door is locked, resume
		if ( followent->key ) {
			return AIFunc_Restore( cs );
		}
		return NULL;
	}

	// go to it
	//
	moveresult = AICast_MoveToPos( cs, followent->r.currentOrigin, followent->s.number );
	// if we cant get there, forget it
	if ( moveresult && moveresult->failure ) {
		return AIFunc_Restore( cs );
	}
	// should we slow down?
	if ( cs->followDist ) {
		cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, cs->followDist );
	}
	// reload?
	AICast_IdleReload( cs );
	return NULL;

}

/*
============
AIFunc_DoorMarkerStart()
============
*/
char *AIFunc_DoorMarkerStart( cast_state_t *cs, int doornum, int markernum ) {
	cs->doorEntNum = doornum;
	cs->doorMarker = markernum;
	cs->oldAifunc = cs->aifunc;
	cs->aifunc = AIFunc_DoorMarker;
	return "AIFunc_DoorMarker";
}

/*
=============
AIFunc_BattleRoll()
=============
*/
char *AIFunc_BattleRoll( cast_state_t *cs ) {
	gclient_t *client = &level.clients[cs->entityNum];
	vec3_t dir;
	//
	// record the time
	cs->lastRollMove = level.time;
	client->ps.eFlags |= EF_NOSWINGANGLES;
	//
	if ( !client->ps.torsoTimer ) {
		if ( cs->battleRollTime < level.time ) {
			return AIFunc_Restore( cs );
		} else {
			// attack?
			if ( cs->enemyNum >= 0 ) {
				AICast_ProcessAttack( cs );
			}
		}
	}
	if ( g_entities[cs->entityNum].health <= 0 ) {
		return AIFunc_DefaultStart( cs );
	}
	//
	trap_EA_Crouch( cs->entityNum );
	cs->attackcrouch_time = level.time + 500;
	// all characters so far only move during the first second of animation
	if ( cs->thinkFuncChangeTime > level.time - 1000 ) {
		// just move in the direction of our ideal_viewangles
		AngleVectors( cs->ideal_viewangles, dir, NULL, NULL );
		trap_EA_Move( cs->entityNum, dir, 300 );
		// we are crouching, move a little faster than normal
		cs->speedScale = 1.5;
	} else if ( cs->takeCoverTime > level.time ) {
		//
		// if we are taking Cover, use this position, if it's bad, we'll just look for a better spot once we're done here
		VectorCopy( cs->bs->origin, cs->takeCoverPos );
	} else if ( cs->enemyNum >= 0 ) {
		//
		// start turning towards our enemy
		AICast_ProcessAttack( cs );
	}
	//
	return NULL;
}

/*
=============
AIFunc_BattleRollStart()
=============
*/
char *AIFunc_BattleRollStart( cast_state_t *cs, vec3_t vec ) {
	int duration;
//	gclient_t *client = &level.clients[cs->entityNum];
	//
	// backup the current thinkfunc, so we can return to it when done
	cs->oldAifunc = cs->aifunc;
	//
	// face the direction of movement
	vectoangles( vec, cs->ideal_viewangles );
	// do the roll
	duration = BG_AnimScriptEvent( &g_entities[cs->entityNum].client->ps, ANIM_ET_ROLL, qfalse, qtrue );
	//
	if ( duration < 0 ) { // it failed
		return NULL;
	}
	// add some duration to make sure it fully plays out
	duration += 100;
	g_entities[cs->entityNum].client->ps.legsTimer = duration;
	g_entities[cs->entityNum].client->ps.torsoTimer = duration;
	//
	cs->noAttackTime = level.time + duration - 200;
	// set the duration
	cs->battleRollTime = level.time + duration;
	// move into crouch position
	//cs->attackcrouch_time = level.time + (duration) + 1000;
	// record the time
	cs->lastRollMove = level.time;
	//
	// make sure we move this frame
	AIFunc_BattleRoll( cs );
	//
	cs->aifunc = AIFunc_BattleRoll;
	return "AIFunc_BattleRoll";
}

/*
=============
AIFunc_BattleDiveStart()
=============
*/
char *AIFunc_BattleDiveStart( cast_state_t *cs, vec3_t vec ) {
	int duration;
//	gclient_t *client = &level.clients[cs->entityNum];
	//
	// backup the current thinkfunc, so we can return to it when done
	cs->oldAifunc = cs->aifunc;
	//
	// face the direction of movement
	vectoangles( vec, cs->ideal_viewangles );
	// force crouching anim
	BG_UpdateConditionValue( cs->entityNum, ANIM_COND_CROUCHING, qtrue, qfalse );
	// do the roll
	duration = BG_AnimScriptEvent( &g_entities[cs->entityNum].client->ps, ANIM_ET_DIVE, qfalse, qtrue );
	//
	if ( duration < 0 ) { // it failed
		return NULL;
	}
	//
	cs->noAttackTime = level.time + duration - 200;
	// set the duration
	cs->battleRollTime = level.time + duration;
	// move into crouch position
	//cs->attackcrouch_time = level.time + (duration) + 1000;
	// record the time
	cs->lastRollMove = level.time;
	//
	// make sure we move this frame
	AIFunc_BattleRoll( cs );
	//
	cs->aifunc = AIFunc_BattleRoll;
	return "AIFunc_BattleRoll";
}

/*
=============
AIFunc_FlipMove()
=============
*/
char *AIFunc_FlipMove( cast_state_t *cs ) {
	gclient_t *client = &level.clients[cs->entityNum];
	vec3_t dir;
	//
	if ( !client->ps.torsoTimer ) {
		cs->attackcrouch_time = 0;
		return AIFunc_Restore( cs );
	}
	if ( g_entities[cs->entityNum].health <= 0 ) {
		return AIFunc_DefaultStart( cs );
	}
	//
	// just move in the direction of our ideal_viewangles
	AngleVectors( cs->ideal_viewangles, dir, NULL, NULL );
	trap_EA_Move( cs->entityNum, dir, 400 );
	// if we are crouching, move a little faster than normal
	if ( cs->attackcrouch_time > level.time ) {
		cs->speedScale = 1.5;
	}
	//
	return NULL;
}

/*
=============
AIFunc_FlipMoveStart()
=============
*/
char *AIFunc_FlipMoveStart( cast_state_t *cs, vec3_t vec ) {
	int duration;
//	gclient_t *client = &level.clients[cs->entityNum];
	//
	// backup the current thinkfunc, so we can return to it when done
	cs->oldAifunc = cs->aifunc;
	//
	// record the time
	cs->lastRollMove = level.time;
	// face the direction of movement
	vectoangles( vec, cs->ideal_viewangles );
	cs->noAttackTime = level.time + 1200;
	// do the roll
	duration = BG_AnimScriptEvent( &g_entities[cs->entityNum].client->ps, ANIM_ET_ROLL, qfalse, qfalse );
	//
	if ( duration < 0 ) { // it failed
		return NULL;
	}
	// move into crouch position
	cs->attackcrouch_time = level.time + 800;
	//
	// make sure we move this frame
	AIFunc_FlipMove( cs );
	//
	cs->aifunc = AIFunc_FlipMove;
	return "AIFunc_FlipMove";
}

/*
=============
AIFunc_BattleHunt

Searches through chase markers after reaching the enemy's last known position.
Can reacquire enemies, pause to look around, then transition into ambush.
=============
*/
char *AIFunc_BattleHunt( cast_state_t *cs ) {
	const float chaseDist = 32;
	gentity_t *followent;
	vec3_t destorg;
	qboolean moved;
	char *rval;
	float dist;
	int i;

	moved = qfalse;

	// Danger overrides hunt behavior
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity,
									  cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// No cover found, but still enter danger avoidance
		}

		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}

	// Door marker behavior temporarily overrides hunt
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	if ( cs->enemyNum < 0 ) {
		return AIFunc_IdleStart( cs );
	}

	if ( cs->aiFlags & AIFL_ATTACK_CROUCH ) {
		cs->attackcrouch_time = level.time + 1000;
	}

	followent = &g_entities[cs->enemyNum];

	// Enemy disappeared
	if ( !followent->inuse ) {
		if ( !( cs->enemyNum < MAX_CLIENTS &&
				( ( followent->client && followent->client->pers.connected == CON_CONNECTING ) ||
				  level.time < 3000 ) ) ) {
			cs->enemyNum = -1;
		}

		return AIFunc_IdleStart( cs );
	}

	// If enemy is visible and attackable, return to battle or shoot while hunting
	AICast_ChooseWeapon( cs, qtrue );

	if ( AICast_EntityVisible( cs, cs->enemyNum, qtrue ) &&
		 AICast_CheckAttack( cs, cs->enemyNum, qfalse ) &&
		 cs->obstructingTime < level.time ) {
		if ( AICast_StopAndAttack( cs ) ) {
			rval = AIFunc_BattleStart( cs );
			if ( rval ) {
				return rval;
			}
		} else {
			AICast_ProcessAttack( cs );
		}

		AICast_ChooseWeapon( cs, qfalse );
	} else {
		int numEnemies;

		AICast_ChooseWeapon( cs, qfalse );

		numEnemies = AICast_ScanForEnemies( cs, enemies );

		if ( numEnemies == -1 ) {
			return NULL;
		} else if ( numEnemies == -2 ) {
			char *retval;

			if ( cs->aiState < AISTATE_COMBAT ) {
				retval = AIFunc_InspectFriendlyStart( cs, enemies[0] );
				if ( retval ) {
					return retval;
				}
			}
		} else if ( numEnemies == -3 ) {
			if ( cs->aiState < AISTATE_COMBAT ) {
				return AIFunc_InspectBulletImpactStart( cs );
			}
		} else if ( numEnemies == -4 ) {
			if ( cs->aiState < AISTATE_COMBAT ) {
				return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
			}
		} else if ( AICast_GotEnoughAmmoForWeapon( cs, cs->bs->cur_ps.weapon ) ) {
			if ( numEnemies > 0 ) {
				cs->enemyNum = enemies[0];

				for ( i = 0; i < numEnemies; i++ ) {
					if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ||
						 AICast_CheckAttack( AICast_GetCastState( enemies[i] ), cs->entityNum, qfalse ) ) {
						cs->enemyNum = enemies[i];
						break;
					} else if ( cs->enemyNum < 0 ) {
						cs->lastEnemy = enemies[i];
					}
				}
			}
		}

		AICast_ChooseWeapon( cs, qfalse );
	}

	// Drop from combat to alert if enemy has not been visible for long enough
	if ( cs->aiState == AISTATE_COMBAT ) {
		if ( cs->vislist[cs->enemyNum].visible_timestamp < level.time - COMBAT_TIMEOUT ) {
			AICast_StateChange( cs, AISTATE_ALERT );
		}
	}

	// Hunt behavior prefers crouching when supported
	if ( cs->attributes[ATTACK_CROUCH] >= 0.1 ) {
		cs->attackcrouch_time = level.time + 1000;
	}

	if ( cs->battleHuntPauseTime ) {
		if ( cs->battleHuntPauseTime < level.time ) {
			// Pause finished, switch to ambush from the current marker
			if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
										 cs->vislist[cs->enemyNum].chase_marker[cs->battleChaseMarker],
										 cs->takeCoverPos ) ) {
				VectorCopy( cs->vislist[cs->enemyNum].chase_marker[cs->battleChaseMarker],
							cs->combatGoalOrigin );
				return AIFunc_BattleAmbushStart( cs );
			}

			VectorCopy( cs->bs->origin, cs->combatGoalOrigin );
			VectorCopy( cs->bs->origin, cs->takeCoverPos );
			return AIFunc_BattleAmbushStart( cs );
		}

		// Stay in place and look around while paused
		if ( cs->battleHuntViewTime < level.time ) {
			cs->battleHuntViewTime = level.time + 700 + rand() % 1000;
			cs->ideal_viewangles[YAW] = AngleMod( cs->ideal_viewangles[YAW] +
												  ( 45.0 + random() * 45.0 ) *
												  ( 2 * ( rand() % 2 ) - 1 ) );
			cs->ideal_viewangles[PITCH] = 0;
		}
	} else {
		// Follow stored chase markers
		VectorCopy( cs->vislist[cs->enemyNum].chase_marker[cs->battleChaseMarker], destorg );

		dist = Distance( destorg, cs->bs->origin );

		if ( dist < chaseDist ) {
			if ( cs->battleChaseMarker == cs->vislist[cs->enemyNum].chase_marker_count - 1 ) {
				cs->battleHuntPauseTime = level.time + 4000;
				cs->battleHuntViewTime = level.time + 1000;
			} else {
				cs->battleChaseMarker += cs->battleChaseMarkerDir;

				if ( cs->battleChaseMarker > cs->vislist[cs->enemyNum].chase_marker_count ) {
					cs->battleChaseMarkerDir *= -1;
					cs->battleChaseMarker = cs->vislist[cs->enemyNum].chase_marker_count - 1;
				}

				if ( cs->battleChaseMarker < 0 ) {
					cs->battleChaseMarkerDir = 1;
					cs->battleChaseMarker = 0;
				}
			}
		}

		if ( cs->battleHuntPauseTime < level.time ) {
			if ( !moved && cs->leaderNum < 0 ) {
				moveresult = AICast_MoveToPos( cs, destorg, cs->enemyNum );

				if ( moveresult && moveresult->failure ) {
					cs->enemyNum = -1;
					return AIFunc_DefaultStart( cs );
				}

				cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, chaseDist );
			}
		}
	}

	AICast_IdleReload( cs );

	return NULL;
}

/*
=============
AIFunc_BattleHuntStart

Starts hunt behavior after reaching the enemy's last known position.
Resets hunt marker traversal and pause timing.
=============
*/
char *AIFunc_BattleHuntStart( cast_state_t *cs ) {

	cs->combatGoalTime = 0;
	cs->battleChaseMarker = 0;
	cs->battleChaseMarkerDir = 1;
	cs->battleHuntPauseTime = 0;

	cs->aifunc = AIFunc_BattleHunt;
	return "AIFunc_BattleHunt";
}

/*
=============
AIFunc_BattleAmbush

Holds or moves to an ambush/cover position after losing the enemy.
Keeps facing likely threat directions, reacquires enemies, and waits in cover indefinitely.
=============
*/
char *AIFunc_BattleAmbush( cast_state_t *cs ) {
	bot_state_t *bs;
	vec3_t destorg, vec, dir;
	float dist;
	int enemies[MAX_CLIENTS], numEnemies, i;
	qboolean shouldAttack, idleYaw;
	aicast_predictmove_t move;

	// Danger overrides ambush behavior if cover can be found
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( AICast_GetTakeCoverPos( cs, cs->dangerEntity,
									 cs->dangerEntityPos, cs->takeCoverPos ) ) {
			cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
			cs->attackcrouch_time = 0;
			return AIFunc_AvoidDangerStart( cs );
		}
	}

	// Door marker behavior temporarily overrides ambush
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	bs = cs->bs;

	if ( cs->enemyNum < 0 ) {
		return AIFunc_IdleStart( cs );
	}

	// Drop from combat to alert if enemy has not been visible for long enough
	if ( cs->aiState == AISTATE_COMBAT ) {
		if ( cs->vislist[cs->enemyNum].visible_timestamp < level.time - COMBAT_TIMEOUT ) {
			AICast_StateChange( cs, AISTATE_ALERT );
		}
	}

	// Ambush/hunt behavior prefers crouching when supported
	if ( cs->attributes[ATTACK_CROUCH] >= 0.1 ) {
		cs->attackcrouch_time = level.time + 2000;
	}

	VectorCopy( cs->takeCoverPos, destorg );
	VectorSubtract( destorg, cs->bs->origin, vec );
	vec[2] *= 0.2;
	dist = VectorLength( vec );

	// Use latest chase marker as the ambush reference point
	if ( cs->vislist[cs->enemyNum].chase_marker_count > 0 ) {
		VectorCopy( cs->vislist[cs->enemyNum].chase_marker[cs->vislist[cs->enemyNum].chase_marker_count - 1],
					cs->combatGoalOrigin );
	}

	shouldAttack = qfalse;
	numEnemies = AICast_ScanForEnemies( cs, enemies );

	// Ambush mode tries not to be interrupted by non-combat perception events
	if ( numEnemies == -2 ) {
		cs->vislist[enemies[0]].flags |= AIVIS_INSPECTED;
		cs->vislist[enemies[0]].flags &= ~AIVIS_INSPECT;
	} else if ( numEnemies > 0 ) {
		if ( AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
			cs->enemyNum = enemies[0];

			for ( i = 0; i < numEnemies; i++ ) {
				if ( ( AICast_EntityVisible( cs, enemies[i], qfalse ) &&
					   AICast_CheckAttack( cs, enemies[i], qfalse ) ) ||
					 ( ( VectorLength( cs->takeCoverPos ) < 1 || dist <= 8 ) &&
					   ( AICast_EntityVisible( AICast_GetCastState( enemies[i] ), cs->entityNum, qfalse ) ||
						 AICast_CheckAttack( AICast_GetCastState( enemies[i] ), cs->entityNum, qfalse ) ||
						 AICast_EntityVisible( AICast_GetCastState( enemies[i] ), cs->entityNum, qtrue ) ) ) ) {
					cs->enemyNum = enemies[i];
					return AIFunc_BattleStart( cs );
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				} else if ( AICast_EntityVisible( cs, enemies[i], qfalse ) ) {
					bot_input_t bi_back;

					// If we can path toward the visible enemy, switch from ambush to chase
					trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi_back );

					if ( AICast_MoveToPos( cs, g_entities[enemies[i]].client->ps.origin, enemies[i] ) ) {
						if ( !moveresult->failure ) {
							cs->enemyNum = enemies[i];
							return AIFunc_BattleChaseStart( cs );
						}
					} else {
						trap_EA_ResetInput( cs->entityNum, &bi_back );
					}
				}
			}
		} else {
			AICast_ChooseWeapon( cs, qfalse );

			if ( !AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
				if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
											 cs->vislist[cs->enemyNum].visible_pos,
											 cs->takeCoverPos ) ) {
					cs->takeCoverTime = level.time + 2000 + rand() % 3000;
					return AIFunc_BattleTakeCoverStart( cs );
				}
			}
		}
	}

	// Ambush cover is kept alive indefinitely
	cs->takeCoverTime = level.time + 1000;

	memset( &move, 0, sizeof( move ) );

	// Move to ambush position if not there yet
	if ( VectorLength( cs->takeCoverPos ) > 1 &&
		 dist > 8 &&
		 cs->obstructingTime < level.time ) {
		const float simTime = 0.8;
		float enemyDist;

		cs->takeCoverTime = level.time + 2000 + rand() % 2000;

		moveresult = AICast_MoveToPos( cs, destorg, -1 );

		if ( moveresult ) {
			if ( moveresult->failure ) {
				trap_BotResetAvoidReach( bs->ms );
				VectorClear( cs->takeCoverPos );
				dist = 0;
			}

			if ( moveresult->blocked ) {
				VectorClear( cs->takeCoverPos );
				dist = 0;
			}
		}

		// Abort movement if prediction collides with a client or moves closer to a nearby enemy
		AICast_PredictMovement( cs, 1, simTime, &move, &cs->lastucmd, -1 );

		enemyDist = Distance( cs->bs->origin, g_entities[cs->enemyNum].s.pos.trBase );
		VectorSubtract( move.endpos, cs->bs->origin, vec );
		VectorNormalize( vec );

		if ( ( move.numtouch && move.touchents[0] < aicast_maxclients ) ||
			 ( enemyDist < 128 &&
			   ( enemyDist - 1 ) > Distance( move.endpos, g_entities[cs->enemyNum].s.pos.trBase ) ) ) {
			VectorClear( cs->takeCoverPos );
		} else if ( dist < 64 ) {
			cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, 0 );
		}

		// Stop early if current/predicted position is already hidden from the ambush origin
		if ( !( cs->aiFlags & AIFL_MISCFLAG1 ) ) {
			if ( move.numtouch ||
				 !AICast_VisibleFromPos( move.endpos, cs->entityNum,
										 cs->combatGoalOrigin, cs->enemyNum, qfalse ) ) {
				cs->aiFlags |= AIFL_MISCFLAG1;
				VectorCopy( cs->bs->origin, cs->takeCoverPos );
			}
		}
	} else {
		if ( cs->obstructingTime > level.time ) {
			AICast_MoveToPos( cs, cs->obstructingPos, -1 );
		}

		if ( shouldAttack ) {
			return AIFunc_BattleStart( cs );
		} else if ( cs->leaderNum >= 0 &&
					Distance( cs->bs->origin, g_entities[cs->leaderNum].r.currentOrigin ) > MAX_LEADER_DIST ) {
			if ( level.time > cs->takeCoverTime ) {
				return AIFunc_ChaseGoalStart( cs, cs->leaderNum, AICAST_LEADERDIST_MAX, qtrue );
			}
		}

		if ( cs->attributes[ATTACK_CROUCH] > 0.1 || cs->crouchHideFlag ) {
			cs->attackcrouch_time = level.time + 2000;
		}
	}

	// Choose idle facing direction while hiding
	if ( !( cs->aiFlags & AIFL_WALKFORWARD ) || !VectorLength( cs->bs->cur_ps.velocity ) ) {
		idleYaw = qtrue;

		if ( cs->enemyNum >= 0 ) {
			VectorSubtract( g_entities[cs->enemyNum].s.pos.trBase, cs->bs->origin, dir );
			vectoangles( dir, cs->ideal_viewangles );
			idleYaw = qfalse;
		} else if ( cs->lastEnemy >= 0 ) {
			VectorSubtract( g_entities[cs->lastEnemy].s.pos.trBase, cs->bs->origin, dir );
			vectoangles( dir, cs->ideal_viewangles );
			idleYaw = qfalse;
		}

		if ( idleYaw &&
			 AICast_VisibleFromPos( cs->bs->origin, cs->entityNum,
									cs->combatGoalOrigin, cs->lastEnemy, qfalse ) ) {
			VectorSubtract( cs->combatGoalOrigin, cs->bs->origin, dir );
			dir[2] = 0;

			if ( VectorNormalize( dir ) > 4 ) {
				idleYaw = qfalse;
				vectoangles( dir, cs->ideal_viewangles );
			}
		}

		if ( idleYaw ) {
			if ( cs->nextIdleAngleChange < level.time ) {
				if ( cs->nextIdleAngleChange + 3000 < level.time ) {
					cs->idleYaw = AICast_GetRandomViewAngle( cs, 512 );

					if ( fabs( AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] ) ) < 45 ) {
						cs->nextIdleAngleChange = level.time + 1000 + rand() % 2500;
					} else {
						cs->nextIdleAngleChange = level.time + 500;
					}

					cs->idleYawChange = AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] );
					cs->ideal_viewangles[PITCH] = 0;
				}
			} else if ( cs->idleYawChange ) {
				cs->idleYawChange = AngleDelta( cs->idleYaw, cs->ideal_viewangles[YAW] );
				cs->ideal_viewangles[YAW] = AngleMod( cs->ideal_viewangles[YAW] +
													  cs->idleYawChange * cs->bs->thinktime );
			}
		}
	}

	if ( !cs->crouchHideFlag && cs->enemyNum < 0 ) {
		if ( cs->attackcrouch_time > level.time + 1000 ) {
			cs->attackcrouch_time = level.time + 1000;
		}
	}

	AICast_IdleReload( cs );

	return NULL;
}

/*
=============
AIFunc_BattleAmbushStart

Starts ambush behavior from cover.
Prepares crouch-attack state and clears cached predicted cover goal.
=============
*/
char *AIFunc_BattleAmbushStart( cast_state_t *cs ) {

	if ( !AICast_CanMoveWhileFiringWeapon( cs->weaponNum ) ) {
		// Heavy/static weapons should run without crouch attack
		cs->attackcrouch_time = 0;
	} else if ( cs->attackcrouch_time > level.time + 1000 ) {
		cs->attackcrouch_time = level.time + 1000;
	}

	// Continue crouch attack briefly if already active
	if ( cs->attributes[ATTACK_CROUCH] > 0.1 && cs->attackcrouch_time >= level.time ) {
		cs->attackcrouch_time = level.time + 1000;
	}

	if ( cs->attributes[ATTACK_CROUCH] > 0.1 ) {
		cs->aiFlags |= AIFL_ATTACK_CROUCH;
	} else {
		cs->aiFlags &= ~AIFL_ATTACK_CROUCH;
	}

	// Clear cached predicted cover goal
	cs->aiFlags &= ~AIFL_MISCFLAG1;

	cs->aifunc = AIFunc_BattleAmbush;
	return "AIFunc_BattleAmbush";
}


/*
============
AIFunc_BattleChase

Chases the current enemy to their last known position.
Handles retreat, reacquiring attack state, hunt/ambush transitions, grenade flush,
direct movement shortcuts, combat spots, rolls/flips, and chase speed control.
============
*/
char *AIFunc_BattleChase( cast_state_t *cs ) {
	const float chaseDist = 32;
	gentity_t *followent, *ent;
	bot_state_t *bs;
	gclient_t *client;
	cast_state_t *ocs;
	vec3_t destorg;
	qboolean moved;
	char *rval;
	float dist;

	ent = &g_entities[cs->entityNum];
	client = &level.clients[cs->entityNum];
	bs = cs->bs;
	moved = qfalse;

	// Danger overrides chase behavior
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// No cover found, but still enter danger avoidance
		}

		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}

	// Door marker behavior temporarily overrides chase
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	if ( cs->enemyNum < 0 ) {
		return AIFunc_IdleStart( cs );
	}

	// Retreat into cover if current combat situation requires it
	if ( AICast_WantToRetreat( cs ) ) {
		if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
									 cs->vislist[cs->enemyNum].visible_pos,
									 cs->takeCoverPos ) ) {
			cs->takeCoverTime = level.time + 2000 + rand() % 3000;
			return AIFunc_BattleTakeCoverStart( cs );
		}
	}

	ocs = AICast_GetCastState( cs->enemyNum );

	// Keep crouch attack alive while chasing if enabled
	if ( cs->aiFlags & AIFL_ATTACK_CROUCH ) {
		if ( cs->attackcrouch_time > level.time || cs->thinkFuncChangeTime < level.time - 1000 ) {
			cs->attackcrouch_time = level.time + 1000;
		}
	}

	followent = &g_entities[cs->enemyNum];

	// Enemy disappeared
	if ( !followent->inuse ) {
		if ( !( cs->enemyNum < MAX_CLIENTS &&
				( ( followent->client && followent->client->pers.connected == CON_CONNECTING ) ||
				  level.time < 3000 ) ) ) {
			cs->enemyNum = -1;
		}

		return AIFunc_IdleStart( cs );
	}

	// If enemy is visible and attackable, return to battle or fire while continuing chase
	AICast_ChooseWeapon( cs, qtrue );

	if ( AICast_EntityVisible( cs, cs->enemyNum, qtrue ) &&
		 AICast_CheckAttack( cs, cs->enemyNum, qfalse ) &&
		 cs->obstructingTime < level.time ) {
		if ( AICast_StopAndAttack( cs ) ) {
			rval = AIFunc_BattleStart( cs );
			if ( rval ) {
				return rval;
			}
		} else {
			AICast_ProcessAttack( cs );
		}

		AICast_ChooseWeapon( cs, qfalse );
	} else {
		AICast_ChooseWeapon( cs, qfalse );
	}

	// Chase clients to last visible position, otherwise chase entity origin directly
	if ( followent->client ) {
		VectorCopy( cs->vislist[cs->enemyNum].visible_pos, destorg );

		dist = Distance( destorg, cs->bs->origin );

		// Reached last known position, switch to hunt/ambush behavior
		if ( dist < chaseDist ) {
			if ( ocs->lastBattleHunted < level.time - 5000 ) {
				ocs->lastBattleHunted = level.time;
				return AIFunc_BattleHuntStart( cs );
			}

			if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
										 cs->vislist[cs->enemyNum].real_visible_pos,
										 cs->takeCoverPos ) ) {
				VectorCopy( cs->vislist[cs->enemyNum].real_visible_pos, cs->combatGoalOrigin );
				return AIFunc_BattleAmbushStart( cs );
			}

			VectorCopy( cs->bs->origin, cs->combatGoalOrigin );
			VectorCopy( cs->bs->origin, cs->takeCoverPos );
			return AIFunc_BattleAmbushStart( cs );
		}
	} else {
		VectorCopy( followent->s.pos.trBase, destorg );
		dist = Distance( cs->bs->origin, destorg );
	}

	// Do not chase enemies into do-not-enter brushes if already close and visible
	if ( AICast_EntityVisible( cs, cs->enemyNum, qtrue ) &&
		 VectorDistance( cs->bs->origin, destorg ) < 384 ) {
		if ( trap_PointContents( destorg, cs->enemyNum ) &
			 ( CONTENTS_DONOTENTER | CONTENTS_DONOTENTER_LARGE ) ) {
			return NULL;
		}
	}

	// Switch to another immediately attackable enemy if available
	numEnemies = AICast_ScanForEnemies( cs, enemies );

	if ( numEnemies == -1 ) {
		return NULL;
	} else if ( numEnemies == -2 ) {
		char *retval;

		retval = AIFunc_InspectFriendlyStart( cs, enemies[0] );
		if ( retval ) {
			return retval;
		}
	}

	AICast_ChooseWeapon( cs, qtrue );

	if ( numEnemies > 0 ) {
		int i;

		for ( i = 0; i < numEnemies; i++ ) {
			if ( enemies[i] != cs->enemyNum &&
				 AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
				cs->enemyNum = enemies[i];
				return AIFunc_BattleStart( cs );
			}
		}
	}

	AICast_ChooseWeapon( cs, qfalse );

	// Try grenade flush while chasing a hidden enemy
	if ( ( lastGrenadeFlush > level.time || lastGrenadeFlush < level.time - 5000 ) &&
		 cs->aiState >= AISTATE_COMBAT &&
		 cs->castScriptStatus.castScriptEventIndex < 0 &&
		 cs->startGrenadeFlushTime < level.time - 3000 &&
		 COM_BitCheck( cs->bs->cur_ps.weapons, WP_GRENADE_LAUNCHER ) &&
		 AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_LAUNCHER ) &&
		 cs->weaponFireTimes[WP_GRENADE_LAUNCHER] < level.time - (int)( 1000 + aicast_skillscale * 1000 ) &&
		 ( cs->weaponNum == WP_GRENADE_LAUNCHER || !( cs->castScriptStatus.scriptFlags & SFL_NOCHANGEWEAPON ) ) &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].visible_pos ) > 100 &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].visible_pos ) < 1400 ) {
		return AIFunc_GrenadeFlushStart( cs );
	}

	if ( ( lastGrenadeFlush > level.time || lastGrenadeFlush < level.time - 5000 ) &&
		 cs->aiState >= AISTATE_COMBAT &&
		 cs->castScriptStatus.castScriptEventIndex < 0 &&
		 cs->startGrenadeFlushTime < level.time - 3000 &&
		 COM_BitCheck( cs->bs->cur_ps.weapons, WP_GRENADE_PINEAPPLE ) &&
		 AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_PINEAPPLE ) &&
		 cs->weaponFireTimes[WP_GRENADE_PINEAPPLE] < level.time - (int)( 1000 + aicast_skillscale * 1000 ) &&
		 ( cs->weaponNum == WP_GRENADE_PINEAPPLE || !( cs->castScriptStatus.scriptFlags & SFL_NOCHANGEWEAPON ) ) &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].visible_pos ) > 100 &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].visible_pos ) < 1400 ) {
		return AIFunc_GrenadeFlushStart( cs );
	}

	// Flaming zombies can emit flame while chasing and facing target direction
	if ( ( cs->aiCharacter == AICHAR_ZOMBIE ||
		   cs->aiCharacter == AICHAR_ZOMBIE_SURV ||
		   cs->aiCharacter == AICHAR_ZOMBIE_FLAME ||
		   cs->aiCharacter == AICHAR_ZOMBIE_GHOST ) &&
		 IS_FLAMING_ZOMBIE( ent->s ) &&
		 fabs( cs->ideal_viewangles[YAW] - cs->viewangles[YAW] ) < 5 ) {
		if ( fabs( sin( ( level.time + cs->entityNum * 314 ) / 1000 ) *
				   cos( ( level.time + cs->entityNum * 267 ) / 979 ) ) < 0.5 ) {
			ent->s.time = level.time + 800;
		}
	}

	AICast_IdleReload( cs );

	if ( dist < chaseDist ) {
		return NULL;
	}

	// Try direct movement to visible enemy first
	if ( !moved &&
		 cs->leaderNum < 0 &&
		 ( cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE || ent->waterlevel > 1 ) &&
		 AICast_EntityVisible( cs, cs->enemyNum, qtrue ) ) {
		aicast_predictmove_t move;
		vec3_t dir;
		bot_input_t bi;
		usercmd_t ucmd;
		trace_t tr;

		trap_Trace( &tr, cs->bs->origin, NULL, NULL,
					followent->r.currentOrigin, cs->entityNum, ent->clipmask );

		if ( tr.entityNum == followent->s.number ) {
			VectorSubtract( followent->r.currentOrigin, cs->bs->origin, dir );
			VectorNormalize( dir );

			if ( !ent->waterlevel ) {
				dir[2] = 0;
			}

			trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi );
			VectorCopy( dir, bi.dir );
			bi.speed = 400;
			bi.actionflags = 0;

			AICast_InputToUserCommand( cs, &bi, &ucmd, bs->cur_ps.delta_angles );
			AICast_PredictMovement( cs, 5, 2.0, &move, &ucmd, cs->enemyNum );

			if ( move.stopevent == PREDICTSTOP_HITENT ) {
				trap_EA_Move( cs->entityNum, dir, 400 );

				// Very close stationary targets can cause corner sticking, so strafe randomly
				if ( followent->client &&
					 VectorLength( followent->client->ps.velocity ) < 50 &&
					 dist < 10 +
						 ( sqrt( cs->bs->cur_ps.maxs[0] * cs->bs->cur_ps.maxs[0] * 8.0 ) / 2.0 +
						   sqrt( followent->client->ps.maxs[0] * followent->client->ps.maxs[0] * 8.0 ) / 2.0 ) ) {
					trap_Trace( &tr, cs->bs->origin, cs->bs->cur_ps.mins, cs->bs->cur_ps.maxs,
								followent->r.currentOrigin, cs->entityNum, ent->clipmask );

					if ( tr.entityNum != followent->s.number ) {
						if ( level.time % 6000 < 2000 ) {
							trap_EA_MoveRight( cs->entityNum );
						} else {
							trap_EA_MoveLeft( cs->entityNum );
						}
					}
				}

				vectoangles( dir, cs->ideal_viewangles );
				cs->ideal_viewangles[2] *= 0.5;
				moved = qtrue;
			}
		}
	}

	// Find tactical attack spot if enemy is visible but not directly attackable
	if ( !moved &&
		 cs->weaponNum >= WP_LUGER &&
		 cs->weaponNum <= WP_AKIMBO &&
		 cs->attributes[TACTICAL] >= 0.1 ) {
		if ( cs->obstructingTime > level.time ) {
			AICast_MoveToPos( cs, cs->obstructingPos, -1 );
			moved = qtrue;
		}

		if ( cs->leaderNum >= 0 ) {
			if ( cs->combatGoalTime < level.time && cs->attackSpotTime < level.time ) {
				cs->attackSpotTime = level.time + 500 + rand() % 500;

				if ( trap_AAS_FindAttackSpotWithinRange( cs->entityNum, cs->leaderNum,
														 cs->enemyNum, MAX_LEADER_DIST,
														 AICAST_TFL_DEFAULT, cs->combatGoalOrigin ) ) {
					cs->combatGoalTime = level.time + 2000;
				}
			}

			if ( cs->combatGoalTime > level.time ) {
				if ( Distance( cs->combatGoalOrigin, g_entities[cs->leaderNum].r.currentOrigin ) > MAX_LEADER_DIST ) {
					cs->combatGoalTime = 0;
				} else {
					moveresult = AICast_MoveToPos( cs, cs->combatGoalOrigin, -1 );

					if ( moveresult && moveresult->failure ) {
						cs->combatGoalTime = 0;
					} else {
						moved = qtrue;

						if ( Distance( cs->bs->origin, cs->combatGoalOrigin ) < 32 ) {
							cs->combatGoalTime = 0;
						}
					}
				}
			} else if ( Distance( cs->bs->origin, g_entities[cs->leaderNum].r.currentOrigin ) > MAX_LEADER_DIST ) {
				return AIFunc_ChaseGoalStart( cs, cs->leaderNum, AICAST_LEADERDIST_MAX, qtrue );
			}
		} else {
			if ( cs->combatGoalTime < level.time && cs->attackSpotTime < level.time ) {
				cs->attackSpotTime = level.time + 500 + rand() % 500;

				if ( trap_AAS_FindAttackSpotWithinRange( cs->entityNum, cs->entityNum,
														 cs->enemyNum, 512,
														 AICAST_TFL_DEFAULT, cs->combatGoalOrigin ) ) {
					cs->combatGoalTime = level.time + 2000;
				}
			}

			if ( cs->combatGoalTime > level.time ) {
				moveresult = AICast_MoveToPos( cs, cs->combatGoalOrigin, -1 );

				if ( moveresult && moveresult->failure ) {
					cs->combatGoalTime = 0;
				} else {
					moved = qtrue;

					if ( Distance( cs->bs->origin, cs->combatGoalOrigin ) < 32 ) {
						cs->combatGoalTime = 0;
						cs->attackSpotTime = level.time + 12000;
					}
				}
			}
		}
	}

	// Fallback: path directly to enemy's last known position
	if ( !moved && cs->leaderNum < 0 ) {
		moveresult = AICast_MoveToPos( cs, destorg, cs->enemyNum );

		if ( moveresult && moveresult->failure ) {
			if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
										 cs->vislist[cs->enemyNum].real_visible_pos,
										 cs->takeCoverPos ) ) {
				VectorCopy( cs->bs->origin, cs->combatGoalOrigin );
				return AIFunc_BattleAmbushStart( cs );
			}

			// Help lopers escape bad spots by allowing attack2 again
			if ( cs->aiCharacter == AICHAR_LOPER || cs->aiCharacter == AICHAR_LOPER_SPECIAL ) {
				cs->weaponFireTimes[WP_MONSTER_ATTACK2] = 0;
			}

			if ( cs->bs->areanum ) {
				VectorCopy( cs->bs->origin, cs->combatGoalOrigin );
				VectorCopy( cs->bs->origin, cs->takeCoverPos );
				return AIFunc_BattleAmbushStart( cs );
			}
		}
	}

	// Slow near chase target
	cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, chaseDist );

	// Accelerate into chase over a short window
	#define BATTLE_CHASE_ACCEL_TIME 300

	if ( cs->attributes[RUNNING_SPEED] > 170 &&
		 level.time < cs->startBattleChaseTime + BATTLE_CHASE_ACCEL_TIME ) {
		float ideal;

		ideal = 0.5 + 0.5 *
			( 1.0 - ( (float)( ( cs->startBattleChaseTime + BATTLE_CHASE_ACCEL_TIME ) - level.time ) /
					  BATTLE_CHASE_ACCEL_TIME ) );

		if ( ideal < cs->speedScale ) {
			cs->speedScale = ideal;
		}
	}

	// Predict next movement for roll/flip opportunities and cautious slowdown
	{
		float simTime = 1.5;
		aicast_predictmove_t move;
		float moveDist;
		vec3_t vec;

		AICast_PredictMovement( cs, 1, simTime, &move, &cs->lastucmd, cs->enemyNum );

		VectorSubtract( move.endpos, cs->bs->origin, vec );
		moveDist = VectorNormalize( vec );

		if ( cs->attributes[TACTICAL] > 0.85 &&
			 ( cs->aiFlags & AIFL_ROLL_ANIM ) &&
			 !client->ps.torsoTimer &&
			 !client->ps.legsTimer &&
			 cs->lastRollMove < level.time - 800 &&
			 move.numtouch == 0 &&
			 moveDist > simTime * cs->attributes[RUNNING_SPEED] * 0.98 &&
			 move.groundEntityNum == ENTITYNUM_WORLD &&
			 AICast_CheckAttackAtPos( cs->entityNum, cs->enemyNum, move.endpos,
									  cs->attackcrouch_time > level.time, qfalse ) ) {
			cs->takeCoverTime = 0;
			return AIFunc_BattleRollStart( cs, vec );
		} else if ( ( cs->aiFlags & AIFL_FLIP_ANIM ) &&
					cs->lastRollMove < level.time - 800 &&
					!client->ps.torsoTimer &&
					cs->castScriptStatus.castScriptEventIndex < 0 &&
					move.numtouch == 0 &&
					moveDist > simTime * cs->attributes[RUNNING_SPEED] * 0.9 &&
					move.groundEntityNum == ENTITYNUM_WORLD &&
					cs->attackcrouch_time < level.time ) {
			int destarea, simarea, starttravel, simtravel;

			destarea = BotPointAreaNum( destorg );
			simarea = BotPointAreaNum( move.endpos );

			starttravel = trap_AAS_AreaTravelTimeToGoalArea( cs->bs->areanum,
															 cs->bs->origin,
															 destarea,
															 cs->travelflags );

			simtravel = trap_AAS_AreaTravelTimeToGoalArea( simarea,
														   move.endpos,
														   destarea,
														   cs->travelflags );

			if ( simtravel < starttravel ) {
				return AIFunc_FlipMoveStart( cs, vec );
			}
		} else if ( !( cs->aiFlags & AIFL_WALKFORWARD ) &&
					VectorDistance( cs->bs->origin, g_entities[cs->enemyNum].s.pos.trBase ) <
					AICast_WeaponRange( cs, cs->weaponNum ) &&
					cs->obstructingTime < level.time &&
					cs->attributes[TACTICAL] > 0.1 &&
					AICast_VisibleFromPos( cs->vislist[cs->enemyNum].visible_pos,
											cs->enemyNum, move.endpos,
											cs->entityNum, qfalse ) ) {
			cs->attackcrouch_time = 0;

			if ( cs->bs->cur_ps.viewheight > cs->bs->cur_ps.crouchViewHeight &&
				 cs->attributes[RUNNING_SPEED] * cs->speedScale > 120 ) {
				cs->speedScale = 120.0 * cs->attributes[RUNNING_SPEED];
			}

			if ( cs->attributes[RUNNING_SPEED] > 140 ) {
				AICast_AimAtEnemy( cs );
			}
		}
	}

	AICast_IdleReload( cs );

	return NULL;
}

/*
============
AIFunc_BattleChaseStart

Starts combat chase behavior.
Resets chase state, shortens cover delay, and enables crouch-attack if supported.
============
*/
char *AIFunc_BattleChaseStart( cast_state_t *cs ) {
	cs->startBattleChaseTime = level.time;
	cs->combatGoalTime = 0;
	cs->battleChaseMarker = -99;
	cs->battleChaseMarkerDir = 1;

	// If cover was recently delayed, allow it again soon during chase
	if ( cs->takeCoverTime > level.time ) {
		cs->takeCoverTime = level.time + 1500 + rand() % 500;
	}

	if ( cs->attributes[ATTACK_CROUCH] > 0.1 ) {
		cs->aiFlags |= AIFL_ATTACK_CROUCH;
	} else {
		cs->aiFlags &= ~AIFL_ATTACK_CROUCH;
	}

	cs->aifunc = AIFunc_BattleChase;
	return "AIFunc_BattleChase";
}

/*
============
AIFunc_AvoidDanger()
============
*/
char *AIFunc_AvoidDanger( cast_state_t *cs ) {
	bot_state_t *bs;
	vec3_t destorg, vec;
	float dist;
	int enemies[MAX_CLIENTS], numEnemies, i;
	qboolean shouldAttack;
	gentity_t *ent;
	trace_t tr;
	vec3_t end;
	gentity_t *danger;

	// we need to move towards it
	bs = cs->bs;
	ent = g_entities + cs->entityNum;
	//
	// TODO: if we are on fire, play the correct torso animation
	if ( ent->s.onFireEnd > level.time ) {
		// set the animation, and a short timer, but long enough to last until the next frame
		//if (g_cheats.integer) G_Printf( "TODO: torso onfire animation\n" );
	}
	//
	// is the danger gone?
	if ( cs->dangerEntityValidTime < level.time ) {
		return AIFunc_DefaultStart( cs );
	}
	//
	// special case: if it's a grenade, and it's going to land near us with some time left before it
	// explodes, try and kick it back
	//
	danger = &g_entities[cs->dangerEntity];
	if ( ent->s.onFireEnd < level.time ) {
		if ( ( danger->s.weapon == WP_GRENADE_LAUNCHER || danger->s.weapon == WP_GRENADE_PINEAPPLE ) &&
			 ( danger->nextthink - level.time > 1500 ) &&
			 ( level.lastGrenadeKick < level.time - 3000 ) &&
			 ( cs->aiFlags & AIFL_CATCH_GRENADE ) &&
			 !( danger->flags & FL_AI_GRENADE_KICK ) ) {
			// if it was thrown by a friend of ours, leave it alone
			if ( !AICast_SameTeam( cs, danger->r.ownerNum ) ) {
				if ( G_PredictMissile( danger, danger->nextthink - level.time, cs->takeCoverPos, qfalse ) ) {
					// make sure it's a valid position, and drop it down to the ground
					cs->takeCoverPos[2] += -ent->r.mins[2] + 12;
					VectorCopy( cs->takeCoverPos, end );
					end[2] -= 90;
					trap_Trace( &tr, cs->takeCoverPos, ent->r.mins, ent->r.maxs, end, cs->entityNum, MASK_SOLID );
					VectorCopy( tr.endpos, cs->takeCoverPos );
					if ( !tr.startsolid && ( tr.fraction < 1.0 ) &&
						 VectorDistance( cs->bs->origin, cs->takeCoverPos ) < cs->attributes[RUNNING_SPEED] * 0.0004 * ( danger->nextthink - level.time - 2000 ) ) {

						// check for a clear path to the grenade
						trap_Trace( &tr, cs->bs->origin, ent->r.mins, ent->r.maxs, cs->takeCoverPos, cs->entityNum, MASK_SOLID );

						if ( VectorDistance( tr.endpos, cs->takeCoverPos ) < 8 ) {
							danger->flags |= FL_AI_GRENADE_KICK;
							ent->flags |= FL_AI_GRENADE_KICK;
							level.lastGrenadeKick = level.time;
							return AIFunc_GrenadeKickStart( cs );   // we should decide our course of action within this start function (dive or return grenade)
						}
					}
				}
			}
			// if it's really close to us, and we're heading for it, may as well pick it up
			if ( VectorLength( danger->s.pos.trDelta ) < 10 && VectorDistance( danger->r.currentOrigin, cs->bs->origin ) < 128 &&
				 ( level.lastGrenadeKick < level.time - 3000 ) &&
				 ( cs->aiFlags & AIFL_CATCH_GRENADE ) ) {
				vec3_t vec;
				VectorSubtract( danger->r.currentOrigin, cs->bs->origin, vec );
				if ( DotProduct( vec, cs->bs->velocity ) > 0 ) {
					danger->flags |= FL_AI_GRENADE_KICK;
					ent->flags |= FL_AI_GRENADE_KICK;
					level.lastGrenadeKick = level.time;
					return AIFunc_GrenadeKickStart( cs );   // we should decide our course of action within this start function (dive or return grenade)
				}
			}
		}
	}
	//
	if ( g_entities[cs->dangerEntity].inuse ) {
		// is our current destination still safe?
		if ( Distance( cs->dangerEntityPos, cs->takeCoverPos ) < cs->dangerDist &&
			 AICast_VisibleFromPos( cs->dangerEntityPos, cs->dangerEntity, cs->takeCoverPos, cs->entityNum, qfalse ) ) {
			//G_Printf("current coverPos is dangerous, looking for a better place..\n" );
			if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
				// just run away from it ???
			}
		}
	} else {
		// the entity isn't here anymore, so stop hiding
		cs->dangerEntityValidTime = -1;
		return AIFunc_DefaultStart( cs );
	}
	//
	VectorCopy( cs->takeCoverPos, destorg );
	VectorSubtract( destorg, cs->bs->origin, vec );
	vec[2] *= 0.2;
	dist = VectorLength( vec );
	//
	shouldAttack = qfalse;
	if ( ent->s.onFireEnd < level.time ) {
		// look for things we should attack
		numEnemies = AICast_ScanForEnemies( cs, enemies );
		if ( numEnemies > 0 ) {
			// default to the first known enemy, overwrite if we find a clearer shot
			cs->enemyNum = enemies[0];
			//
			for ( i = 0; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) || AICast_CheckAttack( AICast_GetCastState( enemies[i] ), cs->entityNum, qfalse ) ) {
					cs->enemyNum = enemies[i];
					shouldAttack = qtrue;
					break;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}
		}
	}
	//
	// if we are now safe from the danger, stop running away
	if ( cs->dangerEntity >= MAX_CLIENTS && Distance( cs->dangerEntityPos, cs->bs->origin ) > cs->dangerDist * 1.5 ) {
		// don't move, wait for danger to pass
	} else
	// are we close enough to the goal?
	if ( dist > 8 ) {
		moveresult = AICast_MoveToPos( cs, destorg, -1 );
		if ( moveresult ) {
			//if the movement failed
			if ( moveresult->failure || moveresult->blocked ) {
				//reset the avoid reach, otherwise bot is stuck in current area
				trap_BotResetAvoidReach( bs->ms );
				if ( g_entities[cs->dangerEntity].inuse ) {
					// find a better spot?
					AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos );
				} else {
					VectorCopy( cs->bs->origin, cs->takeCoverPos );
				}
			}
		}
		if ( ent->s.onFireEnd < level.time ) {
			// slow down real close to the goal, so we don't go passed it
			cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, 0 );
		}
		//
		// pretend we can still see them while we run to our hide pos, this way they are less likely
		// to forget about their enemy once they get there
		if ( ent->s.onFireEnd < level.time && cs->enemyNum >= 0 && cs->vislist[cs->enemyNum].real_visible_timestamp && ( cs->vislist[cs->enemyNum].real_visible_timestamp > level.time - 10000 ) ) {
			AICast_UpdateVisibility( &g_entities[cs->entityNum], &g_entities[cs->enemyNum], qfalse, cs->vislist[cs->enemyNum].real_visible_timestamp == cs->vislist[cs->enemyNum].lastcheck_timestamp );
		}

	} else {
		// set our origin as the hidepos, that way if we are still in danger, we should find a better spot
		VectorCopy( cs->bs->origin, cs->takeCoverPos );
		// check for a movement we should be making
		if ( cs->obstructingTime > level.time ) {
			AICast_MoveToPos( cs, cs->obstructingPos, -1 );
		}

		// if we are on fire, never stop
		if ( ent->s.onFireEnd > level.time ) {
			VectorCopy( cs->bs->origin, cs->dangerEntityPos );
			cs->dangerEntityValidTime = level.time + 10000;
		}

	}
	//
	// if we should be attacking something on our way
	if ( shouldAttack ) {
		//attack the enemy if possible
		AICast_ProcessAttack( cs );
	} else { //if (dist < 48) {
		// if we've recently been in a fight, look towards the enemy
		if ( cs->lastEnemy >= 0 ) {
			// if we only just recently saw them, face that direction
			if ( cs->vislist[cs->lastEnemy].visible_timestamp > ( level.time - 20000 ) ) {
				vec3_t dir;
				//
				VectorSubtract( cs->vislist[cs->lastEnemy].visible_pos, cs->bs->origin, dir );
				VectorNormalize( dir );
				vectoangles( dir, cs->ideal_viewangles );
			}
		}
	}

	// reload?
	AICast_IdleReload( cs );

	return NULL;
}

/*
============
AIFunc_AvoidDangerStart()
============
*/
char *AIFunc_AvoidDangerStart( cast_state_t *cs ) {
	//
	//if (!AICast_CanMoveWhileFiringWeapon( cs->weaponNum )) {
	// always run to the cover point
	cs->attackcrouch_time = 0;
	//}
	// make sure we move if we are allowed (scripting will overwrite this if necessary)
	cs->castScriptStatus.scriptNoMoveTime = 0;
	// resume following once danger has gone
	cs->castScriptStatus.scriptGotoId = -1;
	//
	cs->aifunc = AIFunc_AvoidDanger;
	return "AIFunc_AvoidDanger";
}

/*
============
AIFunc_BattleTakeCover

Moves to a cover position, validates that cover is still useful,
and optionally attacks while retreating.
============
*/
char *AIFunc_BattleTakeCover( cast_state_t *cs ) {
	bot_state_t *bs;
	vec3_t destorg, vec;
	float dist, moveDist;
	int enemies[MAX_CLIENTS], numEnemies, i;
	qboolean shouldAttack;
	aicast_predictmove_t move;
	gclient_t *client;
	bot_moveresult_t *moveresult;

	client = &level.clients[cs->entityNum];

	// Danger overrides normal cover behavior
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// No cover found, but still enter danger avoidance
		}

		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}

	// Door marker behavior temporarily overrides cover movement
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	bs = cs->bs;

	if ( cs->enemyNum < 0 ) {
		return AIFunc_IdleStart( cs );
	}

	// Calculate distance to cover point
	if ( VectorLength( cs->takeCoverPos ) < 1 ) {
		dist = 0;
	} else {
		VectorCopy( cs->takeCoverPos, destorg );
		VectorSubtract( destorg, cs->bs->origin, vec );
		vec[2] *= 0.2;
		dist = VectorLength( vec );
	}

	// Scan for higher-priority perception events or enemies to shoot while moving
	shouldAttack = qfalse;
	numEnemies = AICast_ScanForEnemies( cs, enemies );

	if ( numEnemies == -1 ) {
		return NULL;
	} else if ( numEnemies == -2 ) {
		char *retval;

		retval = AIFunc_InspectFriendlyStart( cs, enemies[0] );
		if ( retval ) {
			return retval;
		}
	} else if ( numEnemies == -3 ) {
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectBulletImpactStart( cs );
		}
	} else if ( numEnemies == -4 ) {
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
		}
	} else if ( numEnemies > 0 ) {
		if ( AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
			cs->enemyNum = enemies[0];

			for ( i = 0; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ||
					 AICast_CheckAttack( AICast_GetCastState( enemies[i] ), cs->entityNum, qfalse ) ||
					 AICast_EntityVisible( AICast_GetCastState( enemies[i] ), cs->entityNum, qtrue ) ) {
					if ( ( cs->aiFlags & AIFL_WALKFORWARD ) || dist <= 12 ) {
						cs->enemyNum = enemies[i];
						return AIFunc_BattleStart( cs );
					}

					shouldAttack = qtrue;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}
		} else {
			AICast_ChooseWeapon( cs, qfalse );

			if ( dist <= 12 && !AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
				if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
											 cs->vislist[cs->enemyNum].visible_pos,
											 cs->takeCoverPos ) ) {
					cs->takeCoverTime = level.time + 2000 + rand() % 3000;
				}
			}
		}
	}

	// Revalidate cover. If the cover point is exposed, try to find a better one.
	if ( AICast_VisibleFromPos( cs->vislist[cs->enemyNum].visible_pos,
								cs->enemyNum, cs->takeCoverPos,
								bs->entitynum, qfalse ) ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->enemyNum,
									  cs->vislist[cs->enemyNum].visible_pos,
									  cs->takeCoverPos ) ) {
			return AIFunc_BattleStart( cs );
		}

		VectorCopy( cs->takeCoverPos, destorg );
		VectorSubtract( destorg, cs->bs->origin, vec );
		vec[2] *= 0.2;
		dist = VectorLength( vec );
	} else if ( dist < 8 ) {
		if ( AICast_EntityVisible( AICast_GetCastState( cs->enemyNum ), cs->entityNum, qtrue ) ||
			 AICast_CheckAttack( AICast_GetCastState( cs->enemyNum ), cs->entityNum, qfalse ) ) {
			if ( !AICast_GetTakeCoverPos( cs, cs->enemyNum,
										  cs->vislist[cs->enemyNum].visible_pos,
										  cs->takeCoverPos ) ) {
				return AIFunc_BattleStart( cs );
			}

			VectorCopy( cs->takeCoverPos, destorg );
			VectorSubtract( destorg, cs->bs->origin, vec );
			vec[2] *= 0.2;
			dist = VectorLength( vec );
		}
	}

	memset( &move, 0, sizeof( move ) );

	// Move toward cover if we have not reached it yet
	if ( VectorLength( cs->takeCoverPos ) > 1 && dist > 8 ) {
		const float simTime = 1.5;
		float enemyDist;

		cs->takeCoverTime = level.time + 2000 + rand() % 2000;

		moveresult = AICast_MoveToPos( cs, destorg, -1 );
		if ( moveresult ) {
			if ( moveresult->failure ) {
				trap_BotResetAvoidReach( bs->ms );
				VectorClear( cs->takeCoverPos );
				dist = 0;
			}

			if ( moveresult->blocked ) {
				VectorClear( cs->takeCoverPos );
				dist = 0;
			}
		}

		// Predict whether cover movement will collide or move us closer to a nearby enemy
		AICast_PredictMovement( cs, 1, simTime, &move, &cs->lastucmd, -1 );

		enemyDist = Distance( cs->bs->origin, g_entities[cs->enemyNum].s.pos.trBase );
		VectorSubtract( move.endpos, cs->bs->origin, vec );
		moveDist = VectorNormalize( vec );

		if ( ( move.numtouch && move.touchents[0] < aicast_maxclients ) ||
			 ( enemyDist < 128 &&
			   ( enemyDist - 1 ) > Distance( move.endpos, g_entities[cs->enemyNum].s.pos.trBase ) ) ) {
			VectorClear( cs->takeCoverPos );
		} else if ( ( cs->aiFlags & AIFL_DIVE_ANIM ) &&
					!client->ps.torsoTimer &&
					cs->castScriptStatus.castScriptEventIndex < 0 &&
					cs->lastRollMove < level.time - 800 &&
					move.numtouch == 0 &&
					moveDist > simTime * cs->attributes[RUNNING_SPEED] * 0.98 &&
					move.groundEntityNum == ENTITYNUM_WORLD &&
					shouldAttack &&
					!AICast_VisibleFromPos( g_entities[cs->enemyNum].s.pos.trBase,
											 cs->enemyNum, move.endpos,
											 cs->entityNum, qfalse ) ) {
			VectorClear( cs->takeCoverPos );
			return AIFunc_BattleDiveStart( cs, vec );
		} else if ( dist < 64 ) {
			cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, 0 );
		}

		// Cache predicted cover endpoint if it is already hidden and valid
		if ( !( cs->aiFlags & AIFL_MISCFLAG1 ) &&
			 !AICast_VisibleFromPos( cs->vislist[cs->enemyNum].real_visible_pos,
									 cs->enemyNum, move.endpos,
									 cs->entityNum, qfalse ) &&
			 !AICast_VisibleFromPos( cs->vislist[cs->enemyNum].real_visible_pos,
									 cs->enemyNum, cs->bs->origin,
									 cs->entityNum, qfalse ) &&
			 trap_AAS_PointAreaNum( move.endpos ) ) {
			VectorCopy( move.endpos, cs->takeCoverPos );
			cs->aiFlags |= AIFL_MISCFLAG1;
		}

		// Use flip movement if it improves travel toward cover
		if ( ( cs->aiFlags & AIFL_FLIP_ANIM ) &&
			 cs->lastRollMove < level.time - 800 &&
			 !client->ps.torsoTimer &&
			 cs->castScriptStatus.castScriptEventIndex < 0 &&
			 move.numtouch == 0 &&
			 moveDist > simTime * cs->attributes[RUNNING_SPEED] * 0.9 &&
			 move.groundEntityNum == ENTITYNUM_WORLD &&
			 cs->attackcrouch_time < level.time ) {
			int destarea, simarea, starttravel, simtravel;

			destarea = BotPointAreaNum( destorg );
			simarea = BotPointAreaNum( move.endpos );

			starttravel = trap_AAS_AreaTravelTimeToGoalArea( cs->bs->areanum,
															 cs->bs->origin,
															 destarea,
															 cs->travelflags );

			simtravel = trap_AAS_AreaTravelTimeToGoalArea( simarea,
														   move.endpos,
														   destarea,
														   cs->travelflags );

			if ( simtravel < starttravel ) {
				return AIFunc_FlipMoveStart( cs, vec );
			}
		}

		// Crouch while retreating to cover when appropriate
		if ( cs->crouchHideFlag ||
			 ( cs->thinkFuncChangeTime < level.time - 2000 &&
			   ( cs->aiFlags & AIFL_ATTACK_CROUCH ) ) ) {
			cs->attackcrouch_time = level.time + 1000;
		}
	} else {
		// Already at cover, wait there until cover time expires
		if ( level.time > cs->takeCoverTime ) {
			return AIFunc_DefaultStart( cs );
		}

		if ( cs->obstructingTime > level.time ) {
			VectorClear( cs->takeCoverPos );
			AICast_MoveToPos( cs, cs->obstructingPos, -1 );
		}

		if ( shouldAttack ) {
			return AIFunc_BattleStart( cs );
		} else if ( numEnemies ) {
			// Attack if enemy is reloading and we can use our weapon
			if ( g_entities[cs->entityNum].client->ps.weaponDelay < 100 &&
				 g_entities[cs->enemyNum].client->ps.weaponDelay > 1100 ) {
				if ( AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) &&
					 AICast_WeaponUsable( cs, cs->weaponNum ) ) {
					return AIFunc_BattleStart( cs );
				}
			}
		} else if ( cs->leaderNum >= 0 &&
					Distance( cs->bs->origin, g_entities[cs->leaderNum].r.currentOrigin ) > MAX_LEADER_DIST ) {
			if ( level.time > cs->takeCoverTime ) {
				return AIFunc_ChaseGoalStart( cs, cs->leaderNum, AICAST_LEADERDIST_MAX, qtrue );
			}
		}

		// Crouch while hiding
		if ( cs->attributes[ATTACK_CROUCH] > 0.1 || cs->crouchHideFlag ) {
			cs->attackcrouch_time = level.time + 1000;
		}
	}

	if ( shouldAttack ) {
		vec3_t enemyDir, moveDir;
		float enemyRange;

		// Abort cover movement if we are moving into a close enemy
		VectorSubtract( g_entities[cs->enemyNum].client->ps.origin, bs->origin, enemyDir );
		enemyRange = VectorNormalize( enemyDir );

		if ( enemyRange < 256 ) {
			VectorCopy( bs->velocity, vec );
			vec[2] = 0;

			if ( VectorNormalize2( vec, moveDir ) > 20 ) {
				if ( DotProduct( moveDir, vec ) > 0.4 ) {
					return AIFunc_BattleStart( cs );
				}
			}
		}

		if ( cs->thinkFuncChangeTime < level.time - 1000 &&
			 AICast_VisibleFromPos( g_entities[cs->enemyNum].client->ps.origin,
									 cs->enemyNum, cs->takeCoverPos,
									 bs->entitynum, qfalse ) ) {
			return AIFunc_BattleStart( cs );
		}

		if ( !move.numtouch &&
			 cs->thinkFuncChangeTime < level.time - 2000 &&
			 enemyRange > 128 &&
			 cs->attributes[TACTICAL] > 0.4 &&
			 cs->attributes[ATTACK_CROUCH] > 0.1 &&
			 cs->attackcrouch_time >= level.time ) {
			cs->attackcrouch_time = level.time + 1000;
		}

		AICast_ProcessAttack( cs );
	} else {
		// Face enemy/last enemy while hiding, then idle reload
		if ( cs->enemyNum >= 0 ) {
			AICast_AimAtEnemy( cs );
		} else if ( cs->lastEnemy >= 0 ) {
			if ( VectorLength( cs->bs->cur_ps.velocity ) < 50 ) {
				vec3_t dir;

				VectorSubtract( cs->vislist[cs->lastEnemy].visible_pos, cs->bs->origin, dir );
				VectorNormalize( dir );
				vectoangles( dir, cs->ideal_viewangles );
			}
		}

		AICast_IdleReload( cs );
	}

	return NULL;
}

/*
============
AIFunc_BattleTakeCoverStart

Starts combat cover movement.
Sets crouch-attack behavior based on whether the current weapon can fire while moving.
============
*/
char *AIFunc_BattleTakeCoverStart( cast_state_t *cs ) {
#ifdef DEBUG
//	if ( cs->attributes[AGGRESSION] >= 1.0 ) {
//		AICast_Printf( 0, "AI taking cover with full aggression!\n" );
//	}
#endif

	if ( !AICast_CanMoveWhileFiringWeapon( cs->weaponNum ) ) {
		// Heavy/static weapons should run to cover without crouch attack
		cs->attackcrouch_time = 0;
		cs->aiFlags &= ~AIFL_ATTACK_CROUCH;
	} else {
		// Mobile weapons may crouch while falling back
		if ( cs->attributes[ATTACK_CROUCH] > 0.1 ) {
			cs->aiFlags |= AIFL_ATTACK_CROUCH;
		} else {
			cs->aiFlags &= ~AIFL_ATTACK_CROUCH;
		}

		cs->attackcrouch_time = 0;
	}

	// Clear cached predicted cover goal
	cs->aiFlags &= ~AIFL_MISCFLAG1;

	cs->aifunc = AIFunc_BattleTakeCover;
	return "AIFunc_BattleTakeCover";
}

/*
============
AIFunc_GrenadeFlush()
============
*/
char *AIFunc_GrenadeFlush( cast_state_t *cs ) {
	vec3_t dir;
	gentity_t   *followent, *grenade, *ent;
	bot_state_t *bs;
	vec3_t destorg, endPos;
	qboolean moved = qfalse;
	int hitclient;
//	qboolean attacked = qfalse;
	float dist, oldyaw;
	int grenadeType;

	bs = cs->bs;
	ent = &g_entities[cs->entityNum];
	//
	// if we are throwing the grenade, keep holding down fire

	if ( AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_LAUNCHER ) ) {
		grenadeType = WP_GRENADE_LAUNCHER;
	} else if ( AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_PINEAPPLE ) ) {
		grenadeType = WP_GRENADE_PINEAPPLE;
	} else { // not enough ammo, abort
		return AIFunc_DefaultStart( cs );
	}

	// (SA) probably read the fweapon from t
	if ( cs->grenadeFlushFiring ) {
		// are we still moving?
		if ( VectorLength( cs->bs->cur_ps.velocity ) ) {
			// keep waiting
			// pause for a bit, so the grenade comes out correctly
			cs->lockViewAnglesTime = level.time + 1200;
			if ( cs->castScriptStatus.scriptNoMoveTime < level.time + 1200 ) {
				cs->castScriptStatus.scriptNoMoveTime = level.time + 1200;
			}
			return NULL;
		}
		if ( cs->weaponFireTimes[cs->grenadeFlushFiring] < cs->thinkFuncChangeTime ) {
			// have we switched weapons?
			if ( cs->weaponNum != cs->grenadeFlushFiring ) {
				// damn
				hitclient = -1;
			} else {
				// keep checking it's ok
				CalcMuzzlePoints( ent, grenadeType );
				// fire a dummy grenade
				grenade = weapon_grenadelauncher_fire( ent, WP_GRENADE_LAUNCHER );
				// check to see what will happen
				hitclient = AICast_SafeMissileFire( grenade, grenade->nextthink - level.time, cs->enemyNum, destorg, cs->entityNum, endPos );
				// kill the grenade
				G_FreeEntity( grenade );
				if ( hitclient != 1 ) {   // it wont hit them, abort
					hitclient = -1;     // a miss is as bad as a friendly kill
				}
			}
			if ( hitclient == -1 ) {  // doh
				//G_Printf("aborted grenade\n");
				cs->castScriptStatus.scriptNoMoveTime = 0;
				cs->lockViewAnglesTime = 0;
				AICast_ChooseWeapon( cs, qfalse );
				return AIFunc_DefaultStart( cs );
			}
			if ( !cs->bs->cur_ps.grenadeTimeLeft ) {
				// hold fire button down
				trap_EA_Attack( bs->client );
				cs->bFlags |= BFL_ATTACKED;
			}
			cs->lockViewAnglesTime = level.time + 500;
			return NULL;
		}
		// the grenade/pineapple has been released!
		cs->lockViewAnglesTime = -1;
		cs->startGrenadeFlushTime = level.time + 2000 + rand() % 2000;    // dont throw one again for a bit
		return AIFunc_DefaultStart( cs );
	}
	//
	// do we need to avoid a danger?
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// shit??
		}
		// go to a position that cannot be seen from the dangerPos
		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}
	//
	// are we waiting for a door?
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}
	//
	if ( cs->weaponNum && ( cs->castScriptStatus.scriptFlags & SFL_NOCHANGEWEAPON ) ) {
		return AIFunc_IdleStart( cs );
	}
	//
	if ( cs->enemyNum < 0 ) {
		return AIFunc_IdleStart( cs );
	}
	//
	// if we have started a script, abort the grenade flush
	if ( cs->castScriptStatus.castScriptEventIndex >= 0 ) {
		return AIFunc_IdleStart( cs );
	}
	// trying for too long?
	if ( cs->startGrenadeFlushTime < level.time - 4000 ) {
		cs->startGrenadeFlushTime = level.time;
		return AIFunc_IdleStart( cs );
	}
	//
	followent = &g_entities[cs->enemyNum];
	//
	// if the entity is not ready yet
	if ( !followent->inuse ) {
		// if it's a connecting client, wait
		if ( !(   ( cs->enemyNum < MAX_CLIENTS )
				  && (   ( followent->client && followent->client->pers.connected == CON_CONNECTING )
						 || ( level.time < 3000 ) ) ) ) { // they don't exist anymore, stop attacking
			cs->enemyNum = -1;
		}
		return AIFunc_IdleStart( cs );
	}
	//
	// if we can see them, go back to an attack state after some time
	if (    AICast_CheckAttack( cs, cs->enemyNum, qfalse )
			&&  cs->obstructingTime < level.time ) { // give us some time to throw the grenade, otherwise go back to attack state
//		if ( ( cs->grenadeFlushEndTime > 0 && cs->grenadeFlushEndTime < level.time ) ) {
//			G_Printf( "aborting, enemy is attackable\n" );
			return AIFunc_BattleStart( cs );
//		} else if ( cs->grenadeFlushEndTime < 0 ) {
//			cs->grenadeFlushEndTime = level.time + 1500;
//		}
		//attack the enemy if possible
//		AICast_ProcessAttack( cs );
//		attacked = qtrue;
	} else {
		// not visible, go to their previously visible position
		if ( !cs->vislist[cs->enemyNum].visible_timestamp || Distance( bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) < 16 ) {
			// we're done attacking, go back to default state, which in turn will recall previous state
			//
			// face the direction they currently are from this position (bit of a hack, but it looks best)
			VectorSubtract( g_entities[cs->enemyNum].client->ps.origin, cs->vislist[cs->enemyNum].visible_pos, dir );
			vectoangles( dir, cs->ideal_viewangles );
			//
			//G_Printf("aborting, reached visible pos\n");
			return AIFunc_DefaultStart( cs );
		}
	}

	// is there someone else we can go for instead?
	numEnemies = AICast_ScanForEnemies( cs, enemies );
	if ( numEnemies == -1 ) { // query mode
		return NULL;
	} else if ( numEnemies == -2 )     { // inspection may be required
		char *retval;

		if ( ( retval = AIFunc_InspectFriendlyStart( cs, enemies[0] ) ) ) {
			return retval;
		}
	} else if ( !( cs->bFlags & BFL_ATTACKED ) && numEnemies > 0 )       {
		int i;
		for ( i = 0; i < numEnemies; i++ ) {
			if ( enemies[i] != cs->enemyNum && AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
				cs->enemyNum = enemies[i];
				//G_Printf("aborting, other enemy\n");
				return AIFunc_BattleStart( cs );
			}
		}
	}

	if ( followent->client ) { // go to the last visible position
		VectorCopy( cs->vislist[cs->enemyNum].visible_pos, destorg );
	} else    // assume we know where other entities are
	{
		VectorCopy( followent->s.pos.trBase, destorg );
	}
	//
	dist = VectorDistance( destorg, cs->bs->origin );
	//
	if ( cs->vislist[cs->enemyNum].lastcheck_timestamp > cs->vislist[cs->enemyNum].real_visible_timestamp ||
		 dist > 128 ) {
		//
		// go to them
		//
		if ( followent->client && followent->health <= 0 ) {
			cs->enemyNum = -1;
			//G_Printf("aborting, enemy dead\n");
			return AIFunc_DefaultStart( cs );
		}
		//
		// ...........................................................
		// Do the movement..
		//
		// move straight to them if we can
		if ( ( cs->leaderNum < 0 ) &&
			 ( cs->bs->cur_ps.groundEntityNum != ENTITYNUM_NONE || g_entities[cs->entityNum].waterlevel > 1 ) ) {
			aicast_predictmove_t move;
			vec3_t dir;
			bot_input_t bi;
			usercmd_t ucmd;
			trace_t tr;

			// trace will eliminate most unsuccessful paths
			trap_Trace( &tr, cs->bs->origin, g_entities[cs->entityNum].r.mins, g_entities[cs->entityNum].r.maxs, followent->r.currentOrigin, cs->entityNum, g_entities[cs->entityNum].clipmask );
			if ( tr.entityNum == followent->s.number ) {
				// try walking straight to them
				VectorSubtract( followent->r.currentOrigin, cs->bs->origin, dir );
				VectorNormalize( dir );
				if ( !ent->waterlevel ) {
					dir[2] = 0;
				}
//				trap_EA_Move( cs->entityNum, dir, 400 );
				trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi );
				VectorCopy( dir, bi.dir );
				bi.speed = 400;
				bi.actionflags = 0;
				AICast_InputToUserCommand( cs, &bi, &ucmd, bs->cur_ps.delta_angles );
				AICast_PredictMovement( cs, 5, 2.0, &move, &ucmd, cs->enemyNum );

				if ( move.stopevent == PREDICTSTOP_HITENT ) { // success!
					trap_EA_Move( cs->entityNum, dir, 400 );
					vectoangles( dir, cs->ideal_viewangles );
					cs->ideal_viewangles[2] *= 0.5;
					moved = qtrue;
				} else {    // clear movement
					//trap_EA_Move(cs->entityNum, dir, 0);
				}
			}
		}
		// just go to them
		if ( !moved ) {
			moveresult = AICast_MoveToPos( cs, destorg, cs->enemyNum );
			if ( moveresult && moveresult->failure ) {    // no path, so go back to idle behaviour
				cs->enemyNum = -1;
				//G_Printf("aborting, movement failure\n");
				return AIFunc_DefaultStart( cs );
			}
		}
	}
	//
	// ...........................................................
	// if we throw a grenade from here, will it get their last visible position?
	//
	CalcMuzzlePoints( ent, grenadeType );
	// fire a dummy grenade
	grenade = weapon_grenadelauncher_fire( ent, WP_GRENADE_LAUNCHER );
	// check to see what will happen
	hitclient = AICast_SafeMissileFire( grenade, grenade->nextthink - level.time, cs->enemyNum, destorg, cs->entityNum, endPos );
	// kill the grenade
	G_FreeEntity( grenade );
//	if ( !attacked ) {
//		cs->weaponNum = grenadeType;	// select grenade launcher
//	}
	// set our angles for the next frame
	oldyaw = cs->ideal_viewangles[YAW];
	AICast_AimAtEnemy( cs );
	// if we can't see them, keep facing our movement dir, but use the pitch information
	if ( !AICast_EntityVisible( cs, cs->enemyNum, qtrue ) ) {
		cs->ideal_viewangles[YAW] = oldyaw;
	}

	if ( hitclient == 1 ) {
		// it will hit their last visible position
		// give us some time to aim and adjust
		if ( cs->thinkFuncChangeTime < level.time - 200 ) {
			cs->bFlags |= BFL_ATTACKED;
			cs->weaponNum = grenadeType;    // select grenade launcher
			cs->grenadeFlushFiring = cs->weaponNum;
			// pause for a bit, so the grenade comes out correctly
			cs->lockViewAnglesTime = level.time + 1200;
			if ( cs->castScriptStatus.scriptNoMoveTime < level.time + 1200 ) {
				cs->castScriptStatus.scriptNoMoveTime = level.time + 1200;
			}
			return NULL;
		}
	} else if ( hitclient == -1 ) {
		// hit a friendly
		cs->startGrenadeFlushTime = level.time + 3000;  // don't try again for a while
		//G_Printf("aborting, too dangerous\n");
		return AIFunc_DefaultStart( cs );
	} else if ( hitclient == -2 ) {
		// went too far, so angle down a bit
		cs->ideal_viewangles[PITCH] += 15 * random();
	} else {
		if ( cs->thinkFuncChangeTime < level.time - 200 ) {
			// if it went reasonably close to them, but safe from us, then fire away
			if ( Distance( endPos, cs->bs->origin ) > 100 + Distance( endPos, g_entities[cs->enemyNum].r.currentOrigin ) ) {
				trap_EA_Attack( bs->client );
				cs->bFlags |= BFL_ATTACKED;
				cs->weaponNum = grenadeType;    // select grenade launcher
				cs->grenadeFlushFiring = cs->weaponNum;
				// pause for a bit, so the grenade comes out correctly
				cs->lockViewAnglesTime = level.time + 1200;
				if ( cs->castScriptStatus.scriptNoMoveTime < level.time + 1200 ) {
					cs->castScriptStatus.scriptNoMoveTime = level.time + 1200;
				}
				return NULL;
			}
		}
		cs->ideal_viewangles[PITCH] += -10 * random();
	}
	//
	return NULL;
}

/*
============
AIFunc_GrenadeFlushStart()
============
*/
char *AIFunc_GrenadeFlushStart( cast_state_t *cs ) {
	lastGrenadeFlush = level.time; // + rand()%2000;
	cs->startGrenadeFlushTime = level.time;
	cs->grenadeFlushEndTime = -1;
	cs->lockViewAnglesTime = 0;
	cs->combatGoalTime = 0;
	cs->grenadeFlushFiring = qfalse;
	// don't wait too long before taking cover, if we just aborted one
	if ( cs->takeCoverTime > level.time + 1000 ) {
		cs->takeCoverTime = level.time + 500 + rand() % 500;
	}
	//
	cs->aifunc = AIFunc_GrenadeFlush;
	return "AIFunc_GrenadeFlush";
}

/*
============
AIFunc_BattleMG42

Mounted MG42 combat behavior.
Tracks valid enemies inside MG42 arcs, fires in bursts, and dismounts if no target is usable.
============
*/
char *AIFunc_BattleMG42( cast_state_t *cs ) {
	bot_state_t *bs;
	gentity_t *mg42, *ent;
	vec3_t angles, vec;
	qboolean unmount;
	qboolean shouldAttack;

	mg42 = &g_entities[cs->mountedEntity];
	ent = &g_entities[cs->entityNum];
	bs = cs->bs;

	unmount = qfalse;

	// Already dismounted
	if ( !ent->active ) {
		return AIFunc_DefaultStart( cs );
	}

	// Finish dismount by facing forward first
	if ( cs->aiFlags & AIFL_DISMOUNTING ) {
		VectorCopy( mg42->s.angles, cs->ideal_viewangles );

		if ( fabs( AngleDifference( mg42->s.angles[YAW], cs->viewangles[YAW] ) ) < 10 ) {
			Cmd_Activate_f( ent );
		}

		return NULL;
	}

	if ( g_entities[cs->enemyNum].health <= 0 ) {
		cs->enemyNum = -1;
	}

	// Build aim angles toward current enemy, using recent real-visible position when possible
	if ( cs->enemyNum >= 0 ) {
		if ( cs->vislist[cs->enemyNum].real_visible_timestamp > level.time - 5000 ) {
			VectorSubtract( cs->vislist[cs->enemyNum].real_visible_pos, mg42->r.currentOrigin, vec );
		} else if ( cs->vislist[cs->enemyNum].visible_timestamp > level.time - 5000 ) {
			VectorSubtract( cs->vislist[cs->enemyNum].visible_pos, mg42->r.currentOrigin, vec );
		} else {
			AngleVectors( mg42->s.angles, vec, NULL, NULL );
		}

		VectorNormalize( vec );
		vectoangles( vec, angles );
		angles[PITCH] = AngleNormalize180( angles[PITCH] );
	}

	// Current enemy is missing, blocked, or outside MG42 movement arcs
	if ( cs->enemyNum < 0 ||
		 !AICast_CheckAttack( cs, cs->enemyNum, qfalse ) ||
		 fabs( AngleDifference( angles[YAW], mg42->s.angles[YAW] ) ) > mg42->harc ||
		 ( angles[PITCH] < 0 && angles[PITCH] + 5 < -mg42->varc ) ||
		 ( angles[PITCH] > 0 && angles[PITCH] - 5 > 5.0 ) ) {
		numEnemies = AICast_ScanForEnemies( cs, enemies );
		shouldAttack = qfalse;

		if ( numEnemies > 0 ) {
			int i;

			cs->enemyNum = enemies[0];
			unmount = qtrue;

			for ( i = 0; i < numEnemies; i++ ) {
				if ( cs->vislist[enemies[i]].real_visible_timestamp > level.time - 5000 ) {
					VectorSubtract( cs->vislist[enemies[i]].real_visible_pos, mg42->r.currentOrigin, vec );
				} else {
					VectorSubtract( cs->vislist[enemies[i]].visible_pos, mg42->r.currentOrigin, vec );
				}

				VectorNormalize( vec );
				vectoangles( vec, angles );
				angles[PITCH] = AngleNormalize180( angles[PITCH] );

				// Keep original MG42 arc test behavior
				if ( !( fabs( AngleDifference( angles[YAW], mg42->s.angles[YAW] ) ) > mg42->harc ||
						( angles[YAW] < 0 && angles[YAW] + 2 < -mg42->varc ) ||
						( angles[YAW] > 0 && angles[YAW] - 2 > 5.0 ) ) ) {
					unmount = qfalse;

					if ( AICast_CheckAttack( cs, enemies[i], qfalse ) ) {
						cs->enemyNum = enemies[i];
						shouldAttack = qtrue;
						break;
					}

					// Suppress targets behind solids if they were recently visible
					if ( AICast_CheckAttack( cs, enemies[i], qtrue ) ) {
						cs->enemyNum = enemies[i];
						shouldAttack = qtrue;
					}
				}
			}
		}

		if ( !shouldAttack ) {
			// If there is no useful suppressed target, return aim to neutral
			if ( cs->enemyNum < 0 ||
				 !AICast_CheckAttack( cs, cs->enemyNum, qtrue ) ||
				 !cs->vislist[cs->enemyNum].real_visible_timestamp ||
				 cs->vislist[cs->enemyNum].real_visible_timestamp < level.time - 2000 ) {
				cs->ideal_viewangles[PITCH] = 0;
				return NULL;
			}
		}

		// Enemy exists, but none are usable from this MG42
		if ( unmount ) {
			AICast_ScriptEvent( cs, "forced_mg42_unmount", NULL );

			if ( !( cs->aiFlags & AIFL_DENYACTION ) ) {
				cs->aiFlags |= AIFL_DISMOUNTING;
				return NULL;
			}
		}
	}

	// Track target and fire in controlled bursts
	VectorCopy( angles, cs->ideal_viewangles );

	if ( cs->triggerReleaseTime < level.time ) {
		trap_EA_Attack( bs->client );
		cs->bFlags |= BFL_ATTACKED;

		if ( cs->triggerReleaseTime < level.time - 3000 ) {
			cs->triggerReleaseTime = level.time + 700 + rand() % 700;
		}
	}

	return NULL;
}

/*
============
AIFunc_BattleMG42Start()
============
*/
char *AIFunc_BattleMG42Start( cast_state_t *cs ) {
	cs->aiFlags &= ~AIFL_DISMOUNTING;
	//
	cs->aifunc = AIFunc_BattleMG42;
	return "AIFunc_BattleMG42";
}

/*
============
AIFunc_InspectBody

Walks to a dead enemy, briefly inspects/taunts the body, then returns to start position.
Interrupts for danger, scripts, new enemies, friendly inspection, bullet impacts, or audible events.
============
*/
char *AIFunc_InspectBody( cast_state_t *cs ) {
	bot_state_t *bs;
	vec3_t destorg, enemyOrg;
	vec3_t vec;
	char *retval;

	// Stop crouching while inspecting
	cs->attackcrouch_time = 0;

	// Danger overrides body inspection
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// No cover found, but still enter danger avoidance
		}

		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}

	// Door marker behavior temporarily overrides inspection
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	// Scripts take control over inspection behavior
	if ( cs->castScriptStatus.castScriptEventIndex >= 0 ) {
		cs->enemyNum = -1;
		return AIFunc_IdleStart( cs );
	}

	bs = cs->bs;

	if ( cs->enemyNum < 0 ) {
		return AIFunc_IdleStart( cs );
	}

	// React to higher-priority perception events
	numEnemies = AICast_ScanForEnemies( cs, enemies );

	if ( numEnemies == -1 ) {
		return NULL;
	}

	if ( numEnemies == -2 ) {
		retval = AIFunc_InspectFriendlyStart( cs, enemies[0] );
		if ( retval ) {
			return retval;
		}
	} else if ( numEnemies == -3 ) {
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectBulletImpactStart( cs );
		}
	} else if ( numEnemies == -4 ) {
		if ( cs->aiState < AISTATE_COMBAT ) {
			return AIFunc_InspectAudibleEventStart( cs, cs->audibleEventEnt );
		}
	} else if ( numEnemies > 0 ) {
		cs->enemyNum = enemies[0];
		return AIFunc_BattleStart( cs );
	}

	VectorCopy( cs->vislist[cs->enemyNum].visible_pos, enemyOrg );

	if ( cs->inspectBodyTime < 0 && Distance( cs->bs->origin, enemyOrg ) > 64 ) {
		// Gibbed bodies are inspected from farther away
		if ( g_entities[cs->enemyNum].health < GIB_HEALTH &&
			 Distance( cs->bs->origin, enemyOrg ) < 180 ) {
			cs->inspectBodyTime = level.time + 1000 + rand() % 1000;
			trap_EA_Gesture( cs->entityNum );
			G_AddEvent( &g_entities[cs->entityNum], EV_GENERAL_SOUND,
						G_SoundIndex( aiDefaults[cs->aiCharacter].soundScripts[ORDERSSOUNDSCRIPT] ) );
		}

		// Walk toward the body
		if ( cs->movestate != MS_CROUCH ) {
			cs->movestate = MS_WALK;
		}

		cs->movestateType = MSTYPE_TEMPORARY;

		moveresult = AICast_MoveToPos( cs, enemyOrg, -1 );

		if ( moveresult && ( moveresult->failure || moveresult->blocked ) ) {
			trap_BotResetAvoidReach( bs->ms );
			cs->enemyNum = -1;
			return AIFunc_IdleStart( cs );
		}

		// Look down at the body when close enough
		if ( Distance( cs->bs->origin, enemyOrg ) < 180 ) {
			VectorSubtract( enemyOrg, cs->bs->origin, destorg );
			destorg[2] -= 20;
			VectorNormalize( destorg );
			vectoangles( destorg, cs->ideal_viewangles );
		}
	} else if ( cs->inspectBodyTime < 0 ) {
		// Reached the body
		cs->inspectBodyTime = level.time + 1000 + rand() % 1000;
		trap_EA_Gesture( cs->entityNum );
		G_AddEvent( &g_entities[cs->entityNum], EV_GENERAL_SOUND,
					G_SoundIndex( aiDefaults[cs->aiCharacter].soundScripts[ORDERSSOUNDSCRIPT] ) );
	} else if ( cs->inspectBodyTime < level.time ) {
		// Return to start position after inspection
		VectorSubtract( cs->startOrigin, cs->bs->origin, vec );
		vec[2] = 0;

		if ( VectorLength( vec ) > 64 ) {
			if ( cs->movestate != MS_CROUCH ) {
				cs->movestate = MS_WALK;
			}

			cs->movestateType = MSTYPE_TEMPORARY;

			moveresult = AICast_MoveToPos( cs, cs->startOrigin, -1 );

			if ( moveresult && ( moveresult->failure || moveresult->blocked ) ) {
				trap_BotResetAvoidReach( bs->ms );
				cs->enemyNum = -1;
				return AIFunc_IdleStart( cs );
			}

			// Keep looking at the body briefly while walking away
			if ( cs->inspectBodyTime + 750 > level.time ) {
				VectorSubtract( enemyOrg, cs->bs->origin, destorg );
				destorg[2] -= 20;
				VectorNormalize( destorg );
				vectoangles( destorg, cs->ideal_viewangles );
			}
		} else {
			cs->attackSNDtime = level.time;
			cs->enemyNum = -1;
			return AIFunc_IdleStart( cs );
		}
	}

	return NULL;
}

/*
============
AIFunc_InspectBodyStart

Starts body inspection behavior.
Throttles repeated inspections so nearby AI do not all inspect at once.
============
*/
char *AIFunc_InspectBodyStart( cast_state_t *cs ) {
	static int lastInspect;

	// If another inspection just started, return to start position instead
	if ( lastInspect <= level.time && lastInspect > level.time - 1000 ) {
		cs->inspectBodyTime = 1;
	} else {
		lastInspect = level.time;
		cs->inspectBodyTime = -1;
	}

	cs->aifunc = AIFunc_InspectBody;
	return "AIFunc_InspectBody";
}


/*
============
AIFunc_GrenadeKick()
============
*/
char *AIFunc_GrenadeKick( cast_state_t *cs ) {
	bot_state_t *bs;
	vec3_t destorg, vec;
	float dist, speed;
	int enemies[MAX_CLIENTS], numEnemies = 0, i;
	qboolean shouldAttack;
	gentity_t *danger;
	gentity_t *ent;
	vec3_t end;
	trace_t tr;
	vec3_t dir;

	// !!! NOTE: the only way control should pass out of here, is by calling AIFunc_DefaultStart()
	ent = &g_entities[cs->entityNum];
	danger = &g_entities[cs->dangerEntity];


	// just to be sure, give us the grenade launcher
	//ent->client->ps.weapons |= (1 << weapon);

	//
	// NOTE: ignore all danger, since we are trying to solve the situation anyway
	//
	// we need to move towards it
	bs = cs->bs;
	//
	// are we throwing it back?
	if ( cs->grenadeFlushFiring ) {
		// wait until the animation is done
		if ( !ent->client->ps.legsTimer ) {
			return AIFunc_DefaultStart( cs );
		}
		// wait till its finished
		return NULL;
		/*
		cs->weaponNum = weapon;	// select grenade launcher
		//
		if (cs->weaponFireTimes[weapon] < cs->thinkFuncChangeTime) {
			if (!cs->bs->cur_ps.grenadeTimeLeft) {
				// hold fire button down
				AICast_AimAtEnemy( cs );
				trap_EA_Attack(bs->client);
				cs->bFlags |= BFL_ATTACKED;
			}
			//
			return NULL;
		}
		// the grenade has been released!
		//
		// modify the explode time
		g_entities[ent->grenadeFired].nextthink = ent->grenadeExplodeTime;
		if (g_entities[ent->grenadeFired].nextthink < level.time + 200) {	// cut them some slack
			g_entities[ent->grenadeFired].nextthink = level.time + 200 + rand()%500;
		}
		// make sure no-one tries to throw this back again (hot potatoe syndrome)
		g_entities[ent->grenadeFired].flags |= FL_AI_GRENADE_KICK;
		//
		cs->grenadeFlushFiring = qfalse;
		cs->lockViewAnglesTime = -1;
		cs->startGrenadeFlushTime = level.time + 2000 + rand()%2000;	// dont throw one again for a bit
		level.lastGrenadeKick = level.time;
		return AIFunc_DefaultStart( cs );
		*/
	}
	//
/*
	// have we caught the grenade?
	if (!(ent->flags & FL_AI_GRENADE_KICK)) {
		// select grenades
		cs->weaponNum = weapon;	// select grenade launcher
		AICast_AimAtEnemy( cs );
		// hold fire
		trap_EA_Attack(bs->client);
		cs->bFlags |= BFL_ATTACKED;
		cs->grenadeFlushFiring = qtrue;
		//
		return NULL;
	}
*/
	//
	// is it about to explode in our face?
	if ( level.time > danger->nextthink - (int)( 2.0 * VectorDistance( cs->bs->origin, danger->r.currentOrigin ) ) ) {
		// abort!!
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// shit??
		}
		// go to a position that cannot be seen from the dangerPos
		cs->takeCoverTime = danger->nextthink + 1000;
		cs->attackcrouch_time = 0;
		level.lastGrenadeKick = level.time;
		return AIFunc_AvoidDangerStart( cs );
	}
	//
	/*
	// are we close enough to start crouching?
	if (danger->s.pos.trDelta[2] < 40 && VectorDistance( danger->r.currentOrigin, cs->bs->origin ) < 48 && (danger->r.currentOrigin[2] < cs->bs->origin[2]) &&
		VectorLength(danger->s.pos.trDelta) < 40) {
		// crouch to pick it up
		cs->attackcrouch_time = level.time + 300;
	}
	*/
	cs->attackcrouch_time = 0;  // animation is played from standing start
	//
	// are we close enough to pick it up?
	if ( /*cs->grenadeGrabFlag <= 0 || */
		( danger->s.pos.trDelta[2] < 20 && VectorDistance( danger->r.currentOrigin, cs->bs->origin ) < 48 && ( danger->r.currentOrigin[2] < cs->bs->origin[2] ) &&
		  VectorLength( danger->s.pos.trDelta ) < 50 ) ) {
		//
		// we have a choice here, either pick up and return, or just kick it
//		if ((cs->grenadeGrabFlag == -1) || (cs->grenadeGrabFlag == qtrue && level.time > danger->nextthink - 2000)) {	// kick

		// play the kick anim
		if ( cs->grenadeGrabFlag == qtrue ) {
			AICast_AimAtEnemy( cs );
			// play the kick anim
			BG_AnimScriptEvent( &ent->client->ps, ANIM_ET_KICKGRENADE, qfalse, qtrue );
			cs->grenadeGrabFlag = -1;
			// stop the grenade from moving away
			danger->s.pos.trDelta[0] = 0;
			danger->s.pos.trDelta[1] = 0;
			if ( danger->s.pos.trDelta[2] > 0 ) {
				danger->s.pos.trDelta[2] = 0;
			}
		} else if ( ent->client->ps.legsTimer < 800 ) {
			// send the grenade on its way
			cs->grenadeFlushFiring = qtrue;
			AngleVectors( cs->viewangles, dir, NULL, NULL );
			dir[2] = 0.4;
			VectorNormalize( dir );
			speed = 400;
			if ( cs->enemyNum >= 0 ) {
				speed = 1.5 * VectorDistance( danger->r.currentOrigin, g_entities[cs->enemyNum].r.currentOrigin );
				if ( speed > 650 ) {
					speed = 650;
				}
			}
			VectorScale( dir, speed, danger->s.pos.trDelta );
			danger->r.ownerNum = ent->s.number;     // we are now the owner, let it pass through us
			danger->s.pos.trTime = level.time - 50;         // move a bit on the very first frame
			VectorCopy( danger->r.currentOrigin, danger->s.pos.trBase );
			danger->s.pos.trType = TR_GRAVITY;
			SnapVector( danger->s.pos.trDelta );                // save net bandwidth
		}
/*
		} else { // throw

			if (cs->grenadeGrabFlag == qtrue) {
				AICast_AimAtEnemy( cs );
				// play the pickup anim
				BG_AnimScriptEvent( &ent->client->ps, ANIM_ET_PICKUPGRENADE, qfalse, qtrue );
				cs->grenadeGrabFlag = qfalse;
				// stop the grenade from moving away
				danger->s.pos.trDelta[0] = 0;
				danger->s.pos.trDelta[1] = 0;
				if (danger->s.pos.trDelta[2] > 0) {
					danger->s.pos.trDelta[2] = 0;
				}
			} else if (ent->client->ps.legsTimer < 400) {
				// send the grenade on its way
				cs->grenadeFlushFiring = qtrue;
				AngleVectors( cs->viewangles, dir, NULL, NULL );
				dir[2] = 0.4;
				VectorNormalize( dir );
				speed = 500;
				if (cs->enemyNum >= 0) {
					speed = 2*VectorDistance(danger->r.currentOrigin, g_entities[cs->enemyNum].r.currentOrigin);
					if (speed > 650)
						speed = 650;
				}
				VectorScale( dir, speed, danger->s.pos.trDelta );
				trap_LinkEntity( danger );
			} else if (ent->client->ps.legsTimer < 800) {
				// stop showing the grenade
				trap_UnlinkEntity( danger );
			}
		}
*/
		//
		return NULL;
	}
	//
	cs->grenadeGrabFlag = qtrue;    // we must play the anim before we can grab it
	//
	// is the danger gone?
	if ( level.time > cs->dangerEntityValidTime || !danger->inuse ) {
		return AIFunc_DefaultStart( cs );
	}
	//
	// update the predicted position of the grenade
	if ( G_PredictMissile( danger, danger->nextthink - level.time, cs->takeCoverPos, qfalse ) ) {
		// make sure it's a valid position, and drop it down to the ground
		cs->takeCoverPos[2] += -ent->r.mins[2] + 8;
		VectorCopy( cs->takeCoverPos, end );
		end[2] -= 80;
		trap_Trace( &tr, cs->takeCoverPos, ent->r.mins, ent->r.maxs, end, cs->entityNum, MASK_SOLID );
		if ( tr.startsolid ) {    // not a valid position, abort
			level.lastGrenadeKick = level.time;
			return AIFunc_DefaultStart( cs );
		}
		VectorCopy( tr.endpos, cs->takeCoverPos );
	} else {    // prediction failed, so use current position
		VectorCopy( danger->r.currentOrigin, cs->takeCoverPos );
		cs->takeCoverPos[2] += 16;  // lift it off the floor
	}

	VectorCopy( cs->takeCoverPos, destorg );
	VectorSubtract( destorg, cs->bs->origin, vec );
	//vec[2] *= 0.2;
	dist = VectorLength( vec );
	//
	// look for things we should attack
	// if we are out of ammo, we shouldn't bother trying to attack
	shouldAttack = qfalse;
	if ( AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
		numEnemies = AICast_ScanForEnemies( cs, enemies );
		if ( numEnemies == -1 ) { // query mode
			return NULL;
		}
		if ( numEnemies == -2 ) { // inspection may be required
			char *retval;

			if ( ( retval = AIFunc_InspectFriendlyStart( cs, enemies[0] ) ) ) {
				return retval;
			}
		}
		if ( numEnemies > 0 ) {
			// default to the first known enemy, overwrite if we find a clearer shot
			cs->enemyNum = enemies[0];
			//
			for ( i = 0; i < numEnemies; i++ ) {
				if ( AICast_CheckAttack( cs, enemies[i], qfalse ) || AICast_CheckAttack( AICast_GetCastState( enemies[i] ), cs->entityNum, qfalse ) ) {
					cs->enemyNum = enemies[i];
					shouldAttack = qtrue;
					break;
				} else if ( cs->enemyNum < 0 ) {
					cs->lastEnemy = enemies[i];
				}
			}
		}
	}
	//
	// are we close enough to the goal?
	if ( dist > 12 ) { // not close enough
					  //
		moveresult = AICast_MoveToPos( cs, destorg, -1 );
		if ( moveresult ) {
			//if the movement failed
			if ( moveresult->failure ) {
				//reset the avoid reach, otherwise bot is stuck in current area
				trap_BotResetAvoidReach( bs->ms );
				// couldn't get there, so stop trying to get there
				level.lastGrenadeKick = level.time;
				return AIFunc_DefaultStart( cs );
			}
			//
			if ( moveresult->blocked ) {  // abort if we get blocked at any point
				level.lastGrenadeKick = level.time;
				return AIFunc_DefaultStart( cs );
			}
		}
		// we should slow down on approaching it
		cs->speedScale = AICast_SpeedScaleForDistance( cs, dist, 0 );
	}
	// if we should be attacking something on our way
	if ( shouldAttack ) {
		vec3_t vec, dir;

		//attack the enemy if possible
		AICast_ProcessAttack( cs );
		//
		// if they are close, and we're heading for them, we should abort this manouver
		VectorSubtract( g_entities[cs->enemyNum].client->ps.origin, bs->origin, vec );
		if ( VectorNormalize( vec ) < 64 ) {
			if ( VectorNormalize2( bs->velocity, dir ) > 20 ) {   // we are moving
				if ( DotProduct( dir, vec ) > 0 ) {
					// abort
					level.lastGrenadeKick = level.time;
					return AIFunc_DefaultStart( cs );
				}
			}
		}
	} else {
		// face the direction that the grenade is coming
		VectorSubtract( danger->r.currentOrigin, cs->bs->origin, dir );
		dir[2] = 0;
		VectorNormalize( dir );
		vectoangles( dir, cs->ideal_viewangles );
	}

	return NULL;
}

/*
=============
AIFunc_GrenadeKickStart()
=============
*/
char *AIFunc_GrenadeKickStart( cast_state_t *cs ) {
	gentity_t *danger;

	danger = &g_entities[cs->dangerEntity];
	// we have decided to kick or throw the grenade away
	cs->grenadeKickWeapon = danger->s.weapon;
	cs->grenadeFlushFiring = qfalse;
	cs->aifunc = AIFunc_GrenadeKick;
	return "AIFunc_GrenadeKick";
}

/*
============
AIFunc_Battle

Main generic combat behavior.
Handles danger avoidance, chasing, cover, grenades, dodging, reloading, and attacking.
============
*/
char *AIFunc_Battle( cast_state_t *cs ) {
	bot_moveresult_t moveresult;
	bot_state_t *bs;
	gentity_t *ent, *enemy;

	ent = &g_entities[cs->entityNum];
	enemy = &g_entities[cs->enemyNum];
	bs = cs->bs;

	// Force combat state if we reached battle logic from a lower state
	if ( cs->aiState < AISTATE_COMBAT ) {
		AICast_StateChange( cs, AISTATE_COMBAT );
	}

	// Active danger has priority over normal combat behavior
	if ( cs->dangerEntityValidTime >= level.time ) {
		if ( !AICast_GetTakeCoverPos( cs, cs->dangerEntity, cs->dangerEntityPos, cs->takeCoverPos ) ) {
			// No cover found, but still enter danger avoidance
		}

		cs->takeCoverTime = cs->dangerEntityValidTime + 1000;
		cs->attackcrouch_time = 0;
		return AIFunc_AvoidDangerStart( cs );
	}

	// Door marker behavior temporarily overrides combat
	if ( cs->doorMarkerTime > level.time - 100 ) {
		return AIFunc_DoorMarkerStart( cs, cs->doorMarkerDoor, cs->doorMarkerNum );
	}

	// Defend behavior can fully handle this frame
	if ( AICast_Defend_Update( cs ) ) {
		return NULL;
	}

	// Stay near leader if we are following one
	if ( cs->leaderNum >= 0 &&
		 Distance( cs->bs->origin, g_entities[cs->leaderNum].r.currentOrigin ) > MAX_LEADER_DIST ) {
		return AIFunc_ChaseGoalStart( cs, cs->leaderNum, AICAST_LEADERDIST_MAX, qtrue );
	}

	if ( cs->enemyNum < 0 ) {
		return AIFunc_DefaultStart( cs );
	}

	if ( enemy->health <= 0 ) {
		if ( ent->aiTeam == AITEAM_NAZI ) {
			return AIFunc_InspectBodyStart( cs );
		}

		return AIFunc_DefaultStart( cs );
	}

	// If we cannot attack from here, switch to chase behavior
	if ( !AICast_StopAndAttack( cs ) ) {
		return AIFunc_BattleChaseStart( cs );
	}

	// Enemy cannot currently be attacked
	if ( cs->bs->cur_ps.weaponTime < 100 &&
		 cs->castScriptStatus.scriptNoMoveTime < level.time &&
		 !AICast_CheckAttack( cs, cs->enemyNum, qfalse ) ) {
		if ( !cs->bs->areanum ) {
			// If outside valid AAS, try to move out of the bad area
			if ( cs->obstructingTime >= level.time ) {
				trap_EA_Move( cs->entityNum, cs->takeCoverPos, 200 );
			} else if ( AICast_GetAvoid( cs, NULL, cs->takeCoverPos, qtrue, cs->enemyNum ) ) {
				VectorSubtract( cs->takeCoverPos, cs->bs->origin, cs->takeCoverPos );

				if ( VectorNormalize( cs->takeCoverPos ) > 60 ) {
					cs->obstructingTime = level.time + 1000 + rand() % 600;
				}

				return NULL;
			}
		} else if ( cs->combatGoalTime > level.time ) {
			// Give active combat goals time to resolve
			if ( cs->combatGoalTime > level.time + 3000 ) {
				cs->combatGoalTime = level.time + 2000 + rand() % 1000;
				cs->combatSpotDelayTime = level.time + 4000 + rand() % 3000;
			}
		} else if ( cs->leaderNum >= 0 ) {
			return AIFunc_BattleChaseStart( cs );
		} else if ( cs->attributes[TACTICAL] > 0.3 &&
					cs->attributes[AGGRESSION] < 1.0 &&
					cs->attributes[AGGRESSION] < random() + 0.5 * cs->attributes[TACTICAL] &&
					cs->takeCoverTime < level.time &&
					AICast_GetTakeCoverPos( cs, cs->enemyNum,
											cs->vislist[cs->enemyNum].real_visible_pos,
											cs->takeCoverPos ) ) {
			// Hide from the enemy's last real visible position
			cs->takeCoverTime = level.time + 2000 + rand() % 4000;
			return AIFunc_BattleTakeCoverStart( cs );
		} else if ( ( lastGrenadeFlush > level.time || lastGrenadeFlush < level.time - 7000 ) &&
					cs->aiState >= AISTATE_COMBAT &&
					cs->castScriptStatus.castScriptEventIndex < 0 &&
					( ( COM_BitCheck( cs->bs->cur_ps.weapons, WP_GRENADE_LAUNCHER ) &&
						AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_LAUNCHER ) &&
						cs->weaponFireTimes[WP_GRENADE_LAUNCHER] < level.time - (int)( aicast_skillscale * 3000 ) ) ||
					  ( COM_BitCheck( cs->bs->cur_ps.weapons, WP_GRENADE_PINEAPPLE ) &&
						AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_PINEAPPLE ) &&
						cs->weaponFireTimes[WP_GRENADE_PINEAPPLE] < level.time - (int)( aicast_skillscale * 3000 ) ) ) &&
					!( cs->weaponNum && ( cs->castScriptStatus.scriptFlags & SFL_NOCHANGEWEAPON ) ) &&
					Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) > 100 &&
					Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) < 1200 &&
					AICast_WantsToChase( cs ) ) {
			return AIFunc_GrenadeFlushStart( cs );
		} else if ( AICast_WantsToChase( cs ) ) {
			return AIFunc_BattleChaseStart( cs );
		} else if ( AICast_WantsToTakeCover( cs, qfalse ) &&
					cs->takeCoverTime < level.time &&
					AICast_GetTakeCoverPos( cs, cs->enemyNum,
											cs->vislist[cs->enemyNum].real_visible_pos,
											cs->takeCoverPos ) ) {
			cs->takeCoverTime = level.time + 4000 + rand() % 2000;
			return AIFunc_BattleTakeCoverStart( cs );
		} else {
			return AIFunc_BattleChaseStart( cs );
		}
	}

	// Move away if another AI asked us to clear their path
	if ( cs->obstructingTime > level.time ) {
		AICast_MoveToPos( cs, cs->obstructingPos, -1 );
		cs->speedScale = cs->attributes[WALKING_SPEED] / cs->attributes[RUNNING_SPEED];
	} else if ( cs->obstructingTime < level.time - 500 + rand() % 300 &&
				Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) < 100 ) {
		// Enemy is too close, try to back away
		if ( AICast_GetAvoid( cs, NULL, cs->obstructingPos, qtrue, cs->enemyNum ) ) {
			cs->obstructingTime = level.time + 500;
		} else {
			cs->obstructingTime = level.time - 1;
		}
	}

	// Keep crouch attack alive briefly while fighting
	if ( cs->aiFlags & AIFL_ATTACK_CROUCH ) {
		if ( cs->attackcrouch_time > level.time || cs->thinkFuncChangeTime < level.time - 1000 ) {
			cs->attackcrouch_time = level.time + 1000;
		}
	} else {
		cs->attackcrouch_time = 0;
	}

	AICast_Blocked( cs, &moveresult, qfalse, NULL );

	// Retreat into cover when needed
	if ( cs->castScriptStatus.scriptNoMoveTime < level.time && AICast_WantToRetreat( cs ) ) {
		if ( AICast_GetTakeCoverPos( cs, cs->enemyNum,
									 cs->vislist[cs->enemyNum].visible_pos,
									 cs->takeCoverPos ) ) {
			cs->takeCoverTime = level.time + 2000 + rand() % 3000;
			return AIFunc_BattleTakeCoverStart( cs );
		}
	}

	// Lob German grenade
	if ( ( lastGrenadeFlush > level.time || lastGrenadeFlush < level.time - 7000 ) &&
		 cs->aiState >= AISTATE_COMBAT &&
		 cs->castScriptStatus.castScriptEventIndex < 0 &&
		 cs->startGrenadeFlushTime < level.time - 3000 &&
		 COM_BitCheck( cs->bs->cur_ps.weapons, WP_GRENADE_LAUNCHER ) &&
		 AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_LAUNCHER ) &&
		 cs->weaponFireTimes[WP_GRENADE_LAUNCHER] < level.time - (int)( aicast_skillscale * 3000 ) &&
		 ( cs->weaponNum == WP_GRENADE_LAUNCHER || !( cs->castScriptStatus.scriptFlags & SFL_NOCHANGEWEAPON ) ) &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) > 100 &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) < 2000 ) {
		return AIFunc_GrenadeFlushStart( cs );
	}

	// Lob Allied grenade
	if ( ( lastGrenadeFlush > level.time || lastGrenadeFlush < level.time - 7000 ) &&
		 cs->aiState >= AISTATE_COMBAT &&
		 cs->castScriptStatus.castScriptEventIndex < 0 &&
		 cs->startGrenadeFlushTime < level.time - 3000 &&
		 COM_BitCheck( cs->bs->cur_ps.weapons, WP_GRENADE_PINEAPPLE ) &&
		 AICast_GotEnoughAmmoForWeapon( cs, WP_GRENADE_PINEAPPLE ) &&
		 cs->weaponFireTimes[WP_GRENADE_PINEAPPLE] < level.time - (int)( aicast_skillscale * 3000 ) &&
		 ( cs->weaponNum == WP_GRENADE_PINEAPPLE || !( cs->castScriptStatus.scriptFlags & SFL_NOCHANGEWEAPON ) ) &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) > 100 &&
		 Distance( cs->bs->origin, cs->vislist[cs->enemyNum].real_visible_pos ) < 2000 ) {
		return AIFunc_GrenadeFlushStart( cs );
	}

	// Dodge enemy aim with a roll if tactical conditions allow it
	if ( cs->attributes[AGGRESSION] < 1.0 &&
		 ent->client->ps.weapon &&
		 ent->client->ps.groundEntityNum == ENTITYNUM_WORLD &&
		 ( !cs->lastRollMove || cs->lastRollMove < level.time - 4000 ) &&
		 cs->attributes[TACTICAL] > 0.5 &&
		 ( cs->aiFlags & AIFL_ROLL_ANIM ) &&
		 VectorLength( cs->bs->cur_ps.velocity ) < 1 ) {
		vec3_t aim, enemyVec, right;

		AngleVectors( enemy->client->ps.viewangles, aim, right, NULL );
		VectorSubtract( cs->bs->origin, enemy->r.currentOrigin, enemyVec );
		VectorNormalize( enemyVec );

		if ( DotProduct( aim, enemyVec ) > 0.97 ) {
			aicast_predictmove_t move;
			vec3_t dir;
			bot_input_t bi, bi_back;
			usercmd_t ucmd;
			float simTime;

			simTime = 0.8;
			cs->lastRollMove = level.time;

			trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi_back );
			trap_EA_ResetInput( cs->entityNum, NULL );

			if ( level.time % 200 < 100 ) {
				VectorNegate( right, dir );
			} else {
				VectorCopy( right, dir );
			}

			trap_EA_Move( cs->entityNum, dir, 400 );
			trap_EA_GetInput( cs->entityNum, (float) level.time / 1000, &bi );

			VectorCopy( dir, bi.dir );

			AICast_InputToUserCommand( cs, &bi, &ucmd, bs->cur_ps.delta_angles );
			AICast_PredictMovement( cs, 4, simTime / 4, &move, &ucmd, cs->enemyNum );

			trap_EA_ResetInput( cs->entityNum, &bi_back );

			if ( move.groundEntityNum == ENTITYNUM_WORLD &&
				 VectorDistance( move.endpos, cs->bs->origin ) >
				 simTime * cs->attributes[RUNNING_SPEED] * 0.8 ) {
				if ( AICast_CheckAttackAtPos( cs->entityNum, cs->enemyNum, move.endpos,
											  cs->bs->cur_ps.viewheight == cs->bs->cur_ps.crouchViewHeight,
											  qfalse ) ) {
					cs->takeCoverTime = 0;
					return AIFunc_BattleRollStart( cs, dir );
				}
			}
		}
	}

	// Reload or switch weapon if clip is too low
	if ( cs->bs->cur_ps.weaponstate != WEAPON_RELOADING &&
		 cs->bs->cur_ps.ammoclip[BG_FindClipForWeapon( cs->bs->cur_ps.weapon )] <
		 (int)( ammoTable[cs->bs->cur_ps.weapon].uses ) ) {
		if ( AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
			trap_EA_Reload( cs->entityNum );
		} else {
			AICast_ChooseWeapon( cs, qfalse );

			if ( cs->weaponNum == WP_NONE ) {
				return AIFunc_DefaultStart( cs );
			}

			if ( !AICast_GotEnoughAmmoForWeapon( cs, cs->weaponNum ) ) {
				return AIFunc_DefaultStart( cs );
			}
		}
	} else {
		AICast_ProcessAttack( cs );
	}

	return NULL;
}

/*
============
AIFunc_BattleStart

Initializes combat behavior and chooses the first battle routine.
May start a special monster attack, chase if out of range, or fall back to generic battle.
============
*/
char *AIFunc_BattleStart( cast_state_t *cs ) {
	char *rval;
	int lastweap;

	// Clear old avoid-reach data when entering combat again
	trap_BotInitAvoidReach( cs->bs->ms );

	// Delay cover/combat-spot decisions so AI does not instantly reposition every combat start
	cs->takeCoverTime = level.time + 300 + rand() % ( 2000 + (int)( 2000.0 * cs->attributes[AGGRESSION] ) );
	cs->combatSpotDelayTime = level.time + 1500 + rand() % 2500;

	// Optionally start combat in crouch-attack mode
	if ( ( random() * 3.0 + 1.0 < cs->attributes[ATTACK_CROUCH] ) &&
		 AICast_RequestCrouchAttack( cs, cs->bs->origin, 0.0 ) ) {
		cs->aiFlags |= AIFL_ATTACK_CROUCH;
	} else {
		cs->attackcrouch_time = 0;
		cs->aiFlags &= ~AIFL_ATTACK_CROUCH;
	}

	cs->lastEnemy = cs->enemyNum;
	cs->startAttackCount++;
	cs->crouchHideFlag = qfalse;

	// Combat interrupts talking
	cs->aiFlags &= ~AIFL_TALKING;

	// Refresh combat inventory before selecting an attack
	AICast_UpdateBattleInventory( cs, cs->enemyNum );

	// Special monster attacks can reject themselves, so allow one or more weapon retries
	while ( qtrue ) {
		rval = NULL;

		// Special attacks are only allowed once the AI is roughly facing the enemy
		if ( fabs( AngleDifference( cs->ideal_viewangles[YAW], cs->viewangles[YAW] ) ) < 10 ) {
			AICast_ChooseWeapon( cs, qtrue );

			if ( cs->weaponNum == WP_MONSTER_ATTACK1 && cs->aifuncAttack1 ) {
				if ( AICast_CheckAttack( cs, cs->enemyNum, qfalse ) ) {
					rval = cs->aifuncAttack1( cs );
				} else {
					rval = AIFunc_BattleChaseStart( cs );
				}
			} else if ( cs->weaponNum == WP_MONSTER_ATTACK2 && cs->aifuncAttack2 ) {
				if ( AICast_CheckAttack( cs, cs->enemyNum, qfalse ) ) {
					rval = cs->aifuncAttack2( cs );
				} else {
					rval = AIFunc_BattleChaseStart( cs );
				}
			} else if ( cs->weaponNum == WP_MONSTER_ATTACK3 && cs->aifuncAttack3 ) {
				if ( AICast_CheckAttack( cs, cs->enemyNum, qfalse ) ) {
					rval = cs->aifuncAttack3( cs );
				} else {
					rval = AIFunc_BattleChaseStart( cs );
				}
			}

			if ( rval ) {
				return rval;
			}

			if ( cs->weaponNum < WP_MONSTER_ATTACK1 || cs->weaponNum > WP_MONSTER_ATTACK3 ) {
				break;
			}

			// Rejected monster attack: cooldown this weapon and try another non-special weapon
			cs->weaponFireTimes[cs->weaponNum] = level.time;

			lastweap = cs->weaponNum;
			AICast_ChooseWeapon( cs, qfalse );

			if ( cs->weaponNum == lastweap ) {
				return NULL;
			}

			continue;
		}

		// Not facing the enemy yet, so only choose a normal weapon
		AICast_ChooseWeapon( cs, qfalse );
		break;
	}

	// Monster attacks must use their special routines, not generic battle
	if ( cs->weaponNum >= WP_MONSTER_ATTACK1 && cs->weaponNum <= WP_MONSTER_ATTACK3 ) {
		return NULL;
	}

	cs->aifunc = AIFunc_Battle;
	return "AIFunc_Battle";
}

/*
============
AIFunc_DefaultStart

Selects the default behavior for an AI.
Handles first-spawn special cases, then falls back to combat or idle.
============
*/
char *AIFunc_DefaultStart( cast_state_t *cs ) {
	qboolean first;
	char *rval;

	first = qfalse;
	rval = NULL;

	if ( cs->aiFlags & AIFL_JUST_SPAWNED ) {
		first = qtrue;
		cs->aiFlags &= ~AIFL_JUST_SPAWNED;
	}

	// Some zombies can use a special portal spawn behavior on first spawn
	switch ( cs->aiCharacter ) {
	case AICHAR_ZOMBIE:
	case AICHAR_ZOMBIE_SURV:
	case AICHAR_ZOMBIE_FLAME:
	case AICHAR_ZOMBIE_GHOST:
		if ( first && ( g_entities[cs->entityNum].spawnflags & 4 ) ) {
			return AIFunc_FlameZombie_PortalStart( cs );
		}
		break;
	default:
		break;
	}

	// If we already have an enemy, enter combat unless we just came from battle
	if ( cs->enemyNum >= 0 && cs->aifunc != AIFunc_Battle ) {
		rval = AIFunc_BattleStart( cs );
	}

	if ( rval ) {
		return rval;
	}

	return AIFunc_IdleStart( cs );
}