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

// g_survival_buy.c

#include "g_local.h"
#include "g_survival.h"


#define PRICE_RANDOM_WEAPON 150
#define PRICE_RANDOM_PERK   250
#define PRICE_WEAPON_UPGRADE 1000
#define PRICE_AMMO_UPGRADED 500

#define PERKS_LIMIT_ENGINEER 4
#define PERKS_LIMIT          3

#define PRICE_ARMOR 150

int Survival_GetDefaultWeaponPrice(int weapon) {
	switch (weapon) {
		case WP_KNIFE:        return 10;
		// Pistols
		case WP_LUGER:        return 30;
		case WP_SILENCER:     return 30;
		case WP_COLT:         return 30;
		case WP_AKIMBO:       return 60;
		case WP_REVOLVER:     return 80;

		// SMGs
		case WP_STEN:         return 90;
		case WP_MP40:         return 100;
		case WP_MP34:         return 120;
		case WP_THOMPSON:     return 120;
		case WP_PPSH:         return 130;

		// Rifles
		case WP_MAUSER:       return 50;
		case WP_MOSIN:        return 60;
		case WP_SNIPERRIFLE:  return 100;
		case WP_SNOOPERSCOPE: return 150;
		case WP_M1GARAND:     return 150;
		case WP_G43:          return 150;

		// Auto Rifles

		// Assault Rifles
		case WP_FG42:         return 250;
		case WP_MP44:         return 250;
		case WP_BAR:          return 250;

		// Shotguns
		case WP_M97:          return 200;

		// Heavy
		case WP_PANZERFAUST:  return 400;
		case WP_FLAMETHROWER: return 500;
		case WP_VENOM:        return 500;
		case WP_TESLA:        return 500;
		case WP_MG42M:        return 500;
		case WP_BROWNING:     return 500;

		// Grenades
		case WP_GRENADE_LAUNCHER:   return 100;
		case WP_GRENADE_PINEAPPLE:  return 100;

		default: return 100;
	}
}

/*
============
Survival_HandleRandomWeaponBox
============
*/
qboolean Survival_HandleRandomWeaponBox(gentity_t *ent, gentity_t *activator, char *itemName, int *itemIndex) {
	if (!activator || !activator->client) return qfalse;

	static const weapon_t random_box_weapons[] = {
		WP_LUGER, WP_SILENCER, WP_COLT,
		WP_AKIMBO, WP_REVOLVER, WP_MP40, WP_THOMPSON, WP_STEN, WP_MP34, WP_PPSH,
		WP_MAUSER, WP_MOSIN, WP_SNIPERRIFLE, WP_SNOOPERSCOPE, WP_FG42,
		WP_M1GARAND, WP_G43, WP_MP44, WP_BAR, WP_M97,
		WP_PANZERFAUST, WP_FLAMETHROWER, WP_VENOM, WP_TESLA,
		WP_MG42M, WP_BROWNING
	};


    const weapon_t *selected_weapons = random_box_weapons;
	int numWeapons = sizeof(random_box_weapons) / sizeof(random_box_weapons[0]);

	int price = ent->price > 0 ? ent->price : PRICE_RANDOM_WEAPON;

	if (activator->client->ps.persistant[PERS_SCORE] < price) {
		trap_SendServerCommand(-1, "mu_play sound/items/use_nothing.wav 0\n");
		return qfalse;
	}

	// Pick a random weapon the player doesn't have
	weapon_t chosen;
	int tries = 20;
	do {
		chosen = selected_weapons[rand() % numWeapons];
		tries--;

		if ( svParams.waveCount < 5 &&
			( chosen == WP_TESLA || chosen == WP_VENOM || chosen == WP_FLAMETHROWER || chosen == WP_MG42M || chosen == WP_BROWNING ) ) {
			continue;
		}
	} while ( ( G_FindWeaponSlot( activator, chosen ) >= 0 ||
		( svParams.waveCount < 5 &&
		( chosen == WP_TESLA || chosen == WP_VENOM || chosen == WP_FLAMETHROWER || chosen == WP_MG42M || chosen == WP_BROWNING ) ) ) && tries > 0 );

	if (tries <= 0) {
		trap_SendServerCommand(-1, "mu_play sound/items/use_nothing.wav 0\n");
		return qfalse;
	}

	// Find the item
	for (int i = 1; bg_itemlist[i].classname; i++)
	{
		if (bg_itemlist[i].giWeapon != chosen)
			continue;

		*itemIndex = i;
		itemName = bg_itemlist[i].classname;
		gitem_t *item = &bg_itemlist[i];

		// Give weapon
		Give_Weapon_New_Inventory(activator, chosen, qfalse);

		// Give full ammo (twice to fill both reserve and clip)
		int maxAmmo = BG_GetMaxAmmo(&activator->client->ps, chosen, LT_AMMO_BONUS_MULTIPLIER);
		Add_Ammo(activator, chosen, maxAmmo, qtrue);  // fill clip
		Add_Ammo(activator, chosen, maxAmmo, qfalse); // top off reserve


		// Bonus: give M7 for Garand
		if (chosen == WP_M1GARAND)
		{
			Give_Weapon_New_Inventory(activator, WP_M7, qfalse);
			int m7MaxAmmo = BG_GetMaxAmmo(&activator->client->ps, WP_M7, LT_AMMO_BONUS_MULTIPLIER);
			Add_Ammo(activator, WP_M7, m7MaxAmmo, qfalse);
		}

		// Select weapon
		activator->client->ps.weapon = chosen;
		activator->client->ps.weaponstate = WEAPON_READY;

		// Deduct points
		Survival_AwardScore(activator, -price);

		// SFX & confirmation
		G_AddPredictableEvent(activator, EV_ITEM_PICKUP, item - bg_itemlist);
		trap_SendServerCommand(-1, "mu_play sound/misc/buy.wav 0\n");

		return qtrue;
	}

	return qfalse;
}

/*
============
Survival_HandleRandomPerkBox
============
*/
qboolean Survival_HandleRandomPerkBox(gentity_t *ent, gentity_t *activator, char **itemName, int *itemIndex) {
	if (!activator || !activator->client) return qfalse;

	static char *random_perks[] = {
		"perk_resilience", "perk_scavenger", "perk_runner",
		"perk_weaponhandling", "perk_rifling", "perk_secondchance"
	};

	int price = (ent->price > 0) ? ent->price : PRICE_RANDOM_PERK;
	const int numPerks = sizeof(random_perks) / sizeof(random_perks[0]);

	// Perk count limit (only matters for NEW perks, upgrades do not consume a slot)
	int perkCount = 0;
	for (int i = 0; i < MAX_PERKS; i++) {
		if (activator->client->ps.perks[i] > 0)
			perkCount++;
	}
	int maxPerks = (activator->client->ps.stats[STAT_PLAYER_CLASS] == PC_ENGINEER) ? PERKS_LIMIT_ENGINEER : PERKS_LIMIT;

	// Not enough score?
	if (activator->client->ps.persistant[PERS_SCORE] < price) {
		G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
		return qfalse;
	}

	// Find a valid outcome (reroll a few times to avoid wasting money on PRO-owned perks)
	for (int tries = 0; tries < 16; tries++) {

		int randomIndex = rand() % numPerks;
		*itemName = random_perks[randomIndex];

		for (int i = 1; bg_itemlist[i].classname; i++) {
			if (!Q_strcasecmp(*itemName, bg_itemlist[i].classname)) {
				*itemIndex = i;
				gitem_t *perkItem = &bg_itemlist[i];

				int perk = perkItem->giTag;
				int level = activator->client->ps.perks[perk];

				// Already PRO? reroll
				if (level >= 2) {
					break;
				}

				// New perk but no free slots? reroll (upgrades are still allowed)
				if (level <= 0 && perkCount >= maxPerks) {
					break;
				}

				// Apply: base (0->1) or pro (1->2)
				if (level <= 0) {
					activator->client->ps.perks[perk] = 1;
				} else {
					activator->client->ps.perks[perk] = 2;
				}

				activator->client->ps.stats[STAT_PERK] |= (1 << perk);
				Survival_AwardScore(activator, -price);

				G_AddPredictableEvent(activator, EV_ITEM_PICKUP, perkItem - bg_itemlist);
				trap_SendServerCommand(-1, "mu_play sound/misc/buy_perk.wav 0\n");
				return qtrue;
			}
		}
	}

	G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
	return qfalse;
}
/*
============
Survival_HandleAmmoPurchase
============
*/
qboolean Survival_HandleAmmoPurchase(gentity_t *ent, gentity_t *activator, int price) {
	if (!activator || !activator->client)
		return qfalse;

	int heldWeap = activator->client->ps.weapon;
	int upgradeLevel;
	if (heldWeap <= WP_NONE || heldWeap >= WP_NUM_WEAPONS)
		return qfalse;

	// Skip utility weapons
	//if (heldWeap == WP_DYNAMITE_ENG || heldWeap == WP_AIRSTRIKE || heldWeap == WP_POISONGAS || heldWeap == WP_SMOKE_BOMB)
	//	return qfalse;

	int ammoIndex = BG_FindAmmoForWeapon(heldWeap);
	if (ammoIndex < 0)
		return qfalse;

	// Use upgrade-aware max ammo
	int maxAmmo = BG_GetMaxAmmo(&activator->client->ps, heldWeap, LT_AMMO_BONUS_MULTIPLIER);

	// Check if already full
	if (activator->client->ps.ammo[ammoIndex] >= maxAmmo)
		return qfalse;

	// Base price fallback: half weapon price
	int basePrice = Survival_GetDefaultWeaponPrice(heldWeap);
	int ammoPrice = basePrice / 2;

	upgradeLevel = activator->client->ps.weaponUpgraded[heldWeap];

	// Upgrade modifier
	if (price <= 0) {
		if (upgradeLevel >= 1) {
			ammoPrice = PRICE_AMMO_UPGRADED* upgradeLevel;
		}
	}

	// Mapper override
	if (price > 0) {
		ammoPrice = price;
	}

	// Check score
	if (activator->client->ps.persistant[PERS_SCORE] < ammoPrice) {
		trap_SendServerCommand(-1, "mu_play sound/items/use_nothing.wav 0\n");
		return qfalse;
	}

	// Refill ammo and clip using upgrade-aware cap
	Add_Ammo(activator, heldWeap, maxAmmo, qtrue);
	Add_Ammo(activator, heldWeap, maxAmmo, qfalse);

	// Deduct score
	Survival_AwardScore(activator, -ammoPrice);

	trap_SendServerCommand(-1, "mu_play sound/misc/buy.wav 0\n");
	return qtrue;
}

/*
============
Survival_HandleWeaponUpgrade
============
*/
qboolean Survival_HandleWeaponUpgrade(gentity_t *ent, gentity_t *activator, int price)
{
	playerState_t *ps = &activator->client->ps;
	int weap = ps->weapon;
	int currentLevel;
	int maxUpgradeLevel = 3;
	int baseUpgradePrice;
	int upgradePrice;

	if (weap <= WP_NONE || weap >= WP_NUM_WEAPONS)
		return qfalse;

	// Weapons that cannot be upgraded
	if ( weap == WP_KNIFE || weap == WP_SNIPERRIFLE || weap == WP_FG42SCOPE || weap== WP_SNOOPERSCOPE  || weap == WP_DYNAMITE  || weap == WP_GRENADE_LAUNCHER || weap == WP_GRENADE_PINEAPPLE ) 
	{
		G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
		return qfalse;
	}

	currentLevel = ps->weaponUpgraded[weap];

	// Already fully upgraded
	if (currentLevel >= maxUpgradeLevel)
	{
		G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
		return qfalse;
	}

	// Use fallback base price
	baseUpgradePrice = PRICE_WEAPON_UPGRADE;

	// Mapper override
	if (price > 0)
	{
		baseUpgradePrice = price;
	}

	// Level 1 = x1, Level 2 = x2, Level 3 = x3
	upgradePrice = baseUpgradePrice * (currentLevel + 1);

	if (activator->client->ps.persistant[PERS_SCORE] < upgradePrice)
	{
		G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
		return qfalse;
	}

	ps->weaponUpgraded[weap]++;

	// If main weapon is upgraded upgrade alt too

	if (weap == WP_MAUSER)
		ps->weaponUpgraded[WP_SNIPERRIFLE] = ps->weaponUpgraded[weap];

	if (weap == WP_FG42)
		ps->weaponUpgraded[WP_FG42SCOPE] = ps->weaponUpgraded[weap];

	Survival_AwardScore(activator, -upgradePrice);

	// Refill ammo
	Add_Ammo(activator, weap, BG_GetMaxAmmo(&activator->client->ps, weap, LT_AMMO_BONUS_MULTIPLIER), qtrue);
	Add_Ammo(activator, weap, BG_GetMaxAmmo(&activator->client->ps, weap, LT_AMMO_BONUS_MULTIPLIER), qfalse);

	trap_SendServerCommand(-1, "mu_play sound/misc/wpn_upgrade.wav 0\n");
	return qtrue;
}
/*
============
Survival_HandleWeaponOrGrenade
============
*/
qboolean Survival_HandleWeaponOrGrenade(gentity_t *ent, gentity_t *activator, gitem_t *item, int price) {
	if (!activator || !item) return qfalse;

	const int weapon = item->giTag;
	const int ammoIndex = BG_FindAmmoForWeapon(weapon);
	int maxAmmo;
	int upgradeLevel;

	if (weapon <= WP_NONE || weapon >= WP_NUM_WEAPONS || ammoIndex < 0)
		return qfalse;

	// Use fallback price if undefined
	if (price <= 0) {
		price = Survival_GetDefaultWeaponPrice(weapon);
	}

	upgradeLevel = activator->client->ps.weaponUpgraded[weapon];

	// Special handling: grenades (no new weapon granted)
	if (item->giType == IT_AMMO && (
		weapon == WP_GRENADE_LAUNCHER ||
		weapon == WP_GRENADE_PINEAPPLE
	)) {
		maxAmmo = BG_GetMaxAmmo(&activator->client->ps, weapon, LT_AMMO_BONUS_MULTIPLIER);

		if (activator->client->ps.ammoclip[weapon] >= maxAmmo) {
			return qfalse; // Already full
		}

		if (COM_BitCheck(activator->client->ps.weapons, weapon)) {
			if (upgradeLevel >= 1) {
				price = PRICE_AMMO_UPGRADED* upgradeLevel;
			} else {
				price /= 2; // Discount if already owned
			}
		}

		if (activator->client->ps.persistant[PERS_SCORE] < price) {
			G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
			return qfalse;
		}

		Survival_AwardScore(activator, -price);
		Add_Ammo(activator, weapon, maxAmmo, qtrue);
		Add_Ammo(activator, weapon, maxAmmo, qfalse);

		G_AddPredictableEvent(activator, EV_ITEM_PICKUP, item - bg_itemlist);
		trap_SendServerCommand(-1, "mu_play sound/misc/buy.wav 0\n");

		return qtrue;
	}

	// Already owns weapon — refill ammo only
	if (COM_BitCheck(activator->client->ps.weapons, weapon)) {
		// Adjust refill price
		if (upgradeLevel >= 1) {
			price = PRICE_AMMO_UPGRADED * upgradeLevel;
		} else {
			price /= 2;
		}

		if (activator->client->ps.persistant[PERS_SCORE] < price) {
			G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
			return qfalse;
		}

		maxAmmo = BG_GetMaxAmmo(&activator->client->ps, weapon, LT_AMMO_BONUS_MULTIPLIER);
		if (activator->client->ps.ammo[weapon] >= maxAmmo) {
			G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
			return qfalse; // Already full
		}

		Survival_AwardScore(activator, -price);

		Add_Ammo(activator, weapon, maxAmmo, qtrue);
		Add_Ammo(activator, weapon, maxAmmo, qfalse);

		G_AddPredictableEvent(activator, EV_ITEM_PICKUP, item - bg_itemlist);
		trap_SendServerCommand(-1, "mu_play sound/misc/buy.wav 0\n");

		return qtrue;
	}

	// Buying a new weapon
	if (activator->client->ps.persistant[PERS_SCORE] < price) {
		G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
		return qfalse;
	}

	Survival_AwardScore(activator, -price);

	Give_Weapon_New_Inventory(activator, weapon, qfalse);

	maxAmmo = BG_GetMaxAmmo(&activator->client->ps, weapon, LT_AMMO_BONUS_MULTIPLIER);
	Add_Ammo(activator, weapon, maxAmmo, qtrue);
	Add_Ammo(activator, weapon, maxAmmo, qfalse);

	G_AddPredictableEvent(activator, EV_ITEM_PICKUP, item - bg_itemlist);
	trap_SendServerCommand(-1, "mu_play sound/misc/buy.wav 0\n");

	return qtrue;
}
/*
============
Survival_HandleArmorPurchase
============
*/
qboolean Survival_HandleArmorPurchase(gentity_t *activator, gitem_t *item, int price) {
	if (!activator || !item || !activator->client) return qfalse;

	if (activator->client->ps.stats[STAT_ARMOR] >= 200)
		return qfalse;

	// Fallback price if not set by mapper
	if (price <= 0)
		price = PRICE_ARMOR;

	// Check score
	if (activator->client->ps.persistant[PERS_SCORE] < price) {
		trap_SendServerCommand(-1, "mu_play sound/items/use_nothing.wav 0\n");
		return qfalse;
	}

	// Deduct, apply, and notify
	Survival_AwardScore(activator, -price);
	activator->client->ps.stats[STAT_ARMOR] = 200;

	G_AddPredictableEvent(activator, EV_ITEM_PICKUP, item - bg_itemlist);
	trap_SendServerCommand(-1, "mu_play sound/misc/buy.wav 0\n");

	return qtrue;
}

/*
============
Survival_GetDefaultPerkPrice
============
*/
int Survival_GetDefaultPerkPrice(int perk) {
	switch (perk) {
		case PERK_SECONDCHANCE:    return 150;
		case PERK_RUNNER:          return 200;
		case PERK_SCAVENGER:       return 250;
		case PERK_WEAPONHANDLING:  return 300;
		case PERK_RIFLING:         return 350;
		case PERK_RESILIENCE:      return 400;
		default:                   return 200;
	}
}

#define PERK_LEVEL_NONE  0
#define PERK_LEVEL_BASE  1
#define PERK_LEVEL_PRO   2

/*
============
Survival_HandlePerkPurchase
============
*/
qboolean Survival_HandlePerkPurchase(gentity_t *activator, gitem_t *item, int price) {
    if (!activator || !item || item->giType != IT_PERK)
        return qfalse;

    int perk = item->giTag;
    int curLevel = activator->client->ps.perks[perk];

    // Determine what we're buying: base (0->1) or pro (1->2)
    int targetLevel = 0;
    if (curLevel <= 0) {
        targetLevel = 1;          // buy base
    } else if (curLevel == 1) {
        targetLevel = 2;          // upgrade to pro
    } else {
        return qfalse;            // already pro (or higher)
    }

    // Only enforce max perks when buying a NEW perk (0->1).
    if (targetLevel == 1) {
        int perkCount = 0;
        for (int i = 0; i < MAX_PERKS; i++) {
            if (activator->client->ps.perks[i] > 0)
                perkCount++;
        }

        int maxPerks = (activator->client->ps.stats[STAT_PLAYER_CLASS] == PC_ENGINEER) ?
            PERKS_LIMIT_ENGINEER : PERKS_LIMIT;

        if (perkCount >= maxPerks)
            return qfalse;
    }

    // Base price: entity override if >0, otherwise from .surv (svParams.*)
    if (price <= 0) {
        price = Survival_GetDefaultPerkPrice(perk);
    }

    // PRO costs double base price
    if (targetLevel == 2) {
        price *= 2;
    }

    // Not enough score?
    if (activator->client->ps.persistant[PERS_SCORE] < price) {
        G_AddEvent(activator, EV_GENERAL_SOUND, G_SoundIndex("sound/items/use_nothing.wav"));
        return qfalse;
    }

    // Grant / upgrade perk
    activator->client->ps.perks[perk] = targetLevel;
    activator->client->ps.stats[STAT_PERK] |= (1 << perk);
    Survival_AwardScore(activator, -price);

    G_AddPredictableEvent(activator, EV_ITEM_PICKUP, item - bg_itemlist);
    trap_SendServerCommand(-1, "mu_play sound/misc/buy_perk.wav 0\n");

    return qtrue;
}


/*QUAKED target_buy (1 0 0) (-8 -8 -8) (8 8 8)
Gives the activator all the items pointed to.
*/
void Use_Target_buy(gentity_t *ent, gentity_t *other, gentity_t *activator) {
	if (!activator || !activator->client || !ent->buy_item) return;

	int itemIndex = 0;
	char *itemName = ent->buy_item;
	int price = (ent->price > 0) ? ent->price : 0;
	int clientNum = activator->client->ps.clientNum;
	gitem_t *item = NULL;
	qboolean success = qfalse;

	// Special case: ammo
	if (!Q_stricmp(itemName, "ammo")) {
		if (Survival_HandleAmmoPurchase(ent, activator, price)) {
			// Survival_HandleAmmoPurchase already deducts its own (upgrade-aware) price internally
			ClientUserinfoChanged(clientNum);
		}
		return;
	}

	// Special case: random weapon
	if (!Q_stricmp(itemName, "random_weapon")) {
		success = Survival_HandleRandomWeaponBox(ent, activator, itemName, &itemIndex);
		if (success) {
			ClientUserinfoChanged(clientNum);
		}
		return; // Don't flow into generic weapon handling
	}

	// Special case: upgrade weapon
	if (!Q_stricmp(itemName, "upgrade_weapon"))
	{
		if (Survival_HandleWeaponUpgrade(ent, activator, price))
		{
			ClientUserinfoChanged(clientNum);
		}
		return;
	}

	// Special case: random perk
	if (!Q_stricmp(itemName, "random_perk")) {
		success = Survival_HandleRandomPerkBox(ent, activator, &itemName, &itemIndex);
		if (success) {
			ClientUserinfoChanged(clientNum);
		}
		return;
	}

	// Fallback: find item by name
	if (itemIndex <= 0) {
		for (int i = 1; bg_itemlist[i].classname; i++) {
			if (!Q_strcasecmp(itemName, bg_itemlist[i].classname)) {
				itemIndex = i;
				break;
			}
		}
	}

	if (itemIndex <= 0) return;
	item = &bg_itemlist[itemIndex];

	// Not enough points?
	if (activator->client->ps.persistant[PERS_SCORE] < price) {
		trap_SendServerCommand(-1, "mu_play sound/items/use_nothing.wav 0\n");
		return;
	}

	switch (item->giType) {
		case IT_WEAPON:
		case IT_AMMO:
			success = Survival_HandleWeaponOrGrenade(ent, activator, item, price);
			break;
		case IT_ARMOR:
			success = Survival_HandleArmorPurchase(activator, item, price);
			break;
		case IT_PERK:
			success = Survival_HandlePerkPurchase(activator, item, price);
			break;
		default:
			return;
	}

	if (success) {
		ClientUserinfoChanged(clientNum);
	}
}

#define AXIS_OBJECTIVE      1
#define ALLIED_OBJECTIVE    2

void Touch_objective_info(gentity_t *ent, gentity_t *other, trace_t *trace) {
	gentity_t *buyEnt = NULL;
	int price = 0;
	int ammoPrice = 0;
	int isWeapon = ent->isWeapon;
	const char *weaponName = ent->translation;
	const char *techName = NULL;
	const gitem_t *item = NULL;
	int upgradeLevel = 0;

	if (other->aiCharacter)
	{
		return;
	}

	// Try to find the linked target_buy
	for (int i = 0; i < level.num_entities; i++)
	{
		buyEnt = &g_entities[i];
		if (!buyEnt->inuse)
			continue;
		if (Q_stricmp(buyEnt->classname, "target_buy") != 0)
			continue;
		if (ent->target && buyEnt->targetname && Q_stricmp(ent->target, buyEnt->targetname) == 0)
		{
			techName = buyEnt->buy_item;
			price = buyEnt->price; // This can be 0, and fallback will trigger
			break;
		}
	}

	// If no target_buy was found but a target exists, try to find linked func_invisible_user
	if (!techName && ent->target)
	{
		for (int i = 0; i < level.num_entities; i++)
		{
			gentity_t *funcUser = &g_entities[i];
			if (!funcUser->inuse)
				continue;
			if (Q_stricmp(funcUser->classname, "func_invisible_user") != 0)
				continue;
			if (funcUser->targetname && Q_stricmp(ent->target, funcUser->targetname) == 0)
			{
				price = funcUser->price;
				break;
			}
		}
	}

	if (other->client->ps.weapon > WP_NONE && other->client->ps.weapon < WP_NUM_WEAPONS)
	{
		upgradeLevel = other->client->ps.weaponUpgraded[other->client->ps.weapon];
	}

	// Handle special cases BEFORE item lookup
	if (techName) {
		if (!Q_stricmp(techName, "ammo")) {

		// Do not show price if holding dynamite
	//	if (other->client->ps.weapon == WP_DYNAMITE_ENG || other->client->ps.weapon == WP_POISONGAS || other->client->ps.weapon == WP_AIRSTRIKE ||  other->client->ps.weapon == WP_SMOKE_BOMB ) {
		//	return;
		//}
		
			price = (price > 0) ? price : Survival_GetDefaultWeaponPrice(other->client->ps.weapon) / 2;

			if (upgradeLevel >= 1)
			{
				price = PRICE_AMMO_UPGRADED * upgradeLevel;
			}

			if (weaponName && price > 0) {
				trap_SendServerCommand(other - g_entities, va(
					"cpbuy \"%s\nprice: %d\"",
					weaponName, price));
				return;
			}
		} else if (!Q_stricmp(techName, "random_weapon")) {
			price = (price > 0) ? price : PRICE_RANDOM_WEAPON;
			if (weaponName && price > 0) {
				trap_SendServerCommand(other - g_entities, va(
					"cpbuy \"%s\nprice: %d\"",
					weaponName, price));
				return;
			}
		}
		else if (!Q_stricmp(techName, "upgrade_weapon"))
		{
			if (upgradeLevel >= 3)
			{
				return;
			}

			if (price <= 0)
			{
				price = PRICE_WEAPON_UPGRADE * (upgradeLevel + 1);
			}

			if (weaponName && price > 0)
			{
				trap_SendServerCommand(other - g_entities, va(
															   "cpbuy \"%s\nprice: %d\"",
															   weaponName, price));
				return;
			}
		}
		else if (!Q_stricmp(techName, "random_perk"))
		{
			price = (price > 0) ? price : PRICE_RANDOM_PERK;
			if (weaponName && price > 0)
			{
				trap_SendServerCommand(other - g_entities, va(
															   "cpbuy \"%s\nprice: %d\"",
															   weaponName, price));
				return;
			}
		}
	}

	// Try to find the item definition
	if (techName) {
		item = BG_FindItem(techName);
	}

	if (!item && techName) {
		for (int i = 1; bg_itemlist[i].classname; i++) {
			if (!Q_stricmp(bg_itemlist[i].classname, techName)) {
				item = &bg_itemlist[i];
				break;
			}
		}
	}

	// If price not defined explicitly, fall back based on item type
	if (price <= 0 && item) {
		if (item->giType == IT_WEAPON || item->giType == IT_AMMO) {
			price = Survival_GetDefaultWeaponPrice(item->giTag);
		} else if (item->giType == IT_PERK) {
			price = Survival_GetDefaultPerkPrice(item->giTag);
		} else if (item->giType == IT_ARMOR) {
			price = PRICE_ARMOR;
		}
	}

	// Ammo price only applies to weapons
	ammoPrice = isWeapon ? price / 2 : 0;

	if (upgradeLevel >= 1)
	{
		ammoPrice = PRICE_AMMO_UPGRADED * upgradeLevel;
	}

	// Perk PRO tip override (dynamic string + dynamic price)
	if (item && item->giType == IT_PERK && weaponName) {
		int perkLevel = other->client->ps.perks[item->giTag];

		if (perkLevel <= 0) {
			if (price > 0) {
				trap_SendServerCommand(other - g_entities, va(
					"cpbuy \"%s\nprice: %d\"",
					weaponName, price));
				return;
			}
		} else if (perkLevel == 1) {
			if (price > 0) {
				trap_SendServerCommand(other - g_entities, va(
					"cpbuy \"%s ^PRO\nprice: %d\"",
					weaponName, price * 2));
				return;
			}
		} else {
			return;
		}
	}

	// Display custom tip if price and name are known
	if (price > 0 && weaponName) {
		if (isWeapon) {
			trap_SendServerCommand(other - g_entities, va(
				"cpbuy \"%s\nprice: %d\nammo_price: %d\"",
				weaponName, price, ammoPrice));
		} else {
			trap_SendServerCommand(other - g_entities, va(
				"cpbuy \"%s\nprice: %d\"",
				weaponName, price));
		}
		return;
	}

	// Otherwise fallback to standard objective info
	if (other->timestamp <= level.time) {
		other->timestamp = level.time + 4500;

		const char *msg = ent->track ? ent->track : va("objective #%i", ent->count);
		int teamFlag = (ent->spawnflags & AXIS_OBJECTIVE) ? 0 :
		               (ent->spawnflags & ALLIED_OBJECTIVE) ? 1 : -1;

		trap_SendServerCommand(other - g_entities, va("oid %d \"You are near %s\n\"", teamFlag, msg));
	}
}