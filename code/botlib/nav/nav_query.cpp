// nav_query.cpp -- query implementations mapped onto the old trap_AAS_* taxonomy; see the migration plan for the mapping table.

#include "nav_local.h"
#include "nav_public.h"

#include <cstring>
#include <cstdio>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
}

// widened from {64,128,64} so characters slightly off the baked surface still snap to a poly.
static const float NAV_SEARCH_EXTENTS[3] = { 96, 160, 96 };
static const int NAV_MAX_CANDIDATES = 64;

// how close to a jump/drop link's takeoff point before we actually commit to it.
static const float NAV_OFFMESH_JUMP_RANGE = 48.0f;

// landing this much below the takeoff counts as a plain fall, not a jump.
static const float NAV_OFFMESH_DROP_TOLERANCE = 24.0f;

// the navmesh is built Y-up (see navgen_geom.cpp's AddVert); Quake is Z-up.
// Every Nav_* entry point converts at its boundary so callers stay in Quake
// space and all internal Detour calls stay in navmesh space. Swapping the
// same two axes twice is a no-op, so one function does both directions.
static void SwapYZ( const float *in, float *out ) {
	float y = in[1];
	out[0] = in[0];
	out[1] = in[2];
	out[2] = y;
}

/*
===========
Nav_FindNearest

pos and nearest are both in navmesh (Y-up) space.
===========
*/
static bool Nav_FindNearest( NavData_t *data, const float *pos, dtPolyRef *ref, float *nearest ) {
	dtQueryFilter filter;
	if ( dtStatusFailed( data->query->findNearestPoly( pos, NAV_SEARCH_EXTENTS, &filter, ref, nearest ) ) ) {
		return false;
	}
	return *ref != 0;
}

/*
====================
Nav_ComputeStraightPath

start, goal and straight are all in navmesh (Y-up) space. outFlags[i] carries
DT_STRAIGHTPATH_OFFMESH_CONNECTION when straight[i] is the takeoff point of a
bridged jump/step-across link (navgen_offmesh.cpp), not ordinary floor.
====================
*/
static bool Nav_ComputeStraightPath( NavData_t *data, const float *start, const float *goal,
									  float *straight, unsigned char *outFlags, dtPolyRef *outRefs,
									  int maxStraight, int *straightCount ) {
	dtQueryFilter filter;
	dtPolyRef startRef, endRef;
	float startNearest[3], endNearest[3];

	if ( !Nav_FindNearest( data, start, &startRef, startNearest ) ) {
		return false;
	}
	if ( !Nav_FindNearest( data, goal, &endRef, endNearest ) ) {
		return false;
	}

	// 512 polys: a smaller buffer makes findPath truncate on long cross-map routes, which looks like "unreachable".
	dtPolyRef path[512];
	int pathCount = 0;
	if ( dtStatusFailed( data->query->findPath( startRef, endRef, startNearest, endNearest, &filter, path, &pathCount, 512 ) ) || pathCount == 0 ) {
		return false;
	}
	// a partial path (ending short of endRef) means start/goal aren't connected, not a real route.
	if ( path[pathCount - 1] != endRef ) {
		return false;
	}

	if ( dtStatusFailed( data->query->findStraightPath( startNearest, endNearest, path, pathCount,
														 straight, outFlags, outRefs, straightCount, maxStraight ) ) ) {
		return false;
	}
	return *straightCount > 0;
}

/*
================
Nav_RaycastClear

true if a straight walkability ray from fromPos (in poly fromRef) reaches
toPos with no wall hit; used for real line-of-sight checks, not just distance.
================
*/
static bool Nav_RaycastClear( NavData_t *data, dtPolyRef fromRef, const float *fromPos, const float *toPos ) {
	dtQueryFilter filter;
	float t = 0.0f;
	float hitNormal[3];
	dtPolyRef path[64];
	int pathCount = 0;
	if ( dtStatusFailed( data->query->raycast( fromRef, fromPos, toPos, &filter, &t, hitNormal, path, &pathCount, 64 ) ) ) {
		return false;
	}
	return t >= 1.0f;
}

/*
==============
Nav_PointToPoly
==============
*/
int Nav_PointToPoly( const float *point ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navPoint;
	SwapYZ( point, navPoint );

	dtPolyRef ref;
	float nearest[3];
	return Nav_FindNearest( data, navPoint, &ref, nearest ) ? (int)ref : 0;
}

/*
============
Nav_Reachable
============
*/
int Nav_Reachable( const float *point ) {
	return Nav_PointToPoly( point ) != 0;
}

/*
=============
Nav_MoveToGoal
=============
*/
int Nav_MoveToGoal( navMoveResult_t *result, const float *start, const float *goal ) {
	memset( result, 0, sizeof( *result ) );

	NavData_t *data = Nav_CurrentData();
	vec3_t navStart, navGoal;
	SwapYZ( start, navStart );
	SwapYZ( goal, navGoal );

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( !data || !Nav_ComputeStraightPath( data, navStart, navGoal, straight, straightFlags, straightRefs, 32, &straightCount ) ) {
		result->failure = 1;
		return 0;
	}

	// off-mesh links get flagged one step early, at index 1, or it's too late to grab.
	int offMeshIdx = -1;
	if ( straightFlags[0] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) {
		offMeshIdx = 0;
	} else if ( straightCount > 1 && ( straightFlags[1] & DT_STRAIGHTPATH_OFFMESH_CONNECTION ) ) {
		offMeshIdx = 1;
	}
	bool offMeshCommitted = false; // close enough to the link to steer at its landing point
	if ( offMeshIdx >= 0 ) {
		const dtOffMeshConnection *conn = data->mesh->getOffMeshConnectionByRef( straightRefs[offMeshIdx] );
		if ( conn ) {
			const NavCacheOffMeshConn *rec = Nav_LookupOffMeshConn( data, conn->userId );
			if ( rec && rec->isLadder ) {
				result->onOffMeshConnection = 1;
				result->onLadderConnection = 1;
				SwapYZ( &conn->pos[0], result->ladderStart );
				SwapYZ( &conn->pos[3], result->ladderEnd );
				SwapYZ( rec->wallNormal, result->ladderWallNormal );
				return 1;
			}
		}
		// jump/drop links need to be close before we commit, unlike ladders above.
		if ( VectorDistance( navStart, &straight[offMeshIdx * 3] ) <= NAV_OFFMESH_JUMP_RANGE ) {
			offMeshCommitted = true;
			// a plain drop just needs a walk off the ledge, not a jump.
			float takeoffY = straight[offMeshIdx * 3 + 1];
			float landingY = ( offMeshIdx + 1 < straightCount ) ? straight[( offMeshIdx + 1 ) * 3 + 1] : takeoffY;
			if ( landingY >= takeoffY - NAV_OFFMESH_DROP_TOLERANCE ) {
				result->onOffMeshConnection = 1;
			}
		}
	}

	const float *target = ( straightCount > 1 ) ? &straight[3] : &straight[0];
	// committed: steer at the landing point, not the now-underfoot takeoff point.
	if ( offMeshCommitted && offMeshIdx + 1 < straightCount ) {
		target = &straight[( offMeshIdx + 1 ) * 3];
	}
	vec3_t navDir;
	VectorSubtract( target, navStart, navDir );
	// AICast_InputToUserCommand wants a horizontal dir (AAS's own "hordir"); Z feeds ucmd->upmove instead.
	navDir[1] = 0.0f; // navmesh Y = vertical (quake Z) after SwapYZ
	if ( VectorNormalize2( navDir, navDir ) < 0.0001f ) {
		result->failure = 1;
		return 0;
	}
	SwapYZ( navDir, result->movedir );
	return 1;
}

/*
=====================
Nav_TravelTimeEstimate

Returns path length in world units, not real travel time; good enough for
the relative "did this move help" comparisons ai_cast_funcs.c makes today.
Segment length is invariant under the Y/Z swap, so no conversion needed.
=====================
*/
int Nav_TravelTimeEstimate( const float *start, const float *goal ) {
	NavData_t *data = Nav_CurrentData();
	vec3_t navStart, navGoal;
	SwapYZ( start, navStart );
	SwapYZ( goal, navGoal );

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( !data || !Nav_ComputeStraightPath( data, navStart, navGoal, straight, straightFlags, straightRefs, 32, &straightCount ) ) {
		return -1;
	}

	float dist = 0.0f;
	for ( int i = 1; i < straightCount; i++ ) {
		vec3_t seg;
		VectorSubtract( &straight[i * 3], &straight[( i - 1 ) * 3], seg );
		dist += VectorLength( seg );
	}
	return (int)dist;
}

/*
==================
Nav_FindHidePosition

Farthest-from-threat candidate within radius that the threat can't actually
raycast to; falls back to farthest-by-distance if nothing is fully hidden.
==================
*/
int Nav_FindHidePosition( const float *from, const float *threat, float radius, float *outPos ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navFrom, navThreat;
	SwapYZ( from, navFrom );
	SwapYZ( threat, navThreat );

	dtQueryFilter filter;
	dtPolyRef startRef, threatRef;
	float startNearest[3], threatNearest[3];
	if ( !Nav_FindNearest( data, navFrom, &startRef, startNearest ) ) {
		return 0;
	}
	if ( !Nav_FindNearest( data, navThreat, &threatRef, threatNearest ) ) {
		return 0;
	}

	dtPolyRef resultRefs[NAV_MAX_CANDIDATES];
	dtPolyRef resultParents[NAV_MAX_CANDIDATES];
	float resultCosts[NAV_MAX_CANDIDATES];
	int resultCount = 0;
	if ( dtStatusFailed( data->query->findPolysAroundCircle( startRef, startNearest, radius, &filter,
															  resultRefs, resultParents, resultCosts, &resultCount, NAV_MAX_CANDIDATES ) ) ) {
		return 0;
	}

	bool found = false, foundHidden = false;
	float bestDist = -1.0f, bestHiddenDist = -1.0f;
	vec3_t bestPos = { 0, 0, 0 }, bestHiddenPos = { 0, 0, 0 };

	for ( int i = 0; i < resultCount; i++ ) {
		float pt[3];
		bool overPoly;
		if ( dtStatusFailed( data->query->closestPointOnPoly( resultRefs[i], startNearest, pt, &overPoly ) ) ) {
			continue;
		}

		vec3_t toThreat;
		VectorSubtract( pt, navThreat, toThreat );
		float dist = VectorLength( toThreat );
		if ( dist > bestDist ) {
			bestDist = dist;
			VectorCopy( pt, bestPos );
			found = true;
		}

		if ( dist > bestHiddenDist && !Nav_RaycastClear( data, threatRef, threatNearest, pt ) ) {
			bestHiddenDist = dist;
			VectorCopy( pt, bestHiddenPos );
			foundHidden = true;
		}
	}

	if ( foundHidden ) {
		SwapYZ( bestHiddenPos, outPos );
		return 1;
	}
	if ( found ) {
		SwapYZ( bestPos, outPos );
	}
	return found ? 1 : 0;
}

/*
=================
Nav_FindAttackSpot

Closest-to-"from" candidate whose range to target falls in [minRange,
maxRange] and that can actually raycast to target; falls back to
closest-in-range if nothing in range has line of sight.
=================
*/
int Nav_FindAttackSpot( const float *from, const float *target, float minRange, float maxRange, float *outPos ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navFrom, navTarget;
	SwapYZ( from, navFrom );
	SwapYZ( target, navTarget );

	dtQueryFilter filter;
	dtPolyRef startRef, targetRef;
	float startNearest[3], targetNearest[3];
	if ( !Nav_FindNearest( data, navFrom, &startRef, startNearest ) ) {
		return 0;
	}
	if ( !Nav_FindNearest( data, navTarget, &targetRef, targetNearest ) ) {
		return 0;
	}

	dtPolyRef resultRefs[NAV_MAX_CANDIDATES];
	dtPolyRef resultParents[NAV_MAX_CANDIDATES];
	float resultCosts[NAV_MAX_CANDIDATES];
	int resultCount = 0;
	if ( dtStatusFailed( data->query->findPolysAroundCircle( startRef, startNearest, maxRange, &filter,
															  resultRefs, resultParents, resultCosts, &resultCount, NAV_MAX_CANDIDATES ) ) ) {
		return 0;
	}

	bool found = false, foundVisible = false;
	float bestCost = -1.0f, bestVisibleCost = -1.0f;
	vec3_t bestPos = { 0, 0, 0 }, bestVisiblePos = { 0, 0, 0 };

	for ( int i = 0; i < resultCount; i++ ) {
		float pt[3];
		bool overPoly;
		if ( dtStatusFailed( data->query->closestPointOnPoly( resultRefs[i], startNearest, pt, &overPoly ) ) ) {
			continue;
		}

		vec3_t toTarget;
		VectorSubtract( pt, navTarget, toTarget );
		float range = VectorLength( toTarget );
		if ( range < minRange || range > maxRange ) {
			continue;
		}

		vec3_t toStart;
		VectorSubtract( pt, navFrom, toStart );
		float travelCost = VectorLength( toStart );
		if ( bestCost < 0.0f || travelCost < bestCost ) {
			bestCost = travelCost;
			VectorCopy( pt, bestPos );
			found = true;
		}

		if ( ( bestVisibleCost < 0.0f || travelCost < bestVisibleCost ) &&
			 Nav_RaycastClear( data, targetRef, targetNearest, pt ) ) {
			bestVisibleCost = travelCost;
			VectorCopy( pt, bestVisiblePos );
			foundVisible = true;
		}
	}

	if ( foundVisible ) {
		SwapYZ( bestVisiblePos, outPos );
		return 1;
	}
	if ( found ) {
		SwapYZ( bestPos, outPos );
	}
	return found ? 1 : 0;
}

/*
=======================
Nav_GetRouteFirstVisPos
=======================
*/
int Nav_GetRouteFirstVisPos( const float *srcpos, const float *destpos, float *outPos ) {
	NavData_t *data = Nav_CurrentData();
	if ( !data ) {
		return 0;
	}
	vec3_t navSrc, navDest;
	SwapYZ( srcpos, navSrc );
	SwapYZ( destpos, navDest );

	dtPolyRef srcRef, destRef;
	float srcNearest[3], destNearest[3];
	if ( !Nav_FindNearest( data, navSrc, &srcRef, srcNearest ) ) {
		return 0;
	}
	if ( !Nav_FindNearest( data, navDest, &destRef, destNearest ) ) {
		return 0;
	}

	// already visible directly from dest? just use src as-is.
	if ( Nav_RaycastClear( data, destRef, destNearest, srcNearest ) ) {
		SwapYZ( srcpos, outPos );
		return 1;
	}

	float straight[32 * 3];
	unsigned char straightFlags[32];
	dtPolyRef straightRefs[32];
	int straightCount = 0;
	if ( !Nav_ComputeStraightPath( data, navSrc, navDest, straight, straightFlags, straightRefs, 32, &straightCount ) ) {
		return 0;
	}

	// first corridor point (walking from src) that dest can see wins.
	for ( int i = 0; i < straightCount; i++ ) {
		if ( Nav_RaycastClear( data, destRef, destNearest, &straight[i * 3] ) ) {
			SwapYZ( &straight[i * 3], outPos );
			return 1;
		}
	}
	return 0;
}

/*
============
Nav_TestPath
============
*/
void Nav_TestPath( const float *start, const float *end ) {
	if ( !Nav_CurrentData() ) {
		Com_Printf( "Nav_TestPath: no navmesh loaded for class %d\n", navCurrentClass );
		return;
	}

	navMoveResult_t result;
	int ok = Nav_MoveToGoal( &result, start, end );
	int dist = Nav_TravelTimeEstimate( start, end );

	if ( !ok ) {
		Com_Printf( "Nav_TestPath: FAILED start (%.0f %.0f %.0f) -> end (%.0f %.0f %.0f)\n",
					start[0], start[1], start[2], end[0], end[1], end[2] );
		return;
	}

	Com_Printf( "Nav_TestPath: OK dist=%d movedir=(%.2f %.2f %.2f)\n",
				dist, result.movedir[0], result.movedir[1], result.movedir[2] );
}

/*
============
Nav_DumpMesh

Writes classIndex's baked navmesh polygons (not navgen's raw input geometry)
to an .obj in quake space, for visual inspection in any 3D viewer.
============
*/
void Nav_DumpMesh( int classIndex ) {
	if ( classIndex < 0 || classIndex >= NAV_MAX_CLASSES ) {
		Com_Printf( "Nav_DumpMesh: invalid class %d\n", classIndex );
		return;
	}

	NavData_t *data = &navData[classIndex];
	if ( !data->loaded || !data->mesh ) {
		Com_Printf( "Nav_DumpMesh: no navmesh loaded for class %d\n", classIndex );
		return;
	}

	char filename[256];
	snprintf( filename, sizeof( filename ), "navdump_%s.obj", navGenClasses[classIndex].name );
	FILE *f = fopen( filename, "w" );
	if ( !f ) {
		Com_Printf( "Nav_DumpMesh: could not open %s for writing\n", filename );
		return;
	}

	const dtNavMesh *mesh = data->mesh;
	int vertBase = 1; // OBJ indices are 1-based
	int totalPolys = 0, totalVerts = 0;

	for ( int i = 0; i < mesh->getMaxTiles(); i++ ) {
		const dtMeshTile *tile = mesh->getTile( i );
		if ( !tile || !tile->header ) {
			continue;
		}

		for ( int j = 0; j < tile->header->vertCount; j++ ) {
			const float *v = &tile->verts[j * 3];
			vec3_t quakeV;
			SwapYZ( v, quakeV ); // navmesh is Y-up; write back out in quake's Z-up space
			fprintf( f, "v %f %f %f\n", quakeV[0], quakeV[1], quakeV[2] );
		}

		for ( int j = 0; j < tile->header->polyCount; j++ ) {
			const dtPoly *poly = &tile->polys[j];
			if ( poly->getType() != DT_POLYTYPE_GROUND ) {
				continue;
			}
			fprintf( f, "f" );
			for ( int k = 0; k < poly->vertCount; k++ ) {
				fprintf( f, " %d", vertBase + poly->verts[k] );
			}
			fprintf( f, "\n" );
			totalPolys++;
		}

		vertBase += tile->header->vertCount;
		totalVerts += tile->header->vertCount;
	}

	fclose( f );
	Com_Printf( "Nav_DumpMesh: wrote %s (%d verts, %d polys)\n", filename, totalVerts, totalPolys );
}
