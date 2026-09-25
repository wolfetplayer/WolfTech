// nav_tilecache.cpp -- Phase 4: dynamic obstacles (doors/movers/constructibles) via DetourTileCache.

#include "nav_local.h"
#include "nav_public.h"

#include <cstring>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
}

#define NAV_MAX_OBSTACLES 256

// one call site's obstacle across every loaded class; a 0 ref means that class skipped it.
struct NavObstacleSlot_t {
	bool used;
	dtObstacleRef ref[NAV_MAX_CLASSES];
};

static NavObstacleSlot_t navObstacles[NAV_MAX_OBSTACLES];
static bool navObstaclesUpToDate[NAV_MAX_CLASSES];

// duplicated from nav_query.cpp: quake is Z-up, navmesh is Y-up.
static void SwapYZ( const float *in, float *out ) {
	float y = in[1];
	out[0] = in[0];
	out[1] = in[2];
	out[2] = y;
}

/*
===================
Nav_AddObstacle
===================
*/
int Nav_AddObstacle( const float *absmin, const float *absmax ) {
	int slot = -1;
	for ( int i = 0; i < NAV_MAX_OBSTACLES; i++ ) {
		if ( !navObstacles[i].used ) {
			slot = i;
			break;
		}
	}
	if ( slot < 0 ) {
		Com_Printf( "Nav_AddObstacle: obstacle table full\n" );
		return 0;
	}

	// SwapYZ doesn't re-sort min/max, so do that ourselves after the swap.
	vec3_t navA, navB, boxMin, boxMax;
	SwapYZ( absmin, navA );
	SwapYZ( absmax, navB );
	for ( int a = 0; a < 3; a++ ) {
		boxMin[a] = navA[a] < navB[a] ? navA[a] : navB[a];
		boxMax[a] = navA[a] > navB[a] ? navA[a] : navB[a];
	}

	NavObstacleSlot_t *obstacle = &navObstacles[slot];
	memset( obstacle->ref, 0, sizeof( obstacle->ref ) );
	bool anyAdded = false;

	for ( int c = 0; c < NAV_MAX_CLASSES; c++ ) {
		NavData_t *data = &navData[c];
		if ( !data->loaded || !data->cache ) {
			continue;
		}
		if ( dtStatusSucceed( data->cache->addBoxObstacle( boxMin, boxMax, &obstacle->ref[c] ) ) ) {
			anyAdded = true;
			navObstaclesUpToDate[c] = false;
		}
	}

	if ( !anyAdded ) {
		Com_DPrintf( "Nav_AddObstacle: rejected by every loaded class (bmin=%.0f %.0f %.0f bmax=%.0f %.0f %.0f)\n",
					 boxMin[0], boxMin[1], boxMin[2], boxMax[0], boxMax[1], boxMax[2] );
		return 0;
	}

	obstacle->used = true;
	Com_DPrintf( "Nav_AddObstacle: slot %d (bmin=%.0f %.0f %.0f bmax=%.0f %.0f %.0f)\n",
				 slot, boxMin[0], boxMin[1], boxMin[2], boxMax[0], boxMax[1], boxMax[2] );
	return slot + 1; // 0 is reserved for "no obstacle" (see gentity_t::navObstacleId)
}

/*
===================
Nav_RemoveObstacle
===================
*/
void Nav_RemoveObstacle( int handle ) {
	int slot = handle - 1;
	if ( slot < 0 || slot >= NAV_MAX_OBSTACLES || !navObstacles[slot].used ) {
		return;
	}

	NavObstacleSlot_t *obstacle = &navObstacles[slot];
	for ( int c = 0; c < NAV_MAX_CLASSES; c++ ) {
		NavData_t *data = &navData[c];
		if ( !data->loaded || !data->cache || obstacle->ref[c] == 0 ) {
			continue;
		}
		data->cache->removeObstacle( obstacle->ref[c] );
		navObstaclesUpToDate[c] = false;
	}
	obstacle->used = false;
}

/*
===================
Nav_ClearObstacles
===================
*/
void Nav_ClearObstacles( void ) {
	memset( navObstacles, 0, sizeof( navObstacles ) );
	// a fresh cache has nothing queued, so it's already up to date.
	for ( int c = 0; c < NAV_MAX_CLASSES; c++ ) {
		navObstaclesUpToDate[c] = true;
	}
}

/*
===================
Nav_UpdateObstacles

Called once per server frame (SV_BotFrame).
===================
*/
void Nav_UpdateObstacles( void ) {
	for ( int c = 0; c < NAV_MAX_CLASSES; c++ ) {
		NavData_t *data = &navData[c];
		if ( !data->loaded || !data->cache ) {
			continue;
		}

		bool upToDate = false;
		data->cache->update( 0.0f, data->mesh, &upToDate );

		if ( upToDate && !navObstaclesUpToDate[c] ) {
			Com_DPrintf( "Nav_UpdateObstacles: %s tile rebuild complete\n", navGenClasses[c].name );
		}
		navObstaclesUpToDate[c] = upToDate;
	}
}
