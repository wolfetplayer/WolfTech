// nav_local.h -- only file allowed to include Recast/Detour headers.

#ifndef __NAV_LOCAL_H
#define __NAV_LOCAL_H

#include "../../recastnavigation/Recast/Include/Recast.h"
#include "../../recastnavigation/Detour/Include/DetourNavMesh.h"
#include "../../recastnavigation/Detour/Include/DetourNavMeshQuery.h"
#include "../../recastnavigation/Detour/Include/DetourNavMeshBuilder.h"
#include "../../recastnavigation/Detour/Include/DetourCommon.h"
#include "../../recastnavigation/DetourTileCache/Include/DetourTileCache.h"
#include "../../recastnavigation/DetourTileCache/Include/DetourTileCacheBuilder.h"
#include "../../recastnavigation/DetourCrowd/Include/DetourCrowd.h"

#include "../../navgen/navgen_classes.h"
#include "../../navgen/navcache_format.h"

#define NAV_MAX_CLASSES NAVGEN_NUM_CLASSES

// generous headroom over the realistic concurrent AI count (sv_maxclients defaults to 8).
#define NAV_CROWD_MAX_AGENTS 64
// matches the engine's absolute MAX_CLIENTS, kept local so this header avoids game headers.
#define NAV_MAX_TRACKED_AGENTS 128

struct NavData_t {
	dtTileCache *cache;
	dtNavMesh *mesh;
	dtNavMeshQuery *query;
	dtTileCacheAlloc *alloc;
	dtTileCacheCompressor *compressor;
	dtTileCacheMeshProcess *meshProcess;
	bool loaded;

	// local-avoidance crowd for this class's mesh; advisory only, see nav_query.cpp.
	dtCrowd *crowd;
	int crowdAgentIdx[NAV_MAX_TRACKED_AGENTS]; // agentId (entity number) -> crowd agent index, -1 if untracked
	unsigned char crowdStuckTicks[NAV_MAX_TRACKED_AGENTS]; // consecutive deadlocked calls, see Nav_MoveToGoal
};

extern NavData_t navData[NAV_MAX_CLASSES];
extern int navCurrentClass;

// current class's loaded data, or NULL if it failed/hasn't loaded.
NavData_t *Nav_CurrentData( void );

// looks up an off-mesh connection record by Detour's userID; NULL if out of range.
const NavCacheOffMeshConn *Nav_LookupOffMeshConn( NavData_t *data, unsigned int userID );

// drops every tracked obstacle handle; call before the caches are destroyed/recreated.
void Nav_ClearObstacles( void );

#endif // __NAV_LOCAL_H
