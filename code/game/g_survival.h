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



// g_survival.h

#ifndef __G_SURVIVAL_H__
#define __G_SURVIVAL_H__

// Everything related to the score system
void Survival_AwardScore(gentity_t *player, int amount);
void Survival_AddKillScore(gentity_t *attacker, gentity_t *victim, int meansOfDeath);
void Survival_AddHeadshotBonus(gentity_t *attacker, gentity_t *victim);
void Survival_AddPainScore(gentity_t *attacker, gentity_t *victim, int damage);
void Survival_PickupTreasure(gentity_t *other);
qboolean Survival_TrySpendMG42Points(gentity_t *player);

// Purchase system
void Use_Target_buy(gentity_t *ent, gentity_t *other, gentity_t *activator);
qboolean Survival_HandleRandomPerkBox(gentity_t *ent, gentity_t *activator, char **itemName, int *itemIndex);
qboolean Survival_HandleAmmoPurchase(gentity_t *ent, gentity_t *activator, int price);
qboolean Survival_HandleWeaponOrGrenade(gentity_t *ent, gentity_t *activator, gitem_t *item, int price);
qboolean Survival_HandleWeaponUpgrade(gentity_t *ent, gentity_t *activator, int price) ;
qboolean Survival_HandleArmorPurchase(gentity_t *activator, gitem_t *item, int price);
qboolean Survival_HandlePerkPurchase(gentity_t *activator, gitem_t *item, int price);
int Survival_GetDefaultWeaponPrice(int weapon);
int Survival_GetDefaultPerkPrice(int perk);
void Touch_objective_info ( gentity_t * ent , gentity_t * other , trace_t * trace ) ;

// Misc stuff
void TossClientItems_Survival(gentity_t *self, gentity_t *attacker);
void TossClientPowerups(gentity_t *self, gentity_t *attacker);
gentity_t *SelectSpawnPoint_AI ( gentity_t *player, gentity_t *ent, vec3_t origin, vec3_t angles ) ;
void AICast_TickSurvivalWave( void );
void Survival_CheckWipe( void );

// Mapper-configurable special-wave enemy pool (set via the "specialWaveType" key
// on the game_manager entity, e.g. "loper_special zombie_ghost")
#define MAX_SPECIAL_WAVE_TYPES 4
void Survival_ParseSpecialWaveTypes( const char *str );
qboolean AICast_IsSpecialWaveChar( int aiChar );

// Per-character health/speed-scaling curve used by AICast_ApplySurvivalAttributes(); leave runSpeedCap at 0 to skip speed scaling for a character.
typedef struct survCharCurve_s
{
	int healthBaseEarly;
	int healthPerStep;
	int healthGrowthBase;
	float healthGrowthRate;
	int healthCap;      // 0 = no cap configured (still gated by g_survivalAiHealthCap)

	float runSpeedBase, runSpeedPerStep, runSpeedCap;
	float sprintSpeedBase, sprintSpeedPerStep, sprintSpeedCap;
	float crouchSpeedBase, crouchSpeedPerStep, crouchSpeedCap;
} survCharCurve_t;

// One g_survivalDifficulty tier of a character's AI skill values, used by BG_SetBehaviorForSurvival().
typedef struct survCharSkillTier_s
{
	float aimSkillBase, aimSkillCap;
	float aimAccuracyBase, aimAccuracyCap;
	float attackSkillBase, attackSkillCap;
	float aggressionBase, aggressionCap;
	float reactionTimeBase, reactionTimeFloor;
} survCharSkillTier_t;

typedef struct survCharSkill_s
{
	survCharSkillTier_t easy;   // g_survivalDifficulty 0
	survCharSkillTier_t hard;   // g_survivalDifficulty 1
} survCharSkill_t;

// Survival balance config, loaded server-side only from maps/survival_default.surv + maps/<mapname>.surv - see g_survival_config.c.
typedef struct survConfig_s
{
	int prepareTime;
	int intermissionTime;
	int initialKillCountReq;
	int friendlySpawnTime;

	int waveEliteGuard;
	int waveTrench;
	int waveBlackguard;
	int waveVenom;
	int waveProto;
	int waveWarriors;
	int waveHelga;
	int waveHeinrich;
	int waveLopers;
	int waveFlamers;

	int initialSoldiers;
	int initialTrench;
	int initialZombies;
	int initialFlamers;
	int initialWarriors;
	int initialProtos;
	int initialPartisan;
	int initialElites;
	int initialBlackguards;
	int initialVenoms;
	int initialLopers;
	int initialHelgas;
	int initialHeinrichs;

	int maxSoldiers, soldiersIncrease;
	int maxZombies, zombiesIncrease;
	int maxFlamers, flamersIncrease;
	int maxWarriors, warriorsIncrease;
	int maxElites, elitesIncrease;
	int maxTrench, trenchIncrease;
	int maxBlackguards, blackguardsIncrease;
	int maxHelgas, helgasIncrease;
	int maxHeinrichs, heinrichsIncrease;
	int maxLopers, lopersIncrease;
	int maxVenoms, venomsIncrease;
	int maxProtos, protosIncrease;

	int specialWaveChance;
	int specialWaveMinStart;
	int specialWaveMinGap;
	int specialWaveMaxGap;
	int specialWaveCountInitial;
	int specialWaveCountIncrease;
	int specialWaveCountMax;

	int ghostTeleportMinRange;
	int ghostTeleportMaxRange;
	int ghostTeleportDelay;

	int perksLimit;
	int perksLimitEngineer;
	int priceArmor;
	int priceWeaponUpgrade;
	int priceAmmoUpgraded;
	int priceRandomWeapon;
	int priceRandomPerk;

	int rwbSpinDuration;
	int rwbSpinIntervalMin;
	int rwbSpinIntervalMax;
	int rwbDecisionTime;
	int rwbBlinkStart;
	int rwbBlinkInterval;
	int rwbFloatHeight;
	int rwbBobHeight;
	int rwbBobSpeed;

	// Wave count where each character's health formula switches from linear to exponential; also used for the hard-mode rawSteps bonus below.
	int growthCurveWaveThreshold;

	// g_survivalDifficulty==1 padding added to rawSteps before the health formula runs: rawSteps += base + rawSteps/divisor.
	int hardStepsBonusBaseEarly, hardStepsBonusDivisorEarly;
	int hardStepsBonusBaseLate, hardStepsBonusDivisorLate;

	// Per-character health/speed curves, indexed by AICharacters_t; set via "<charname>.<field>" keys, e.g. "zombie_ghost.health_cap 300".
	survCharCurve_t charCurve[NUM_CHARACTERS];

	// Per-character AI skill tiers, indexed by AICharacters_t; set via "<charname>.easy_<field>"/"<charname>.hard_<field>" keys.
	survCharSkill_t charSkill[NUM_CHARACTERS];

	// -1 = no override; set via "weapon_price.<name> <price>" / "perk_price.<name> <price>".
	int weaponPrice[WP_NUM_WEAPONS];
	int perkPrice[NUM_PERKS];
} survConfig_t;

extern survConfig_t survCfg;

// Loads survCfg; call once before anything reads it (currently: top of AICast_InitSurvival).
void Survival_LoadConfig( void );


// Survival parameters
typedef struct svParams_s
{
	// not loaded
	int activeAI[NUM_CHARACTERS];
	int survivalKillCount;
	int maxActiveAI[NUM_CHARACTERS];
	int waveCount;
	int waveKillCount;
	int killCountRequirement;

	int spawnedThisWave;
	int spawnedThisWaveFriendly;
	qboolean wavePending;
    int waveChangeTime;
	qboolean waveInProgress;

	qboolean specialWaveActive;
	int lastSpecialWave;
	qboolean forceSpecialWaveNext;       // dev/test override, set by "forcespecialwave" client command

	int specialWaveTypes[MAX_SPECIAL_WAVE_TYPES];  // AICharacters_t pool, mapper-configurable
	int numSpecialWaveTypes;
	int currentSpecialWaveType;                    // AICharacters_t featured in the active special wave

	qboolean waveGameOver;              // all players died mid-wave, game over triggered

	// loaded from .surv file
	int   aliveFriendliestoCallReinforce;

	float soldierExplosiveDmgBonus;

} svParams_t;

extern svParams_t svParams;

#endif // __G_SURVIVAL_H__