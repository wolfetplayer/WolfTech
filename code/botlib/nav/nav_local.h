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

#include "../../navgen/navgen_classes.h"

#define NAV_MAX_CLASSES NAVGEN_NUM_CLASSES

struct NavData_t {
	dtTileCache *cache;
	dtNavMesh *mesh;
	dtNavMeshQuery *query;
	dtTileCacheAlloc *alloc;
	dtTileCacheCompressor *compressor;
	dtTileCacheMeshProcess *meshProcess;
	bool loaded;
};

extern NavData_t navData[NAV_MAX_CLASSES];
extern int navCurrentClass;

// current class's loaded data, or NULL if it failed/hasn't loaded.
NavData_t *Nav_CurrentData( void );

#endif // __NAV_LOCAL_H
