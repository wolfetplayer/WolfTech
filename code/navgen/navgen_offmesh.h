// navgen_offmesh.h -- finds gaps a walkable-surface bake can't connect (steep stairs, ledges, short drops) and proposes off-mesh links to bridge them. See navgen_offmesh.cpp.

#ifndef __NAVGEN_OFFMESH_H
#define __NAVGEN_OFFMESH_H

#include <vector>
#include "navcache_format.h"
#include "navgen_geom.h"

// one already-baked tile layer blob (same shape navgen_bake.cpp builds before writing to disk).
struct NavGenTileBlob {
	int tx, ty, tlayer;
	unsigned char *data; // not owned/freed by NavGen_FindOffMeshConns
	int dataSize;
};

// proposes bridging connections between unreachable polygon regions, validated against geom so a link can't cut through a wall.
void NavGen_FindOffMeshConns( const NavCacheHeader &header,
							   const std::vector<NavGenTileBlob> &tiles,
							   const navGeom_t *geom,
							   std::vector<NavCacheOffMeshConn> &outConns );

#endif // __NAVGEN_OFFMESH_H
